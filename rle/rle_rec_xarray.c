/**
 * rle_rec_xarray.c
 * run-length ecndoing benchmark where an xarray is used for the input and
 * output
 */

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

// parameters (with default values)
static unsigned long rle_rec_limit   = 256;
static unsigned long xarr_rle_grain  = 32;
static unsigned long xarr_syms_grain = 512;
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

void
rle_mkrand_inc(xarray_t *xarr, size_t rle_nr, size_t *syms_nr)
{
	assert(xarr);
	assert(xarray_elem_size(xarr) == sizeof(struct rle_node));
	assert(xarray_size(xarr) == 0);

	size_t syms_cnt = 0;
	const int max_freq_max = 100;
	const int max_freq_min = 2;
	const int freq_inc_nr = rle_nr / (max_freq_max - max_freq_min);
	const int min_freq = 1;

	for (size_t i=0; i<rle_nr; i++) {
		struct rle_node *rle = xarray_append(xarr);
		int max_freq = max_freq_min + i/freq_inc_nr;
		//printf("min_freq: %d max_freq:%d\n", min_freq, max_freq);
		rle->symbol = 'a' + (i % 26);
		rle->freq = min_freq + (rand() % (max_freq - min_freq +1));
		syms_cnt += rle->freq;
	}

	if (syms_nr != NULL)
		*syms_nr = syms_cnt;
}

