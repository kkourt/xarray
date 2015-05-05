#ifndef XARRAY_RPA_H__
#define XARRAY_RPA_H__

#if !defined(XARRAY_H__)
#error "Don't include this file directly. Include xarray.h!"
#endif

#define XARRAY_IMPL "RPA"

#include "rope_array.h"

static void
xarr_rpa_rebal_implicit(struct rpa *rpa)
{
	//rpa_rebalance(rpa);
}

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
	xarr_rpa_rebal_implicit(&xarr->rpa);
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

	xarr_rpa_rebal_implicit(&arr1->rpa);
	return arr1;
}

// TODO
void
xarray_split(xarray_t *xa, xarray_t *xa1, xarray_t *xa2)
{
	assert(false);
}

#define XARR_REBALANCE_
static inline void
xarray_rebalance(xarray_t *xa)
{
	rpa_rebalance(&xa->rpa);
}

static inline bool
xarray_is_balanced(xarray_t *xarr)
{
	return rpa_is_balanced(xarr->rpa.root);
}

static inline void
xarray_print(xarray_t *xarr)
{
	rpa_print(&xarr->rpa);
}

/**
 * Slices
 */

// let xarray.h know that we are implementing our own slices
#define XSLICE_

struct xslice_s {
	struct rpa_slice sl;
};

static inline size_t
xslice_size(xslice_t *xslice)
{
	return xslice->sl.sl_len;
}

static inline long
xsl_idx(xslice_t *xsl, long i)
{
	if (i < 0)
		i = xslice_size(xsl) + i;
	return i;
}

static inline void
xslice_init(xarray_t *xarr, size_t idx, size_t len, xslice_t *xsl)
{
	idx = xarr_idx(xarr, idx);
	assert(idx + len <= xarray_size(xarr));
	rpa_init_slice(&xsl->sl, &xarr->rpa, idx, len);
}

static inline xelem_t *
xslice_getchunk(xslice_t *xsl, long idx, size_t *chunk_elems)
{
	return rpa_slice_getchunk(&xsl->sl, idx, chunk_elems);
}


static inline xelem_t *
xslice_get(xslice_t *xsl, long idx)
{
	size_t dummy;
	return xslice_getchunk(xsl, idx, &dummy);
}

static inline xelem_t *
xslice_getnextchunk(xslice_t *xsl, size_t *nelems)
{
	return rpa_slice_getnextchunk(&xsl->sl, nelems);
}

static inline xelem_t *
xslice_getnext(xslice_t *xsl)
{
	void *ret;
	size_t dummy;

	if (xslice_size(xsl) == 0) {
		return NULL;
	}

	// get chunk to return
	ret = rpa_slice_get_firstchunk(&xsl->sl, &dummy);
	assert(dummy > 0);
	rpa_slice_move_start(&xsl->sl, 1);

	return ret;
}

static inline void
xslice_split(xslice_t *xsl, xslice_t *xsl1, xslice_t *xsl2)
{
	rpa_slice_split(&xsl->sl, &xsl1->sl, &xsl2->sl);
}

#endif /* XARRAY_RPA_H__ */
