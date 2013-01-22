#ifndef XVARRAY_H
#define XVARRAY_H

#include "xarray.h"

struct xvarray;
typedef struct xvarray xvarray_t;

xvarray_t *
xvarray_create(xarray_t *xarr);

// XXX: What happens when the last xvarray is destroyed?
// for compatibility with the copy case, we can say that the actual sla is
// destroyed. Need to see how that will work.
void
xvarray_destroy(xvarray_t *xvarr);

void
xvarray_init(xvarray_t *xvarr, xarray_t *xarr);

xvarray_t *
xvarray_branch(xvarray_t *xvarr);

// you can't change this pointer
xelem_t const *
xvarray_get_rd(xvarray_t *xvarr, long idx);

// you can change this pointer
xelem_t *
xvarray_get_rdwr(xvarray_t *xvarr, long idx);

#if defined(XARRAY_DA__)
#include "xvarray_dynarray.h"
#elif defined(XARRAY_SLA__)
#include "xvarray_sla.h"
#endif

#endif /* XVARRAY_H */
