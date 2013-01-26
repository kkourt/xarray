#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#ifdef NO_CILK
#define cilk_spawn
#define cilk_sync
#else
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#define YES_CILK
#endif

#include "rle_rec_stats.h"
DECLARE_RLE_STATS

unsigned
rle_getmyid(void)
{
	unsigned ret = 0;
	#if defined(YES_CILK)
	ret = __cilkrts_get_worker_number();
	#endif
	return ret;
}


#include <xarray.h>

// parameters
static unsigned long rle_rec_limit   = 256;
static unsigned long xarr_rle_grain  = 32;
static unsigned long xarr_syms_grain = 256;
static unsigned long sla_max_level = 5;
static float         sla_p = 0.5;


struct rle_node {
	char   symbol;
	size_t freq;
};

static inline void
xarray_append_rle(xarray_t *xarr, char symbol, size_t freq)
{
	assert(xarray_elem_size(xarr) == sizeof(struct rle_node));
	struct rle_node *n = xarray_append(xarr);
	n->symbol = symbol;
	n->freq = freq;
}

void
rle_print(xarray_t *rles)
{
	size_t idx=0;
	size_t rles_size = xarray_size(rles);
	printf("RLEs: %p [size:%lu]\n", rles, rles_size);
	while (idx < rles_size) {
		size_t chunk_size;
		const struct rle_node *r;
		r = xarray_getchunk(rles, idx, &chunk_size);
		for (size_t i=0; i<chunk_size; i++) {
			printf("i=%lu sym:%c freq:%lu\n", i, r[i].symbol, r[i].freq);
		}
		idx += chunk_size;
	}
	printf("\n");
}


void
rle_mkrand(xarray_t *xarr, size_t rle_nr, size_t *syms_nr)
{
	assert(xarr);
	assert(xarray_elem_size(xarr) == sizeof(struct rle_node));
	assert(xarray_size(xarr) == 0);

	size_t syms_cnt = 0;
	const int max_freq = 100;
	const int min_freq = 1;

	for (size_t i=0; i<rle_nr; i++) {
		struct rle_node *rle = xarray_append(xarr);
		rle->symbol = 'a' + (i % 26);
		rle->freq = min_freq + (rand() % (max_freq - min_freq +1));
		syms_cnt += rle->freq;
	}

	if (syms_nr != NULL)
		*syms_nr = syms_cnt;
}

xarray_t *
rle_decode(xarray_t *rle, unsigned long syms_nr)
{
	xarray_t *ret = xarray_create(&(struct xarray_init){
		.elem_size = sizeof(char),
		.da = {
			.elems_alloc_grain = xarr_syms_grain,
			.elems_init = syms_nr
		},
		.sla = {
			.p                =  sla_p,
			.max_level        =  sla_max_level,
			.elems_chunk_size =  xarr_syms_grain,
		}
	});

	char *syms_ch;
	size_t syms_ch_len, syms_ch_idx=0;
	void append_sym(char symbol) {
		if (syms_ch_idx >= syms_ch_len) {
			assert(syms_ch_idx == syms_ch_len);
			xarray_append_finalize(ret, syms_ch_idx);
			syms_ch = xarray_append_prepare(ret, &syms_ch_len);
			syms_ch_idx = 0;
		}
		syms_ch[syms_ch_idx] = symbol;
		syms_ch_idx++;
	}

	size_t xarr_size = xarray_size(rle);
	syms_ch = xarray_append_prepare(ret, &syms_ch_len);
	for (size_t x=0; x<xarr_size; x++) {
		struct rle_node *n = xarray_get(rle, x);
		for (size_t i=0; i<n->freq; i++) {
			append_sym(n->symbol);
		}
	}
	xarray_append_finalize(ret, syms_ch_idx);

	assert(xarray_size(ret) == syms_nr);
	return ret;
}

static xarray_t *
rle_create_xarr(void)
{
	xarray_t *ret = xarray_create(&(struct xarray_init){
		.elem_size = sizeof(struct rle_node),
		.da = {
			.elems_alloc_grain = xarr_rle_grain,
			.elems_init = xarr_rle_grain,
		},
		.sla = {
			.p                =  sla_p,
			.max_level        =  sla_max_level,
			.elems_chunk_size =  xarr_rle_grain,
		}
	});
	assert(xarray_elem_size(ret) == sizeof(struct rle_node));
	return ret;
}

