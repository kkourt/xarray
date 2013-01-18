#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "verp.h"

struct verarray {
	verobj_t vo;
	unsigned int **ptrs;
	size_t block_size;
	size_t blocks_nr;
};
typedef struct verarray verarray_t;

static void
xptr_dealloc(void *ptr)
{
}

static void  *
xmalloc(size_t s)
{
	void *ret;
	if ((ret = malloc(s)) == NULL) {
		perror("malloc");
		exit(1);
	}
	return ret;
}

verarray_t *
verarray_init(size_t block_size, size_t blocks_nr)
{
	verarray_t *ret;
	ret = xmalloc(sizeof(verarray_t));
	ret->block_size = block_size;
	ret->blocks_nr = blocks_nr;
	ret->ptrs = xmalloc(blocks_nr*sizeof(unsigned int *));
	for (unsigned int i=0; i<blocks_nr; i++) {
		ret->ptrs[i] = xmalloc(block_size);
	}
	return ret;
}

ver_t *
verarray_newver(verarray_t *verarray, ver_t *p)
{
	return ver_alloc(p);
}

void
verarray_set(verarray_t *verarray, unsigned long key, unsigned long val, ver_t *v)
{
}

unsigned long
verarray_get(verarray_t *verarray, unsigned long key, ver_t *v)
{
}

#ifdef VERARRAY_TEST

#include "tsc.h"

int
main(int argc, const char *argv[])
{
	if (argc < 4) {
		fprintf(stderr,
		        "Usage: %s <array_size> <block_size> <accesses>\n",
			argv[0]);
		exit(1);
	}

	unsigned int asize = atol(argv[1]);
	unsigned int bsize = atol(argv[2]);
	unsigned int accesses = atol(argv[3]);
	unsigned int seed = time(NULL);

	tsc_t tc;
	/* normal pointers */
	srand(seed);
	printf("CoPy\n");
	unsigned int *p, *p_copy;
	unsigned int sum_copy = 0;
	p = xmalloc(asize*sizeof(unsigned int));
	for (unsigned int i=0; i<asize; i++)
		p[i] = i;
	tsc_init(&tc);
	tsc_start(&tc);
	p_copy = xmalloc(asize*sizeof(unsigned int));
	memcpy(p_copy, p, asize*sizeof(unsigned int));
	for (unsigned int j=0; j<accesses; j++) {
		unsigned int idx = rand() % asize;
		p_copy[idx] = 0;
	}
	#ifdef DO_SUMS
	for (unsigned int j=0; j<asize; j++) {
		sum_copy += p_copy[j];
	}
	#endif
	tsc_pause(&tc);
	tsc_report(&tc);

	/* versioned pointers */
	tsc_t t;
	srand(seed);
	printf("VerSions\n");
	unsigned int sum_versions = 0;
	sla_t *sla = sla_init(10, .5);
	sla->def_nitems = bsize;
	for (unsigned int i=0; i<asize; i++)
		sla_append(sla, i);
	tsc_init(&t);
	tsc_start(&t);
	verarray_t *verarray = verarray_init(sla);
	ver_t *v1 = verarray_newver(verarray, verarray->vo.ver_base);
	for (unsigned int j=0; j<accesses; j++) {
		unsigned int idx = rand() % asize;
		verarray_set(verarray, idx, 0, v1);
	}
	#ifdef DO_SUMS
	for (unsigned int j=0; j<asize; j++) {
		unsigned int x = verarray_get(verarray, j, v1);
		sum_versions += x;
	}
	#endif
	tsc_pause(&t);
	tsc_report(&t);

	printf("\ntC/tV=%lf\n", (double)tsc_getticks(&tc)/(double)tsc_getticks(&t));
	for (unsigned int j=0; j<asize; j++) {
		unsigned int x0 = p_copy[j];
		unsigned int x1 = verarray_get(verarray, j, v1);
		if (x0 != x1) {
			fprintf(stderr, "copy:%d and versions:%d differ for j=%d\n", x0, x1, j);
		}
	}
	assert(sum_versions == sum_copy);
	return 0;
}
#endif
