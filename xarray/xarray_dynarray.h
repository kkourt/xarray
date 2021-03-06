/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#ifndef XARRAY_DYNARRAY_H__
#define XARRAY_DYNARRAY_H__

#if !defined(XARRAY_H__)
#error "Don't include this file directly. Include xarray.h first!"
#endif

#define XARRAY_IMPL "DA"

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

static inline void
xarray_verify(xarray_t *xarr)
{
}

static inline xelem_t *
xarray_getlast(xarray_t *xarr)
{
	size_t da_size = dynarray_size(&xarr->da);
	return dynarray_get(&xarr->da, da_size - 1);
}

static inline xelem_t *
xarray_get(xarray_t *xarr, long i)
{
	size_t idx = xarr_idx(xarr, i);
	return dynarray_get(&xarr->da, idx);
}

static inline xelem_t *
xarray_getchunk(xarray_t *xarr, long i, size_t *nelems)
{
	size_t da_size = dynarray_size(&xarr->da);
	size_t idx = xarr_idx(xarr, i);
	*nelems = da_size - idx;
	return dynarray_get(&xarr->da, idx);
}

static inline xelem_t *
xarray_append(xarray_t *xarr)
{
	return dynarray_alloc(&xarr->da);
}

// XXX: uses private fields of da
static inline xelem_t *
xarray_append_prepare(xarray_t *xarr, size_t *nelems)
{
	dynarray_t *da = &xarr->da;
	if (da->next_idx >= da->elems_nr) {
		assert(da->next_idx == da->elems_nr);
		dynarray_expand(da);
	}

	assert(da->next_idx < da->elems_nr);
	*nelems = da->elems_nr - da->next_idx;
	return dynarray_get__(da, da->next_idx);
}

// XXX: uses private fields of da
static inline void
xarray_append_finalize(xarray_t *xarr, size_t nelems)
{
	dynarray_t *da = &xarr->da;
	assert(nelems <= da->elems_nr - da->next_idx);
	da->next_idx += nelems;
}

static inline void
xarray_pop(xarray_t *xarr, size_t elems)
{
	dynarray_dealloc_nr(&xarr->da, elems);
}

static inline void
xarray_print(xarray_t *xarr)
{
	//printf("%s: (%s:%s) NYI\n", __FUNCTION__, __FILE__, __LINE__);
}

#endif
