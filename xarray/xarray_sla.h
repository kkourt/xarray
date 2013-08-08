#ifndef XARRAY_SLA_H__
#define XARRAY_SLA_H__

#include <stddef.h> // size_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(XARRAY_H__) && !defined(XVARRAY_SLA_H)
#error "Don't include this file directly. Include xarray.h"
#endif

#include "sla-chunk.h"
#include "sla-chunk-mm.h"
#include "misc.h"

// This is a quick hack, so that we can just use xslice_t to declare a slice
// statatically. The proper solution is to change the interface, so that app
// does malloc() or alloca()
//#define SLA_MAX_LEVEL 5
// XXX: Actually defined in command line arguments

#define XARRAY_IMPL "SLA"

// @elems_nr is not currently needed, we keep it for doing some assertions. We
// might use it in the future if we want to keep preallocated space on the array
// -- i.e., ->sla.total_size / ->elem_size != ->elems_nr
struct xarray_s {
	sla_t sla;
	size_t elems_nr;
	size_t elem_size;
	size_t elems_chunk_size;
};

static inline void
xarray_verify(xarray_t *xarr)
{
	assert(xarr->sla.total_size % xarr->elem_size == 0);
	assert(xarr->sla.total_size / xarr->elem_size == xarr->elems_nr);
	#if !defined(NDEBUG)
	sla_verify(&xarr->sla);
	#endif
}

void
xarray_init(xarray_t *xarr, struct xarray_init *init)
{
	xarr->elems_nr = 0;
	xarr->elem_size = init->elem_size;
	xarr->elems_chunk_size = init->sla.elems_chunk_size;
	if (init->sla.max_level > SLA_MAX_LEVEL) {
		fprintf(stderr, "SLA_MAX_LEVEL is too small\n");
		abort();
	}
	sla_init(&xarr->sla, init->sla.max_level, init->sla.p);
	xarray_verify(xarr);
}

xarray_t *
xarray_create(struct xarray_init *init)
{
	xarray_t *ret = malloc(sizeof(*ret));
	if (ret == NULL) {
		perror("malloc");
		abort();
	}
	xarray_init(ret, init);
	return ret;
}

static inline size_t
xarray_size(xarray_t *xarr)
{
	xarray_verify(xarr);
	return xarr->elems_nr;
}

xarray_t *
xarray_concat(xarray_t *arr1, xarray_t *arr2)
{
	assert(arr1->elem_size == arr2->elem_size);

	sla_concat(&arr1->sla, &arr2->sla);
	arr1->elems_nr += arr2->elems_nr;

	sla_destroy(&arr2->sla);
	free(arr2);

	xarray_verify(arr1);
	return arr1;
}

void
xarray_split(xarray_t *xa, xarray_t *xa1, xarray_t *xa2)
{
	size_t mid = xa->elems_nr / 2;
	size_t offset = mid*xa->elem_size;

	xa1->elem_size        = xa2->elem_size        = xa->elem_size;
	xa1->elems_chunk_size = xa2->elems_chunk_size = xa->elems_chunk_size;

	sla_split_coarse(&xa->sla, &xa1->sla, &xa2->sla, offset);

	xa1->elems_nr = xa1->sla.total_size / xa1->elem_size;
	xarray_verify(xa1);

	xa2->elems_nr = xa2->sla.total_size / xa2->elem_size;
	xarray_verify(xa2);

	sla_destroy(&xa->sla);
	free(xa);
}

static inline size_t
xarray_elem_size(xarray_t *xarr)
{
	xarray_verify(xarr);
	return xarr->elem_size;
}

static inline xelem_t *
xarray_getlast(xarray_t *xarr)
{
	sla_node_t *last;
	size_t offset;

	last = SLA_TAIL_NODE(&xarr->sla, 0);
	offset = SLA_NODE_NITEMS(last) - xarr->elem_size;
	return (xelem_t *)((char *)last->chunk + offset);
}

static inline xelem_t *
xarray_get(xarray_t *xarr, long idx)
{
	sla_node_t *node;
	size_t elem_size, chunk_off;

	idx = xarr_idx(xarr, idx);
	elem_size = xarr->elem_size;
	node = sla_find(&xarr->sla, idx*elem_size, &chunk_off);
	assert(chunk_off < node->chunk_size);

	return (xelem_t *)((char *)node->chunk + chunk_off);
}

static inline xelem_t *
xarray_getchunk(xarray_t *xarr, long idx, size_t *chunk_elems)
{
	sla_node_t *node;
	size_t elem_size, chunk_off;

	idx = xarr_idx(xarr, idx);
	elem_size = xarr->elem_size;
	node = sla_find(&xarr->sla, idx*elem_size, &chunk_off);
	assert(chunk_off < node->chunk_size);

	size_t chunk_len = node->chunk_size - chunk_off;
	assert(chunk_len % elem_size == 0);
	*chunk_elems = XARR_MIN(chunk_len / elem_size, xarray_size(xarr) - idx);

	return (xelem_t *)((char *)node->chunk + chunk_off);
}

