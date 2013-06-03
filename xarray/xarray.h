
#ifndef XARRAY_H__
#define XARRAY_H__

#include <stddef.h> // size_t
#include <assert.h>
#include <string.h> // memcpy

#define XARR_MIN(x,y) ((x<y) ? (x):(y))

/* just to make the interface self-explanatory */
struct xarray_s;
typedef struct xarray_s xarray_t;
typedef void xelem_t;


// implementation specific xarray initialization parameters
struct xarray_init {
	// @elem_size: size of elements
	size_t elem_size;
	union {
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

// _create: allocate and initialize an xarray
// _init:   initialize an already allocate xarray
xarray_t *xarray_create(struct xarray_init *init);
void      xarray_init(xarray_t *xarr, struct xarray_init *init);

//  _size:      return the size of the array in elements
//  _elem_size: return the element size of the array
static size_t   xarray_size(xarray_t *xarr);
static size_t   xarray_elem_size(xarray_t *xarr);

/**
 * xarray uses the following indexing scheme:
 * index:
 *    0 : first
 *    1 : second
 *    ...
 *   -2 : second to last
 *   -1 : last
 *
 */

// find actual index based on the above scheme
static inline long
xarr_idx(xarray_t *xarr, long i)
{
	size_t xarr_size = xarray_size(xarr);
	if (i < 0)
		i = xarr_size + i;
	assert(i >= 0 && i < xarr_size);
	return i;
}

// get element at index @idx (indices are in elements)
static xelem_t *xarray_get(xarray_t *xarr, long idx);
// return a pointer to on index @idx (in elements) and set @nelems to the
// remaining number of elements in the returned contiguous chunk.
static xelem_t *xarray_getchunk(xarray_t *xarr, long idx, size_t *nelems);

/**
 * To allow for in-place appends we break it in two steps:
 *   - _prepare, where a buffer is returned
 *      @nelems is an output-only variable set to the size of the buffer
 *
 *   ... user writes up to @nelems elements to the buffer ....
 *
 *   - _finalize, finalizes the append
 *      @nelems is the number of elements actually written to the buffer
 *
 *  (also see xarray_append_elems, xarray_append_set below)
 */
static xelem_t *xarray_append_prepare(xarray_t *xarr, size_t *nelems);
static void     xarray_append_finalize(xarray_t *xarr, size_t nelems);
// append a single element -- returns a pointer to write the element
static xelem_t *xarray_append(xarray_t *xarr);

/**
 * concatenate two arrays
 *  return the concatenated array
 *   @arr1 and @arr2 become invalid
 */
xarray_t *xarray_concat(xarray_t *arr1, xarray_t *arr2);

/**
 * split an array approximately in the middle
 *  approximately -> within a constant (typically the allocation grain)
 *   TODO (split in @idx)
 */
void      xarray_split(xarray_t *xa, xarray_t *xa1, xarray_t *xa2);


/**
 * pop @nelems elements from array -- i.e., reduce its size by discarding
 * elements from the end.
 *
 * Concerns about _pop() interface:
 * - returning an in-place buffer with the elements is not straightforward
 *   because the caller will then become responsible for deallocating the
 *   buffer. Since this will not happen on all calls there will need to be a
 *   flag to notify the caller on when it needs to deallocate the pointer it
 *   got. This seems too complicated, so we don't do it.
 *
 * - On structured implementations, there is the question of what happens when
 *   the user tries to pop more than the internal chunk of the tail node. Either
 *   the function loops over and deallocates multiple nodes or returns to the
 *   user the number of elements it actually popped. If we returned the popped
 *   buffer, the latter method would be the sensible thing to do. Since we
 *   don't, we follow the former approach -- the implementation should loop over
 *   multiple nodes if needed.
 */
static void xarray_pop(xarray_t *xarr, size_t nelems);

/**
 * Xarray slices
 */
struct xslice_s;
typedef struct xslice_s xslice_t;

static void     xslice_init(xarray_t *xarr, size_t idx, size_t len, xslice_t *xsl);
static xelem_t *xslice_get(xslice_t *xslice, long idx);
static xelem_t *xslice_getchunk(xslice_t *xslice, long idx, size_t *nelems);
static xelem_t *xslice_getnextchunk(xslice_t *xslice, size_t *nelems);
static size_t   xslice_size(xslice_t *xslice);
static void     xslice_split(xslice_t *xsl, xslice_t *xsl1, xslice_t *xsl2);

/**
 * xarray append helpers
 */
static inline void
// helper to append @total_elems from @elems in @xarr
xarray_append_elems(xarray_t *xarr, xelem_t *elems, size_t total_elems)
{
	size_t   nelems, elems_i, elem_size;
	xelem_t *dst;

	elem_size = xarray_elem_size(xarr);
	elems_i   = 0;
	while (total_elems > 0) {
		dst = xarray_append_prepare(xarr, &nelems);
		nelems = XARR_MIN(nelems, total_elems);
		memcpy(dst, elems + elems_i, nelems*elem_size);
		xarray_append_finalize(xarr, nelems);
		elems_i     += nelems;
		total_elems -= nelems;
	}
}

// helper append @total_elems that are set to @c
static inline void
xarray_append_set(xarray_t *xarr, char c, size_t total_elems)
{
	size_t nelems, elem_size;
	xelem_t *dst;

	elem_size = xarray_elem_size(xarr);
	while (total_elems > 0) {
		dst = xarray_append_prepare(xarr, &nelems);
		nelems = XARR_MIN(nelems, total_elems);
		memset(dst, c, nelems*elem_size);
		xarray_append_finalize(xarr, nelems);
		total_elems -= nelems;
	}
}

// include xarray implementation functions
#if defined(XARRAY_DA__)
#include "xarray_dynarray.h"
#elif defined(XARRAY_SLA__)
#include "xarray_sla.h"
#endif

/**
 * If xarray implementations define their own slice functions, they should
 * define XSLICE_.
 *
 * If XSLICE_ is not defined, then slices are implemented ontop of the xarray
 * interface below.
 */
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
		*chunk_elems = XARR_MIN(cs, xsl->len - idx);
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
