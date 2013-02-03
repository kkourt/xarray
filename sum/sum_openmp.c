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

#include <omp.h>

#include "misc.h"
#include "tsc.h"

int *
arr_int_mkrand(size_t nints, int *sum_ptr)
{
	int sum = 0;
	int *ret;

	ret = xmalloc(sizeof(int)*nints);

	for (size_t i=0; i<nints; i++) {
		ret[i] = rand() % 100;
		sum += ret[i];
	}

	*sum_ptr = sum;
	return ret;
}

int
main(int argc, const char *argv[])
{
	unsigned nthreads;
	size_t nints;
	int sum1, sum2;
	int *arr;

	nints = 0;
	if (argc > 1)
		nints = atol(argv[1]);
	if (nints == 0)
		nints = 100000;

	#pragma omp parallel
	#pragma omp master
	nthreads = omp_get_num_threads();

	printf("Number of threads: %u\n", nthreads);
	printf("number of ints:    %lu\n", nints);
	arr = arr_int_mkrand(nints, &sum1);

	sum2 = 0;
	tsc_t t; tsc_init(&t); tsc_start(&t);
	#pragma omp parallel for reduction(+:sum2)
	for (size_t i=0; i<nints; i++) {
		sum2 += arr[i];
	}
	tsc_pause(&t);

	tsc_report("sum_OMP", &t);

	if (sum1 != sum2) {
		fprintf(stderr, "Error in sum: %d vs %d\n", sum1, sum2);
		abort();
	}

	printf("DONE\n");
	return 0;
}
