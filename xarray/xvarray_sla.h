#ifndef XVARRAY_SLA_H
#define XVARRAY_SLA_H

#if !defined(XVARRAY_H)
#error "Don't include this file directly. Include xvarray.h"
#endif

#include "xarray_sla.h"
#include "ver.h"
#include "verp.h"
#include "misc.h"

// this is common for all xvarray structures. We could embed it into struct sla,
// I thought it might be better to keep them seperate.
struct xvarray_glbl {
	spinlock_t lock;
	ver_t      *v_base;
};

struct xvarray {
	xarray_t            *xarr;
	ver_t               *xv_ver;
	struct xvarray_glbl *glbl;
};

/*
 * ver->v_log is:
 *   v_log[0]:             Number of entries
 *   v_log[1]...v_log[-2]: Entries
 *   v_log[-1]:            ->next pointer (last)
 */
void
xvarr_log_init(ver_t *v)
{
	v->v_log[0] = 0;
}

void
xvarray_init(xvarray_t *xvarr, xarray_t *xarr)
{
	ver_t *ver;
	ver = ver_create();

	xvarr->xarr   = xarr;
	xvarr->xv_ver = ver;

	xvarr->glbl         = xmalloc(sizeof(struct xvarray_glbl));
	xvarr->glbl->v_base = ver;
	spinlock_init(&xvarr->glbl->lock);

	xvarr_log_init(ver);
}


xvarray_t *
xvarray_create(xarray_t *xarr)
{
	xvarray_t *ret;
	ret = xmalloc(sizeof(xvarray_t));
	xvarray_init(ret, xarr);
	return ret;
}

void
xvarray_do_destroy(xvarray_t *xvarr)
{
	if ( ver_putref(xvarr->xv_ver) == NULL) {
		// TODO: all references are gone
		// delete ->xarr?
	}
}

void
xvarray_destroy(xvarray_t *xvarr)
{
	xvarray_do_destroy(xvarr);
	free(xvarr);
}

void
xvarray_do_branch(xvarray_t *xvarr, xvarray_t *xvarr_parent)
{
	xvarr->xarr   = xvarr_parent->xarr;
	xvarr->glbl   = xvarr_parent->glbl;
	xvarr->xv_ver = ver_branch(xvarr_parent->xv_ver);
	xvarr_log_init(xvarr->xv_ver);
}

xvarray_t *
xvarray_branch(xvarray_t *xvarr)
{
	xvarray_t *ret;
	ret = xmalloc(sizeof(xvarray_t));
	xvarray_do_branch(ret, xvarr);
	return ret;
}

// you can't change this pointer
xelem_t const *
xvarray_get_rd(xvarray_t *xvarr, long idx)
{
	sla_node_t *node;
	size_t chunk_off;
	size_t elem_size;
	void *data;

	elem_size = xvarr->xarr->elem_size;
	idx       = xarr_idx(xvarr->xarr, idx);
	node      = sla_find(&xvarr->xarr->sla, idx*elem_size, &chunk_off);
	data      = vp_ptr(node->chunk, xvarr->xv_ver);

	return (xelem_t *)((char *)data + chunk_off);
}

// you can change this pointer
xelem_t *
xvarray_get_rdwr(xvarray_t *xvarr, long idx)
{
	sla_node_t *node;
	size_t chunk_off, elem_size;
	void *vp, *chunk;
	verp_t *verp;

	elem_size = xvarr->xarr->elem_size;
	idx       = xarr_idx(xvarr->xarr, idx);
	node      = sla_find(&xvarr->xarr->sla, idx*elem_size, &chunk_off);
	vp        = node->chunk;

	if (vp_is_normal_ptr(vp)) {
		chunk = vp;
		// XXX: there is a race on xvarr->glbl->v_base here, but I don't
		// think it matters, as long as we either get the old or the new
		// ->v_base
		if (ver_eq(xvarr->xv_ver, xvarr->glbl->v_base)) {
			// just a pointer and we are the base, just return the
			// pointer
			goto end;
		}
		// need to transform this pointer to a verp
		// Create one, and add the pointer for the base version
		verp = verp_alloc();
		verp_insert_ptr(verp, xvarr->glbl->v_base, vp);
		node->chunk = verp_mark_ptr(verp);
	} else {
		ver_t *data_ver;
		verp = vp_unmark_ptr(vp);
		chunk = verp_find_ptr(verp, xvarr->xv_ver, &data_ver);
		if (data_ver == xvarr->xv_ver) {
			// our version exists on the map, so we just return the
			// pointer
			goto end;
		}
	}

	// If we reach to this point, we need to insert an entry to the map. The
	// map will point to a new chunk, which we need to copy
	size_t alloc_grain = xvarr->xarr->elems_chunk_size*elem_size;
	void *dst = sla_chunk_alloc(alloc_grain);
	memcpy(dst, chunk, alloc_grain);
	verp_insert_ptr(verp, xvarr->xv_ver, dst);
	chunk = dst;

end:
	return (xelem_t *)((char *)chunk + chunk_off);
}

#endif /* XARRAY_MV_SLA_H */
