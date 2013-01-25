
#ifndef XARRAY_H__
#define XARRAY_H__

#include <stddef.h> // size_t
#include <assert.h>
#include <string.h> // memcpy

#define MIN(x,y) ((x<y) ? (x):(y))

/* just to make the interface self-explanatory */
struct xarray_s;
typedef struct xarray_s xarray_t;
typedef void xelem_t;


struct xarray_init {
	// @elem_size: size of elements
	size_t elem_size;
	struct {
		/* dynarray-specific parameters:
		 *  @elems_alloc_grain: allocation grain in elements
		 *  @elems_init: initial array size in elements
		 * */
		struct {
			size_t elems_alloc_grain;
			size_t elems_init;
		} da;
		/* sla-specific parameters
		 *  @p: percentage of skip list
		 *  @max_level: maximum level of skip-list
		 *  @elems_chunk_size: size of chunk in elements
		 */
		struct {
			float p;
			unsigned max_level;
			size_t elems_chunk_size;
		} sla;
	};
};

/**
 * Indexes:
 * idx:
 *    0 : first
 *    1 : second
 *    ...
 *   -2 : second to last
 *   -1 : last
 *
 */

/*
 * concatenate two arrays
 *  return the concatenated array
 *   @arr1 and @arr2 become invalid
 */

xarray_t *xarray_create(struct xarray_init *init);
void      xarray_init(xarray_t *xarr, struct xarray_init *init);
xarray_t *xarray_concat(xarray_t *arr1, xarray_t *arr2);
void      xarray_split(xarray_t *xa, xarray_t *xa1, xarray_t *xa2);

static size_t   xarray_size(xarray_t *xarr);
static size_t   xarray_elem_size(xarray_t *xarr);
static xelem_t *xarray_get(xarray_t *xarr, long idx);
// return a pointer to the @idx location of the array and the remaining number
// of elements @nelems in the contiguous chunk.
static xelem_t *xarray_getchunk(xarray_t *xarr, long idx, size_t *nelems);
static xelem_t *xarray_append(xarray_t *xarr);

// @nelems in prepare is only an output variable
static xelem_t *xarray_append_prepare(xarray_t *xarr, size_t *nelems);
static void     xarray_append_finalize(xarray_t *xarr, size_t nelems);

static xelem_t *xarray_pop(xarray_t *xarr, size_t elems);

struct xslice_s;
typedef struct xslice_s xslice_t;

static void     xslice_init(xarray_t *xarr, size_t idx, size_t len, xslice_t *xsl);
static xelem_t *xslice_get(xslice_t *xslice, long idx);
static xelem_t *xslice_getchunk(xslice_t *xslice, long idx, size_t *nelems);
static xelem_t *xslice_getnextchunk(xslice_t *xslice, size_t *nelems);
static size_t   xslice_size(xslice_t *xslice);
static void     xslice_split(xslice_t *xsl, xslice_t *xsl1, xslice_t *xsl2);

static inline long
xarr_idx(xarray_t *xarr, long i)
{
	size_t xarr_size = xarray_size(xarr);
	if (i < 0)
		i = xarr_size + i;
	assert(i >= 0 && i < xarr_size);
	return i;
}

static inline void
xarray_append_elems(xarray_t *xarr, xelem_t *elems, size_t total_elems)
{
	size_t   nelems, elems_i, elem_size;
	xelem_t *dst;

	elem_size = xarray_elem_size(xarr);
	elems_i   = 0;
	while (total_elems > 0) {
		dst = xarray_append_prepare(xarr, &nelems);
		nelems = MIN(nelems, total_elems);
		memcpy(dst, elems + elems_i, nelems*elem_size);
		xarray_append_finalize(xarr, nelems);
		elems_i     += nelems;
		total_elems -= nelems;
	}
}

// append @total_elems, and set to @c
static inline void
xarray_append_set(xarray_t *xarr, char c, size_t total_elems)
{
	size_t nelems, elem_size;
	xelem_t *dst;

	elem_size = xarray_elem_size(xarr);
	while (total_elems > 0) {
		dst = xarray_append_prepare(xarr, &nelems);
		nelems = MIN(nelems, total_elems);
		memset(dst, c, nelems*elem_size);
		xarray_append_finalize(xarr, nelems);
		total_elems -= nelems;
	}
}

#if defined(XARRAY_DA__)
#include "xarray_dynarray.h"
#elif defined(XARRAY_SLA__)
#include "xarray_sla.h"
#endif

#ifndef XSLICE_
#define XSLICE_
struct xslice_s {
	size_t idx, len;
	xarray_t *xarr;
};

static inline long
xsl_idx(xslice_t *xsl, long i)
{
	size_t xsl_size = xsl->len;
	if (i < 0)
		i = xsl_size + i;
	return i;
}

static inline void
xslice_init(xarray_t *xarr, size_t idx, size_t len, xslice_t *xsl)
{
	idx = xarr_idx(xarr, idx);
	assert(idx + len <= xarray_size(xarr));
	xsl->idx = idx;
	xsl->len = len;
	xsl->xarr = xarr;

}

static inline xelem_t *
xslice_get(xslice_t *xsl, long idx)
{
	idx = xsl_idx(xsl, idx);
	return idx < xsl->len ? xarray_get(xsl->xarr, xsl->idx + idx) : NULL;
}

static inline xelem_t *
xslice_getnext(xslice_t *xsl)
{
	xelem_t *ret = NULL;
	if (xsl->len > 0) {
		ret = xarray_get(xsl->xarr, xsl->idx);
		xsl->idx++;
		xsl->len--;
	} else ret = NULL;

	return ret;
}

static inline xelem_t *
xslice_getchunk(xslice_t *xsl, long idx, size_t *chunk_elems)
{
	xelem_t *ret;
	idx = xsl_idx(xsl, idx);
	if (idx < xsl->len) {
		size_t cs;
		ret = xarray_getchunk(xsl->xarr, xsl->idx + idx, &cs);
		*chunk_elems = MIN(cs, xsl->len - idx);
	} else {
		ret = NULL;
		*chunk_elems = 0;
	}
	return ret;
}

static inline xelem_t *
xslice_getnextchunk(xslice_t *xslice, size_t *nelems)
{
	xelem_t *ret;

	ret = xslice_getchunk(xslice, 0, nelems);
	assert(*nelems <= xslice->len);
	xslice->idx += *nelems;
	xslice->len -= *nelems;
	return ret;
}

static inline size_t
xslice_size(xslice_t *xsl)
{
	return xsl->len;
}

static inline void
xslice_split(xslice_t *xsl, xslice_t *xsl1, xslice_t *xsl2)
{
	size_t l1 = xsl->len / 2;
	size_t l2 = xsl->len - l1;

	xsl1->idx = xsl->idx;
	xsl1->len = l1;

	xsl2->idx = xsl->idx + l1;
	xsl2->len = l2;

	xsl1->xarr = xsl2->xarr = xsl->xarr;
}
#endif /* XSLICE_ */

#endif /* XARRAY_H__ */
