/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#ifndef XVARRAY_H
#define XVARRAY_H

#include "xarray.h"

struct xvarray;
typedef struct xvarray xvarray_t;

struct xvchunk;
typedef struct xvchunk xvchunk_t;

xvarray_t *
xvarray_create(xarray_t *xarr);

// XXX: What happens when the last xvarray is destroyed?
// for compatibility with the copy case, we can say that the actual sla is
// destroyed. Need to see how that will work.
static void
xvarray_destroy(xvarray_t *xvarr);

static void
xvarray_init(xvarray_t *xvarr, xarray_t *xarr);

static xvarray_t *
xvarray_branch(xvarray_t *xvarr);

// you can't change this pointer
static xelem_t const *
xvarray_get_rd(xvarray_t *xvarr, long idx);

// return a pointer to the @idx location of the array and the remaining number
// of elements @nelems in the contiguous chunk. This is only for reading
static xelem_t const *
xvarray_getchunk_rd(xvarray_t *xvarr, long idx, size_t *nelems);

// you can change this pointer
static xelem_t *
xvarray_get_rdwr(xvarray_t *xvarr, long idx);

// as the _rd variant, but you can write this pointer
static xelem_t *
xvarray_getchunk_rdwr(xvarray_t *xvarr, long idx, size_t *nelems);

#if defined(XARRAY_DA__)
#include "xvarray_dynarray.h"
#elif defined(XARRAY_SLA__)
#include "xvarray_sla.h"
#endif

#endif /* XVARRAY_H */
