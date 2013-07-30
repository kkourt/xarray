#ifndef XARRAY_RPA_H__
#define XARRAY_RPA_H__

#if !defined(XARRAY_H__)
#error "Don't include this file directly. Include xarray.h!"
#endif

#define XARRAY_IMPL "RPA"

#include "rope_array.h"

struct xarray_s {
	struct rpa rpa;
};

static inline size_t
xarray_size(xarray_t *xarr)
{
	return rpa_size(&xarr->rpa);
}

static inline size_t
xarray_elem_size(xarray_t *xarr)
{
	return rpa_elem_size(&xarr->rpa);
}

static inline void
xarray_verify(xarray_t *xarr)
{
	#if !defined(NDEBUG)
	rpa_verify(&xarr->rpa);
	#endif
}

static inline xelem_t *
xarray_getlast(xarray_t *xarr)
{
	return rpa_getlast(&xarr->rpa);
}

static inline xelem_t *
xarray_get(xarray_t *xarr, long i)
{
	size_t idx = xarr_idx(xarr, i);
	return rpa_getchunk(&xarr->rpa, idx, NULL);
}

static inline xelem_t *
xarray_getchunk(xarray_t *xarr, long i, size_t *nelems)
{
	size_t idx = xarr_idx(xarr, i);
	return rpa_getchunk(&xarr->rpa, idx, nelems);
}

static inline xelem_t *
xarray_append(xarray_t *xarr)
{
	return rpa_append(&xarr->rpa);
}

static inline xelem_t *
xarray_append_prepare(xarray_t *xarr, size_t *nelems)
{
	return rpa_append_prepare(&xarr->rpa, nelems);
}

static inline void
xarray_append_finalize(xarray_t *xarr, size_t nelems)
{
	rpa_append_finalize(&xarr->rpa, nelems);
}

static inline void
xarray_pop(xarray_t *xarr, size_t elems)
{
	do {
		size_t cnt = elems;
		rpa_pop(&xarr->rpa, &cnt);
		assert(cnt <= elems);
		elems -= cnt;
	} while (elems > 0);
}


void
xarray_init(xarray_t *xarr, struct xarray_init *init)
{
	rpa_init(&xarr->rpa, init->elem_size, init->rpa.elems_alloc_grain);
}

xarray_t *
xarray_create(struct xarray_init *init)
{
	xarray_t *ret;

	ret = xmalloc(sizeof(*ret));
	xarray_init(ret, init);

	return ret;
}

xarray_t *
xarray_concat(xarray_t *arr1, xarray_t *arr2)
{
	struct rpa_hdr *n1, *n2;

	assert(arr1->rpa.elem_size == arr2->rpa.elem_size);

	n1 = arr1->rpa.root;
	n2 = arr2->rpa.root;

	arr1->rpa.tail = arr2->rpa.tail;
	arr1->rpa.root = &rpa_concat(n1, n2)->n_hdr;

	free(arr2);
	return arr1;
}

// TODO
void
xarray_split(xarray_t *xa, xarray_t *xa1, xarray_t *xa2)
{
	assert(false);
}

#endif /* XARRAY_RPA_H__ */