static inline xelem_t *
xarray_append(xarray_t *xarr)
{
	size_t elem_size = xarr->elem_size;
	sla_t *sla = &xarr->sla;

	if (sla_tailnode_full(sla)) {
		size_t alloc_grain = xarr->elems_chunk_size*elem_size;
		char *buff = sla_chunk_alloc(alloc_grain);
		unsigned lvl;
		sla_node_t *node = sla_node_alloc(sla, buff, alloc_grain, &lvl);
		sla_append_node(sla, node, lvl);
	}

	xelem_t *ret = sla_append_tailnode__(sla, &elem_size);
	assert(ret != NULL);
	assert(elem_size == xarr->elem_size);
	xarr->elems_nr++;
	xarray_verify(xarr);
	return ret;
}

static inline xelem_t *
xarray_append_prepare(xarray_t *xarr, size_t *nelems)
{
	sla_t *sla = &xarr->sla;
	size_t elem_size = xarr->elem_size;

	//RLE_TIMER_START(sla_append_prepare, rle_getmyid());

	if (sla_tailnode_full(sla)) {
		size_t alloc_grain = xarr->elems_chunk_size*elem_size;
		char *buff = sla_chunk_alloc(alloc_grain);
		unsigned lvl;
		sla_node_t *node = sla_node_alloc(sla, buff, alloc_grain, &lvl);
		sla_append_node(sla, node, lvl);
	}

	sla_node_t *n = SLA_TAIL_NODE(sla, 0);
	assert(n != sla->head);
	size_t nlen = SLA_NODE_NITEMS(n);
	assert(nlen < n->chunk_size);
	size_t chunk_len = n->chunk_size - nlen;
	assert(chunk_len % elem_size == 0);
	*nelems = chunk_len / elem_size;

	//RLE_TIMER_PAUSE(sla_append_prepare, rle_getmyid());

	return n->chunk + nlen;
}

static inline void
xarray_append_finalize(xarray_t *xarr, size_t nelems)
{
	sla_t *sla = &xarr->sla;
	size_t elem_size = xarr->elem_size;
	sla_node_t *n = SLA_TAIL_NODE(sla, 0);
	assert(n != sla->head);
	size_t upd_len = nelems*elem_size;
	assert(n->chunk_size - SLA_NODE_NITEMS(n) >= upd_len);

	for (unsigned i=0; i<sla->cur_level; i++) {
		if (SLA_TAIL_NODE(sla, i) == n)
			SLA_NODE_CNT(n, i) += upd_len;
		else
			SLA_TAIL_CNT(sla, i) += upd_len;
	}

	sla->total_size += upd_len;
	xarr->elems_nr  += nelems;
	xarray_verify(xarr);
}

static inline void
xarray_pop(xarray_t *xarr, size_t elems)
{
	const size_t elem_size = xarr->elem_size;
	size_t pop_len = elems*elem_size;

	do {
		size_t cnt = pop_len;
		sla_pop_tailnode(&xarr->sla, &cnt);
		assert(pop_len >= cnt);
		pop_len -= cnt;
	} while (pop_len > 0);

	xarr->elems_nr -= elems;
	xarray_verify(xarr);
}

static inline void
xarray_print(xarray_t *xarr)
{
	sla_print(&xarr->sla);
}

// let xarray.h know that we are implementing our own slices
#define XSLICE_

// XXX: do we need something line nextchunk() to make things faster?
//     this would return the first chunk and its size, and move the head to the
//     next chunk

// TODO: add support for negative indexes in slices

struct xslice_s {
	size_t start, len;
	xarray_t *xarr;
	sla_fwrd_t ptr[SLA_MAX_LEVEL];
};

static inline void
xslice_validate(xslice_t *xsl)
{
	#if !defined(NDEBUG)
	size_t real_idx = xsl->start*xsl->xarr->elem_size;
	size_t ptr_key  = xsl->ptr[0].cnt;
	assert(real_idx >= ptr_key);
	assert(real_idx <  ptr_key + SLA_NODE_NITEMS(xsl->ptr[0].node));
	#endif
}

static inline void
xslice_init(xarray_t *xarr, size_t idx, size_t len, xslice_t *xsl)
{
	size_t real_idx;
	if (idx + len > xarray_size(xarr)) {
		fprintf(stderr, "idx=%zu len=%zu xarr_size=%zu\n",
		        idx, len, xarray_size(xarr));
		abort();
	}
	xsl->start = idx;
	xsl->len = len;
	xsl->xarr = xarr;

	real_idx = idx*xarr->elem_size;
	sla_setptr(&xarr->sla, real_idx, xsl->ptr);
}

