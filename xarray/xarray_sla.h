#ifndef XARRAY_SLA_H__
#define XARRAY_SLA_H__

#include <stddef.h> // size_t

#if !defined(XARRAY_H__)
#error "Don't include this file directly. Include xarray.h first!"
#endif

#include "sla-chunk.h"
#include "misc.h"

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
	sla_verify(&xarr->sla);
}

void
xarray_init(xarray_t *xarr, struct xarray_init *init)
{
	xarr->elems_nr = 0;
	xarr->elem_size = init->elem_size;
	xarr->elems_chunk_size = init->sla.elems_chunk_size;
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
xarray_get(xarray_t *xarr, long idx)
{
	size_t xarr_size = xarray_size(xarr);
	if (idx < 0)
		idx = xarr_size + idx;
	assert(idx >= 0 && idx < xarr_size);

	sla_node_t *node;
	size_t chunk_off;
	node = sla_find(&xarr->sla, idx*xarr->elem_size, &chunk_off);
	assert(chunk_off < node->chunk_size);
	return (xelem_t *)((char *)node->chunk + chunk_off);
}

static inline xelem_t *
xarray_getchunk(xarray_t *xarr, long idx, size_t *chunk_elems)
{
	size_t elem_size = xarr->elem_size;
	size_t xarr_size = xarray_size(xarr);
	if (idx < 0)
		idx = xarr_size + idx;
	assert(idx >= 0 && idx < xarr_size);

	size_t chunk_off;
	sla_node_t *n = sla_find(&xarr->sla, idx*elem_size, &chunk_off);
	assert(chunk_off < n->chunk_size);

	size_t chunk_len = n->chunk_size - chunk_off;
	assert(chunk_len % elem_size == 0);
	*chunk_elems = chunk_len / elem_size;

	return (xelem_t *)((char *)n->chunk + chunk_off);
}

static inline xelem_t *
xarray_append(xarray_t *xarr)
{
	size_t elem_size = xarr->elem_size;
	sla_t *sla = &xarr->sla;

	if (sla_tailnode_full(sla)) {
		size_t alloc_grain = xarr->elems_chunk_size*elem_size;
		char *buff = xmalloc(alloc_grain);
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
xarray_pop(xarray_t *xarr, size_t elems)
{
	size_t elem_size = xarr->elem_size;
	xelem_t *ret = sla_pop_tailnode(&xarr->sla, &elem_size);
	assert(elem_size == xarr->elem_size);
	xarr->elems_nr--;
	xarray_verify(xarr);
	return ret;
}

#endif
