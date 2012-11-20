#ifndef XARRAY_DYNARRAY_H__
#define XARRAY_DYNARRAY_H__

#if !defined(XARRAY_H__)
#error "Don't include this file directly. Include xarray.h first!"
#endif

#include "dynarray.h"
#include <string.h>

struct xarray_s {
	dynarray_t da;
};

static inline size_t
xarray_size(xarray_t *xarr)
{
	return dynarray_size(&xarr->da);
}

static inline size_t
xarray_elem_size(xarray_t *xarr)
{
	return dynarray_elem_size(&xarr->da);
}

static inline xelem_t *
xarray_get(xarray_t *xarr, long i)
{
	size_t da_size = dynarray_size(&xarr->da);
	if (i < 0)
		i = da_size + i;
	assert(i >= 0 && i < da_size);
	return dynarray_get(&xarr->da, i);
}

static inline xelem_t *
xarray_getchunk(xarray_t *xarr, long i, size_t *chunk_size)
{
	size_t da_size = dynarray_size(&xarr->da);
	if (i < 0)
		i = da_size + i;
	assert(i >= 0 && i < da_size);
	*chunk_size = da_size - i;
	return dynarray_get(&xarr->da, i);
}

static inline xelem_t *
xarray_append(xarray_t *xarr)
{
	return dynarray_alloc(&xarr->da);
}

static inline xelem_t *
xarray_pop(xarray_t *xarr, size_t elems)
{
	xelem_t *ret = xarray_get(xarr, -elems);
	dynarray_dealloc_nr(&xarr->da, elems);
	return ret;
}

#endif
