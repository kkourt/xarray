/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "sla.h"
#include "verp.h"

struct versla {
	verobj_t vo;
	sla_t    *sla;
};
typedef struct versla versla_t;

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

versla_t *
versla_init(sla_t *sla)
{
	versla_t *ret;
	ret = xmalloc(sizeof(versla_t));
	ret->sla = sla;
	ret->vo.ver_base = ver_alloc(NULL);
	ret->vo.ptr_dealloc = xptr_dealloc;
	return ret;
}

ver_t *
versla_newver(versla_t *versla, ver_t *p)
{
	return ver_alloc(p);
}

/* dummy, because we don't support versioning on link pointers */
sla_node_t *
versla_find(versla_t *versla, unsigned long key, unsigned long *off, ver_t *v)
{
	assert(v == NULL && "NYI");
	return sla_find(versla->sla, key, off);
}

void
versla_set(versla_t *versla, unsigned long key, unsigned long val, ver_t *v)
{
	unsigned long off;
	sla_node_t *n = sla_find(versla->sla, key, &off);
	verp_t *verp;

	if (vp_is_normal_ptr(n->items)) {
		if (v == versla->vo.ver_base) { /* special case */
			n->items[off] = val;
			return;
		}
		verp = verp_allocate(5);
		/* insert normal pointer as base version */
		verp_update_ptr(verp, versla->vo.ver_base, n->items);
		n->items = verp_mark_ptr(verp);
	} else {
		verp = vp_unmark_ptr(n->items);
		verp_gc(verp, versla->vo.ver_base);
	}

	ver_t *cver; /* current version */
	unsigned long *items = verp_find_ptr(verp, v, &cver);
	if (cver != v) {
		size_t s = n->items_size*sizeof(unsigned long);
		unsigned long *nitems = xmalloc(s);
		memcpy(nitems, items, s);
		verp_update_ptr(verp, v, nitems);
		items = nitems;
	}

	items[off] = val;
}

unsigned long
versla_get(versla_t *versla, unsigned long key, ver_t *v)
{
	unsigned long off;
	sla_node_t *n = sla_find(versla->sla, key, &off);

	unsigned long *items = vp_ptr(n->items, v);
	return items[off];
}

#ifdef VERSLA_TEST

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
	sla_t *sla = sla_init(10, .5, 16, time(NULL));
	sla->def_nitems = bsize;
	for (unsigned int i=0; i<asize; i++)
		sla_append(sla, i);
	tsc_init(&t);
	tsc_start(&t);
	versla_t *versla = versla_init(sla);
	ver_t *v1 = versla_newver(versla, versla->vo.ver_base);
	for (unsigned int j=0; j<accesses; j++) {
		unsigned int idx = rand() % asize;
		versla_set(versla, idx, 0, v1);
	}
	#ifdef DO_SUMS
	for (unsigned int j=0; j<asize; j++) {
		unsigned int x = versla_get(versla, j, v1);
		sum_versions += x;
	}
	#endif
	tsc_pause(&t);
	tsc_report(&t);

	printf("\ntC/tV=%lf\n", (double)tsc_getticks(&tc)/(double)tsc_getticks(&t));
	for (unsigned int j=0; j<asize; j++) {
		unsigned int x0 = p_copy[j];
		unsigned int x1 = versla_get(versla, j, v1);
		if (x0 != x1) {
			fprintf(stderr, "copy:%d and versions:%d differ for j=%d\n", x0, x1, j);
		}
	}
	assert(sum_versions == sum_copy);
	return 0;
}
#endif
