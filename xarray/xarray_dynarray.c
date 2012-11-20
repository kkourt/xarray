#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"
#include "xarray.h"
#include "xarray_dynarray.h"

#define ELEMS_ALLOC_GRAIN_DEFAULT 128

/**
 * Thin layer that implements xarray ontop of dynarray
 *  Most functions live in xarray_dynarray.h
 */

void
xarray_init(xarray_t *xarr, struct xarray_init *init)
{
	size_t elems_alloc_grain = init->da.elems_alloc_grain;
	if (elems_alloc_grain == 0)
		elems_alloc_grain = ELEMS_ALLOC_GRAIN_DEFAULT;

	dynarray_init(&xarr->da,
	              init->elem_size, elems_alloc_grain, init->elems_init);
}

xarray_t *
xarray_create(struct xarray_init *init)
{
	xarray_t *ret = malloc(sizeof(*ret));
	if (!ret) {
		perror("malloc");
		exit(1);
	}
	xarray_init(ret, init);
	return ret;
}

xarray_t *
xarray_concat(xarray_t *arr1, xarray_t *arr2)
{
	size_t arr2_size = dynarray_size(&arr2->da);
	size_t elem_size = dynarray_elem_size(&arr2->da);
	assert(elem_size == dynarray_elem_size(&arr1->da));
	void *src = dynarray_destroy(&arr2->da);
	if (arr2_size > 0) {
		void *dst = dynarray_alloc_nr(&arr1->da, arr2_size);
		memcpy(dst, src, arr2_size*elem_size);
		free(src);
	}
	return arr1;
}