static inline xelem_t *
xslice_get(xslice_t *xsl, long idx)
{
	size_t real_idx, chunk_off;
	sla_node_t *node;
	xarray_t *xarr;

	assert(idx >= 0 && "FIXME");
	if (idx >= xsl->len)
		return NULL;

	xarr = xsl->xarr;
	real_idx = (xsl->start + idx)*xarr->elem_size;
	node = sla_ptr_find(&xarr->sla, xsl->ptr, real_idx, &chunk_off);
	//node = sla_find(&xarr->sla, real_idx, &chunk_off);
	assert(chunk_off < node->chunk_size);
	//if (idx==0) printf("%20s: slice:%p real_idx=%lu node=%p off=%lu\n", __FUNCTION__, xsl, real_idx, node, chunk_off);
	return (xelem_t *)((char *)node->chunk + chunk_off);
}

static inline xelem_t *
xslice_getnext(xslice_t *xsl)
{
	size_t real_idx, chunk_off, node_key, elem_size;
	sla_node_t *node;
	xelem_t *ret;


	assert(xsl->len > 0);
	node      = xsl->ptr[0].node;
	node_key  = xsl->ptr[0].cnt;
	elem_size = xsl->xarr->elem_size;
	real_idx  = xsl->start*elem_size;

	assert(xsl->ptr[0].cnt + SLA_NODE_NITEMS(node) > real_idx);
	assert(node_key <= real_idx);
	chunk_off = real_idx - node_key;

	ret = (xelem_t *)((char *)node->chunk + chunk_off);
	assert(xslice_get(xsl, 0) == ret);

	if (node->chunk_size - chunk_off == elem_size) {
		size_t __attribute__((unused)) dummy ;
		xelem_t __attribute__((unused)) *xret;
		xret = xslice_getnextchunk(xsl, &dummy);
		assert(dummy == 1);
		assert(xret == ret);
	} else {
		(xsl->start)++;
		(xsl->len)--;
	}

	//printf("%20s: slice:%p real_idx=%lu node=%p off=%lu\n", __FUNCTION__, xsl, real_idx, node, chunk_off);
	xslice_validate(xsl);
	return ret;
}

static inline xelem_t *
xslice_getchunk(xslice_t *xsl, long idx, size_t *elems)
{
	sla_node_t *n;
	size_t real_idx, chunk_off, chunk_size, elem_size;
	xarray_t *xarr;

	assert(idx >= 0 && "FIXME");
	if (idx >= xsl->len) {
		*elems = 0;
		return NULL;
	}

	xarr = xsl->xarr;
	elem_size = xarr->elem_size;
	real_idx = (xsl->start + idx)*elem_size;
	n = sla_ptr_find(&xarr->sla, xsl->ptr, real_idx, &chunk_off);
	//n = sla_find(&xarr->sla, real_idx, &chunk_off);
	chunk_size = n->chunk_size - chunk_off;
	assert(chunk_size % elem_size == 0);
	*elems = XARR_MIN(chunk_size / elem_size, xsl->len - idx);
	return (xelem_t *)((char *)n->chunk + chunk_off);
}

static inline xelem_t *
xslice_getnextchunk(xslice_t *xsl, size_t *nelems)
{
	sla_node_t *node;
	size_t node_key, real_idx, elem_size, chunk_off;

	//sla_print(&xsl->xarr->sla);
	//sla_ptr_print(xsl->ptr, xsl->xarr->sla.cur_level);

	elem_size = xsl->xarr->elem_size;
	real_idx  = xsl->start*elem_size;
	node      = sla_ptr_nextchunk(&xsl->xarr->sla, xsl->ptr, &node_key);
	if (node == NULL || xsl->len == 0) {
		*nelems = 0;
		return NULL;
	}

	assert(node_key <= real_idx);
	assert(node_key % elem_size == 0);

	chunk_off = real_idx - node_key;
	*nelems = XARR_MIN(xsl->len, (node->chunk_size - chunk_off) / elem_size);

	(xsl->start) += *nelems;
	(xsl->len)   -= *nelems;

	return (xelem_t *)((char *)node->chunk + chunk_off);
}

static inline size_t
xslice_size(xslice_t *xslice)
{
	return xslice->len;
}

static inline void
xslice_split(xslice_t *xsl, xslice_t *xsl1, xslice_t *xsl2)
{
	size_t l1 = xsl->len / 2;
	size_t l2 = xsl->len - l1;

	xsl1->start = xsl->start;
	xsl1->len = l1;

	xsl2->start = xsl->start + l1;
	xsl2->len = l2;

	xsl1->xarr = xsl2->xarr = xsl->xarr;

	memcpy(xsl1->ptr, xsl->ptr, sizeof(sla_fwrd_t)*SLA_MAX_LEVEL);
	xslice_validate(xsl1);
	//assert((unsigned long)xsl1->ptr == (unsigned long)&xsl1->ptr);
	size_t real_idx = xsl2->start*xsl2->xarr->elem_size;
	sla_ptr_setptr(&xsl->xarr->sla, xsl1->ptr, real_idx, xsl2->ptr);
	//sla_setptr(&xsl->xarr->sla, xsl2->start, xsl2->ptr);
	xslice_validate(xsl2);
}

#endif // XARRAY_SLA_H__
