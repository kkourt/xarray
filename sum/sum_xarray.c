#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#ifdef NO_CILK
#define cilk_spawn
#define cilk_sync
#else
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#define YES_CILK
#endif

#include "tsc.h"
#include "xarray.h"
#include "sum_op.h"
#include "sum_stats.h"
DECLARE_SUM_STATS

// parameters
static unsigned long sum_rec_limit   = 256;
static unsigned long xarr_grain      = 64;
static unsigned long sla_max_level   = 5;
static float         sla_p = 0.5;

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

	set_paraml("SUM_REC_LIMIT",   sum_rec_limit);
	set_paraml("XARR_GRAIN",      xarr_grain);
	set_paraml("SLA_MAX_LEVEL",   sla_max_level);
	set_paramf("SLA_P",           sla_p);

	#undef set_paraml
	#undef set_paramf
}

static xarray_t *
create_xarr_int(void)
{
	xarray_t *ret = xarray_create(&(struct xarray_init) {
		.elem_size = sizeof(int),
		.da = {
			.elems_alloc_grain = xarr_grain,
			.elems_init = xarr_grain,
		},
		.sla = {
			.p                =  sla_p,
			.max_level        =  sla_max_level,
			.elems_chunk_size =  xarr_grain,
		},
		.rpa = {
			.elems_alloc_grain = xarr_grain,
		}
	});
	assert(xarray_elem_size(ret) == sizeof(int));
	return ret;
}


// 0..99 , return sum
size_t
xarr_int_mkrand(xarray_t *xarr, size_t nints)
{
	assert(xarr);
	assert(xarray_elem_size(xarr) == sizeof(int));
	assert(xarray_size(xarr) == 0);
	size_t ret = 0;

	#if 0
	for (size_t i=0; i<nints; i++) {
		int *x_ptr;
		x_ptr   = xarray_append(xarr);
		*x_ptr  = (int)i;
		ret   += sum_op(*x_ptr);
	}
	#else
	for (size_t i=0; i<nints; i++) {
		int *x_ptr;
		x_ptr   = xarray_append(xarr);
		*x_ptr  = rand() % 100;
		ret   += sum_op(*x_ptr);
	}
	#endif

	return ret;
}

int
sum_seq(xslice_t *ints)
{
	int *ch;
	size_t ch_len;
	int ret = 0;

	SUM_TIMER_START(sum_seq);
	while (1) {
		ch = xslice_getnextchunk(ints, &ch_len);
		if (ch_len == 0)
			break;

		//printf("SUMMING from %d to %d\n", ch[0], ch[ch_len-1]);
		for (size_t i=0; i<ch_len; i++)
			ret += sum_op(ch[i]);
	}
	SUM_TIMER_PAUSE(sum_seq);

	return ret;
}

int
sum_rec(xslice_t *ints)
{

	assert(xslice_size(ints) > 0);
        #if defined(YES_CILK)
        int __attribute__((unused)) myid = __cilkrts_get_worker_number();
        #else
        int __attribute__((unused)) myid = 0;
        #endif
	mysum_stats_set(myid);

	//printf("SIZE=%zd\n", xslice_size(ints));
	/* unitary solution */
	if (xslice_size(ints) <= sum_rec_limit) {
		return sum_seq(ints);
	}

	xslice_t s1, s2;
	SUM_TIMER_START(sum_split);
	xslice_split(ints, &s1, &s2);
	SUM_TIMER_PAUSE(sum_split);
	int ret1 = cilk_spawn sum_rec(&s1);
	int ret2 = cilk_spawn sum_rec(&s2);
	cilk_sync;

	return ret1 + ret2;
}

int
main(int argc, const char *argv[])
{
	unsigned nthreads;
	size_t nints;
	xarray_t __attribute__((unused)) *ints;
	xslice_t ints_sl;

	#if defined(YES_CILK)
	nthreads = __cilkrts_get_nworkers();
	#else
	nthreads = 1;
	#endif

	sum_stats_create(nthreads);
	sum_stats_init(nthreads);

	set_params();

	ints = create_xarr_int();

	nints = 0;
	if (argc > 1)
		nints = atol(argv[1]);
	if (nints == 0)
		nints = 100000;

	srand(time(NULL));
	printf("xarray impl:       %s\n",  XARRAY_IMPL);
	printf("number of ints:    %lu\n", nints);
	printf("sum_rec_limit:     %lu\n", sum_rec_limit);
	printf("xarr_grain:        %lu\n", xarr_grain);
	printf("sla_max_level:     %lu\n", sla_max_level);
	printf("SLA_MAX_LEVEL:     %u\n",  SLA_MAX_LEVEL);
	printf("sla_p:             %lf\n", sla_p);
	printf("Number of threads: %u\n", nthreads);

	int sum1, sum2, sum3;

	sum1 = xarr_int_mkrand(ints, nints);

	sum_stats_init(nthreads);

	xslice_init(ints, 0, xarray_size(ints), &ints_sl);

	#if 1
	mysum_stats_set(0);
	TSC_MEASURE_TICKS(xticks_seq, {
		sum3 = sum_seq(&ints_sl);
	});
	tsc_report_ticks("sum_seq(serial)", xticks_seq);
	sum_stats_init(nthreads);
	#endif

	//printf("DOING SUM\n");
	xslice_init(ints, 0, xarray_size(ints), &ints_sl);
	tsc_t ytsc_; tsc_init(&ytsc_); tsc_start(&ytsc_);
	// TSC_MEASURE_TICKS(xticks, {
		sum2 = cilk_spawn sum_rec(&ints_sl);
		cilk_sync;
	//});
	tsc_pause(&ytsc_); uint64_t xticks = tsc_getticks(&ytsc_);

	tsc_report_ticks("sum_rec_ALL", xticks);
	sum_stats_report(nthreads, xticks);

	if (sum1 != sum2) {
		fprintf(stderr, "Error in sum2: correct=%d vs xarray=%d\n", sum1, sum2);
		abort();
	}

	if (sum1 != sum3) {
		fprintf(stderr, "Error in sum3: correct=%d vs xarray=%d\n", sum1, sum3);
		abort();
	}

	printf("DONE\n");
	return 0;
}