xarray_t *
rle_encode(xslice_t *syms)
{
	RLE_TIMER_START(rle_encode, rle_getmyid());
	char prev, curr;
	xarray_t *rles;

	RLE_TIMER_START(rle_alloc, rle_getmyid());
	rles = rle_create_xarr();
	RLE_TIMER_PAUSE(rle_alloc, rle_getmyid());

	prev = *((char *)xslice_getnext(syms));
	size_t freq = 1;

	const char *syms_ch;             // symbols chunk
	struct rle_node *rles_ch;        // rles chunk
	size_t syms_ch_len, rles_ch_len; // chunk sizes
	size_t rles_ch_idx=0;            // rles chunk size

	void append_rle(char symbol, size_t frq) {
		if (rles_ch_idx >= rles_ch_len) {
			assert(rles_ch_idx == rles_ch_len);
			xarray_append_finalize(rles, rles_ch_idx);
			rles_ch = xarray_append_prepare(rles, &rles_ch_len);
			rles_ch_idx = 0;
		}
		rles_ch[rles_ch_idx].symbol = symbol;
		rles_ch[rles_ch_idx].freq = frq;
		rles_ch_idx++;
	}


	RLE_TIMER_START(rle_encode_loop, rle_getmyid());
	rles_ch = xarray_append_prepare(rles, &rles_ch_len);
	while (1) {
		syms_ch = xslice_getnextchunk(syms, &syms_ch_len);
		if (syms_ch_len == 0)
			break;

		for (size_t i=0; i<syms_ch_len; i++) {
			curr = syms_ch[i];
			if (curr == prev) {
				freq++;
			} else  {
				append_rle(prev, freq);
				prev = curr;
				freq = 1;
			}
		}
	}
	append_rle(prev, freq);
	xarray_append_finalize(rles, rles_ch_idx);
	RLE_TIMER_PAUSE(rle_encode_loop, rle_getmyid());

	RLE_TIMER_PAUSE(rle_encode, rle_getmyid());
	xarray_verify(rles);
	return rles;
}

xarray_t *
rle_merge(xarray_t *rle1, xarray_t *rle2)
{
	RLE_TIMER_START(rle_merge, rle_getmyid());
	xarray_verify(rle1);
	xarray_verify(rle2);
	assert(rle1 != NULL); assert(xarray_elem_size(rle1) == sizeof(struct rle_node));
	assert(rle2 != NULL); assert(xarray_elem_size(rle2) == sizeof(struct rle_node));

	//printf("MERGING-------------------\n");
	//printf("rle1\n"); rle_print(rle1);
	//printf("rle2\n"); rle_print(rle2);
	struct rle_node *rle1_lst = xarray_getlast(rle1);
	struct rle_node *rle2_fst = xarray_get(rle2,  0);

	if (rle1_lst->symbol == rle2_fst->symbol) {
		rle2_fst->freq += rle1_lst->freq;
		xarray_pop(rle1, 1);
	}

	xarray_t *ret = xarray_concat(rle1, rle2);
	//printf("ret\n"); rle_print(ret);
	//printf("-------------------------\n");;
	RLE_TIMER_PAUSE(rle_merge, rle_getmyid());
	return ret;
}

xarray_t *
rle_encode_rec(xslice_t *syms)
{

	xarray_t *rle1, *rle2, *ret;


	assert(xslice_size(syms) > 0);
	/* unitary solution */
	if (xslice_size(syms) <= rle_rec_limit) {
		ret = rle_encode(syms);
		xarray_verify(ret);
		return ret;
	}

	/* splitting */
	xslice_t s1, s2;
	RLE_TIMER_START(rle_split, rle_getmyid());
	xslice_split(syms, &s1, &s2);
	RLE_TIMER_PAUSE(rle_split, rle_getmyid());

	/*
	printf("----\n");
	printf("syms: idx=%lu len=%lu\n", syms->idx, syms->len);
	printf("s1: idx=%lu len=%lu\n", s1.idx, s1.len);
	printf("s2: idx=%lu len=%lu\n", s2.idx, s2.len);
	printf("----\n");
	*/

	rle1 = cilk_spawn rle_encode_rec(&s1);
	rle2 = cilk_spawn rle_encode_rec(&s2);
	cilk_sync;

	/* combine solutions */
	assert(rle1 != NULL && rle2 != NULL);

	ret = rle_merge(rle1, rle2);

	return ret;
}