void
rle_mkrand_imba(xarray_t *xarr, size_t rle_nr, size_t *syms_nr)
{
	assert(xarr);
	assert(xarray_elem_size(xarr) == sizeof(struct rle_node));
	assert(xarray_size(xarr) == 0);

	size_t syms_cnt = 0;
	const int max_freq_max = 100;
	const int max_freq_min = 2;
	const int min_freq = 1;

	for (size_t i=0; i<rle_nr; i++) {
		struct rle_node *rle = xarray_append(xarr);
		int max_freq = (i > rle_nr / 2) ? max_freq_max : max_freq_min;
		//printf("min_freq: %d max_freq:%d\n", min_freq, max_freq);
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
			// NB: for DA we preallocate the array for all symbols,
			// which makes it much faster than the other
			// implementations
			.elems_init = syms_nr
		},
		.sla = {
			.p                =  sla_p,
			.max_level        =  sla_max_level,
			.elems_chunk_size =  xarr_syms_grain,
		},
		.rpa = {
			.elems_alloc_grain = xarr_syms_grain
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

	struct rle_node *rles_ch;
	size_t rles_ch_len=0, rles_ch_idx=0;
	struct rle_node *get_rle(void) {
		struct rle_node *rle_ret;
		static size_t rles_idx = 0;
		if (rles_ch_idx >= rles_ch_len) {
			rles_idx += rles_ch_len;
			assert(rles_ch_len == rles_ch_idx);
			rles_ch = xarray_getchunk(rle, rles_idx, &rles_ch_len);
			rles_ch_idx = 0;
		}
		rle_ret = rles_ch + rles_ch_idx;
		rles_ch_idx++;
		return rle_ret;
	}

	size_t xarr_size = xarray_size(rle);
	syms_ch = xarray_append_prepare(ret, &syms_ch_len);
	for (size_t x=0; x<xarr_size; x++) {
		struct rle_node *n = get_rle();
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
		},
		.rpa = {
			.elems_alloc_grain = xarr_rle_grain
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
		//RLE_TIMER_START(append_rle, rle_getmyid());
		if (rles_ch_idx >= rles_ch_len) {
			assert(rles_ch_idx == rles_ch_len);
			xarray_append_finalize(rles, rles_ch_idx);
			rles_ch = xarray_append_prepare(rles, &rles_ch_len);
			rles_ch_idx = 0;
		}
		rles_ch[rles_ch_idx].symbol = symbol;
		rles_ch[rles_ch_idx].freq = frq;
		rles_ch_idx++;
		//RLE_TIMER_PAUSE(append_rle, rle_getmyid());
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

// TODO: parallel version
// returns true if rles match
bool
rle_cmp(xarray_t *rle1, xarray_t *rle2)
{
	size_t rle1_size = xarray_size(rle1);
	size_t rle2_size = xarray_size(rle2);
	bool ret = true;
	if (rle1_size != rle2_size) {
		printf("size mismatch: rle1:%lu rle2:%lu\n",
		       rle1_size, rle2_size);
		ret = false;
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

// return true of rles match
bool
rle_cmp_fast(xarray_t *rle1, xarray_t *rle2)
{
	size_t rle1_size = xarray_size(rle1);
	size_t rle2_size = xarray_size(rle2);
	bool ret = true;
	if (rle1_size != rle2_size) {
		printf("size mismatch: rle1:%lu rle2:%lu\n",
		       rle1_size, rle2_size);
		ret = false;
	}


	xslice_t r1_slice, r2_slice;
	xslice_init(rle1, 0, rle1_size, &r1_slice);
	xslice_init(rle2, 0, rle2_size, &r2_slice);

	struct rle_node *r1_chunk, *r2_chunk;
	size_t r1_chunk_idx, r2_chunk_idx; // position on the chunk
	size_t r1_chunk_len, r2_chunk_len; // remaining chunk length
	size_t total_idx = 0; // for printing the proper index

	r1_chunk = xslice_getnextchunk(&r1_slice, &r1_chunk_len);
	r2_chunk = xslice_getnextchunk(&r2_slice, &r2_chunk_len);
	r1_chunk_idx = r2_chunk_idx = 0;

	bool r1_done = false, r2_done = false;
	while (!r1_done || !r2_done) {
		struct rle_node *r1 = r1_chunk + r1_chunk_idx;
		struct rle_node *r2 = r2_chunk + r2_chunk_idx;
		size_t r_len = MIN(r1_chunk_len, r2_chunk_len);
		for (size_t i=0; i<r_len; i++) {
			if (r1->symbol != r2->symbol) {
				printf("symbol mismatch on rle %lu  "
				       "r1->symbol=%c r2->symbol=%c\n",
				       total_idx + i,
				       r1->symbol, r2->symbol);
				return false;
			}

			if (r1->freq != r2->freq) {
				printf("freq mismatch on rle %lu  "
				       "r1->symbol=%c r2->symbol=%c "
				       "r1->freq=%lu r2->freq=%lu\n",
				       total_idx + i,
				       r1->symbol, r2->symbol, r1->freq, r2->freq);
				return false;
			}
			r1++; r2++;
		}
		total_idx += r_len;

		if (r1_chunk_len == r_len) {
			r1_chunk = xslice_getnextchunk(&r1_slice, &r1_chunk_len);
			r1_chunk_idx = 0;
			r1_done = r1_chunk_len == 0;
		} else {
			assert(r_len < r1_chunk_len);
			r1_chunk_idx += r_len;
			r1_chunk_len -= r_len;
		}

		if (r2_chunk_len == r_len) {
			r2_chunk = xslice_getnextchunk(&r2_slice, &r2_chunk_len);
			r2_chunk_idx = 0;
			r2_done = r2_chunk_len == 0;
		} else {
			assert(r_len < r2_chunk_len);
			r2_chunk_idx += r_len;
			r2_chunk_len -= r_len;
		}
	}

	return ret;
}

static void
set_params(void)
{
	long x;
	char *x_str;

	#define set_paraml(env_str, param) \
	do { \
		if ( (x_str = getenv(env_str)) != NULL) { \
			x = atol(x_str); \
			if (x != 0) \
				param = x; \
		} \
	} while (0)

	#define set_paramf(env_str, param) \
	do { \
		if ( (x_str = getenv(env_str)) != NULL) { \
			x = atof(x_str); \
			if (x != 0) \
				param = x; \
		} \
	} while (0)

	set_paraml("RLE_REC_LIMIT",   rle_rec_limit);
	set_paraml("XARR_RLE_GRAIN",  xarr_rle_grain);
	set_paraml("XARR_SYMS_GRAIN", xarr_syms_grain);
	set_paraml("SLA_MAX_LEVEL",   sla_max_level);
	set_paramf("SLA_P",           sla_p);

	#undef set_paraml
	#undef set_paramf
}

int
main(int argc, const char *argv[])
{
	unsigned long syms_nr, rles_nr;

	set_params();

	unsigned nthreads = 1;
	#if defined(YES_CILK)
	nthreads = __cilkrts_get_nworkers();
	#endif
	rle_stats_create(nthreads);

	xarray_t __attribute__((unused)) *rle, *rle_rec, *rle_new;
	rle = rle_create_xarr();
	rle_rec = rle_create_xarr();


	rles_nr = 0;
	if (argc > 1)
		rles_nr = atol(argv[1]);
	if (rles_nr == 0)
		rles_nr = 100000;

	srand(time(NULL));


	rle_stats_init(nthreads);

	TSC_REPORT_TICKS("rle_mkrand",{
		rle_mkrand(rle, rles_nr, &syms_nr);
	});

	TSC_REPORT_TICKS("rebalance rle_mkrand", {
		//xarray_rebalance(rle);
	});

	printf("xarray impl: %s\n",        XARRAY_IMPL);
	printf("number of rles:    %lu\n", rles_nr);
	printf("number of symbols: %lu\n", syms_nr);
	printf("rle_rec_limit:     %lu\n", rle_rec_limit);
	printf("xarr_rle_grain:    %lu\n", xarr_rle_grain);
	printf("xarr_syms_grain:   %lu\n", xarr_syms_grain);
	printf("sla_max_level:     %lu\n", sla_max_level);
	printf("sla_p:             %lf\n", sla_p);
	printf("Number of threads: %u\n", nthreads);


	rle_stats_init(nthreads);

	xarray_t *syms;
	TSC_REPORT_TICKS("rle_decode",{
		syms = rle_decode(rle, syms_nr);
	});

	TSC_REPORT_TICKS("rebalance syms", {
		//xarray_rebalance(syms);
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
	TSC_REPORT_TICKS("rle_cmp", {
		if (!rle_cmp(rle, rle_new)) {
			fprintf(stderr, "RLEs do not match\n");
			exit(1);
		}
	});
	#endif
	//rle_stats_report(nthreads, tsc_getticks(&total_ticks));

	xslice_init(syms, 0, xarray_size(syms), &syms_sl);
	rle_stats_init(nthreads);

	tsc_t ytsc_; tsc_init(&ytsc_); tsc_start(&ytsc_);
	// TSC_MEASURE_TICKS(xticks, {
		rle_rec = cilk_spawn rle_encode_rec(&syms_sl);
		cilk_sync;
	//});
	tsc_pause(&ytsc_); uint64_t xticks = tsc_getticks(&ytsc_);

	tsc_report_ticks("rle_encode_rec", xticks);
	rle_stats_report(nthreads, xticks);
	rle_stats_destroy();

	printf("rle_rec balanced? %d\n", xarray_is_balanced(rle_rec));
	TSC_REPORT_TICKS("rebalance rle_rec", {
		//xarray_rebalance(rle_rec);
	});

	//rle_print(rle_rec);
	#if !defined(NDEBUG)
	TSC_REPORT_TICKS("rle_cmp", {
		if (!rle_cmp(rle, rle_rec)) {
			fprintf(stderr, "RLEs do not match\n");
			exit(1);
		}
	});
	#endif

	printf("DONE\n");
	return 0;
}

// let b:syntastic_c_cflags="-I../xarray -std=gnu11 -DXARRAY_DA__ -DNO_CILK"
