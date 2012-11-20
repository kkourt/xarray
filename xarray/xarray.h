
#ifndef XARRAY_H__
#define XARRAY_H__

#include <stddef.h> // size_t
#include <assert.h>

#define MIN(x,y) ((x<y) ? (x):(y))

/* just to make the interface self-explanatory */
struct xarray_s;
typedef struct xarray_s xarray_t;
typedef void xelem_t;


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

/**
 * idx:
 *    0 : first
 *    1 : second
 *    ...
 *   -2 : second to last
 *   -1 : last
 *
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
static xelem_t *xarray_getchunk(xarray_t *xarr, long idx, size_t *chunk_size);
static xelem_t *xarray_append(xarray_t *xarr);
//static xelem_t *xarray_append_chunk(xarray_t *xarr, size_t elems_nr, size_t *chunk_size);
static xelem_t *xarray_pop(xarray_t *xarr, size_t elems);

struct xslice_s;
typedef struct xslice_s xslice_t;

static void     xslice_init(xarray_t *xarr, size_t idx, size_t len, xslice_t *xsl);
static xelem_t *xslice_get(xslice_t *xslice, long idx);
static xelem_t *xslice_getchunk(xslice_t *xslice, long idx, size_t *chunk_size);
static size_t   xslice_size(xslice_t *xslice);
static void     xslice_split(xslice_t *xsl, xslice_t *xsl1, xslice_t *xsl2);


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

static inline void
xslice_init(xarray_t *xarr, size_t idx, size_t len, xslice_t *xsl)
{
	assert(idx + len <= xarray_size(xarr));
	xsl->idx = idx;
	xsl->len = len;
	xsl->xarr = xarr;

}

static inline xelem_t *
xslice_get(xslice_t *xsl, long idx)
{
	return idx < xsl->len ? xarray_get(xsl->xarr, xsl->idx + idx) : NULL;
}

static inline xelem_t *
xslice_getchunk(xslice_t *xsl, long idx, size_t *chunk_size)
{
	xelem_t *ret;
	if (idx < xsl->len) {
		size_t cs;
		ret = xarray_getchunk(xsl->xarr, xsl->idx + idx, &cs);
		*chunk_size = MIN(cs, xsl->len - idx);
	} else {
		ret = NULL;
		*chunk_size = 0;
	}
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