int
rle_cmp(xarray_t *rle1, xarray_t *rle2)
{
	size_t rle1_size = xarray_size(rle1);
	size_t rle2_size = xarray_size(rle2);
	int ret = 1;
	if (rle1_size != rle2_size) {
		printf("size mismatch: rle1:%lu rle2:%lu\n",
		       rle1_size, rle2_size);
		ret = 0;
	}

	for (size_t i=0; i<MIN(rle1_size, rle2_size); i++) {
		struct rle_node *r1 = xarray_get(rle1, i);
		struct rle_node *r2 = xarray_get(rle2, i);

		assert(r1 != NULL && r2 != NULL);

		if (r1->symbol != r2->symbol) {
			printf("symbol mismatch on rle %lu  "
			       "r1->symbol=%c r2->symbol=%c\n",
			       i, r1->symbol, r2->symbol);
			return 0;
		}

		if (r1->freq != r2->freq) {
			printf("freq mismatch on rle %lu  "
			       "r1->symbol=%c r2->symbol=%c "
			       "r1->freq=%lu r2->freq=%lu\n",
			       i, r1->symbol, r2->symbol, r1->freq, r2->freq);
			return 0;
		}
	}

	return ret;
}

int
main(int argc, const char *argv[])
{
	unsigned long syms_nr, rles_nr;
	char *rle_rec_limit_str;
	/*
	#ifdef YES_CILK
	Cilk_time tm_begin, tm_elapsed;
	ilk_time wk_begin, wk_elapsed;
	Cilk_time cp_begin, cp_elapsed;
	#endif
	*/

	unsigned nthreads = 1;
	#if defined(YES_CILK)
	nthreads = __cilkrts_get_nworkers();
	#endif
	rle_stats_create(nthreads);

	xarray_t __attribute__((unused)) *rle, *rle_rec, *rle_new;
	rle = rle_create_xarr();
	rle_rec = rle_create_xarr();

	if ( (rle_rec_limit_str = getenv("RLE_REC_LIMIT")) != NULL) {
		rle_rec_limit = atol(rle_rec_limit_str);
		if (rle_rec_limit == 0)
			rle_rec_limit = 64;
	}

	rles_nr = 0;
	if (argc > 1)
		rles_nr = atol(argv[1]);
	if (rles_nr == 0)
		rles_nr = 100000;

	srand(time(NULL));


	rle_stats_init(nthreads);

	TSC_REPORT_TICKS("rle_mkrand", {
		rle_mkrand(rle, rles_nr, &syms_nr);
		//rle_print(rle);
	});

	printf("number of rles:%lu\n", rles_nr);
	printf("number of symbols:%lu\n", syms_nr);
	printf("rle_rec_limit:%lu\n", rle_rec_limit);
	printf("Number of threads:%u\n", nthreads);


	rle_stats_init(nthreads);

	xarray_t *syms;
	TSC_REPORT_TICKS("rle_decode",{
		syms = rle_decode(rle, syms_nr);
	});

	xslice_t syms_sl;
	xslice_init(syms, 0, xarray_size(syms), &syms_sl);

	/*
	 * start RLE
	 */
	TSC_REPORT_TICKS("rle_encode", {
		rle_new = rle_encode(&syms_sl);
	});
	//rle_print(rle_new);


	#if !defined(NDEBUG)
	if (rle_cmp(rle, rle_new) != 1) {
		fprintf(stderr, "RLEs do not match\n");
		exit(1);
	}
	#endif
	//rle_stats_report(nthreads, tsc_getticks(&total_ticks));

	/*
	#ifdef YES_CILK
	cp_begin = Cilk_user_critical_path;
	wk_begin = Cilk_user_work;
	tm_begin = Cilk_get_wall_time();
	#endif
	*/

	xslice_init(syms, 0, xarray_size(syms), &syms_sl);
	rle_stats_init(nthreads);

	TSC_MEASURE_TICKS(xticks, {
		rle_rec = cilk_spawn rle_encode_rec(&syms_sl);
		cilk_sync;
	});

	tsc_report_ticks("rle_encode_rec:", xticks);
	rle_stats_report(nthreads, xticks);
	rle_stats_destroy();

	/*
	#ifdef YES_CILK
	tm_elapsed = Cilk_get_wall_time() - tm_begin;
	wk_elapsed = Cilk_user_work - wk_begin;
	cp_elapsed = Cilk_user_critical_path - cp_begin;
	#endif
	*/

	//rle_print(rle_rec);
	#if !defined(NDEBUG)
	if (rle_cmp(rle, rle_rec) != 1) {
		fprintf(stderr, "RLEs do not match\n");
		exit(1);
	}
	#endif

	/*
	#ifdef YES_CILK
	printf("Running time = %4f s\n", Cilk_wall_time_to_sec(tm_elapsed));
	printf("Work          = %4f s\n", Cilk_time_to_sec(wk_elapsed));
	printf("Span          = %4f s\n\n", Cilk_time_to_sec(cp_elapsed));
	#endif
	*/


	printf("DONE\n");
	return 0;
}
