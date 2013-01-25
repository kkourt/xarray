#ifndef XVARRAY_SLA_H
#define XVARRAY_SLA_H

#if !defined(XVARRAY_H)
#error "Don't include this file directly. Include xvarray.h"
#endif

#include "xarray_sla.h"
#include "ver.h"
#include "verp.h"
#include "misc.h"

// verp_log lives in a version and is used to maintain information about what
// verps have mappings for a particular version. This is aimed to help
// collecting garbage versions.
// Q: Do we assume that the log is complete?
// Q: When do we use the log to remove the versions?
//   For the versions to be removed, we need to be on a "dead-end" branch --
//   We separate this case by using ver_destroy instead of ver_release in the
//   version's decref. The difference is that ver_relase() can be used when we
//   wan't to GC the versions, but not the mappings in 

// this is common for all xvarray structures. We could embed it into struct sla,
// but we keep them seperate to be less intrusive.
struct xvarray_glbl {
	spinlock_t lock;
	ver_t      *v_base;
};

struct xvarray {
	xarray_t            *xarr;
	ver_t               *xv_ver;
	struct xvarray_glbl *glbl;
};

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

	verp_log_init(ver);
}

void
xvarray_print_vps(xvarray_t *xvarr)
{
	sla_node_t *node;
	unsigned i=0;
	sla_for_each(&xvarr->xarr->sla, 0, node) {
		printf("i=%4u\t%p\n", i++, node->chunk);
		vp_print(node->chunk);
	}
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
	ver_t *ver = xvarr->xv_ver;

	// check if there are children
	if (verp_log_nadds(ver) + 1 == ver_refcnt(ver)) {
		// no children:
		// collect versions from mappings based on the log
		verp_log_gc(ver, sla_chunk_free);
		assert(ver_refcnt(ver) == 1);
	} else {
		assert(verp_log_nadds(ver) + 1 < ver_refcnt(ver));
	}

	if (ver_putref(xvarr->xv_ver) == NULL) {
		// TODO: all references are gone
		// delete ->xarr?
		// we need to delete _glbl for certain though
	}
}

/**
 * xvarray_destroy() is called when a xvarray is not in use any more. It has to
 * The function needs to distinguish between two cases:
 *  - dead-ends are ->xv_vers that their mappings can be collected
 *  - live versions where we can't remove the mappings, because they are needed
 * We make this distinction by checking if the version has any children.
 * Note xvarray_destroy() works better if we make sure that _destroy() is called
 * first on the child and then on the parent.
 */
static inline void
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
	verp_log_init(xvarr->xv_ver);
}

xvarray_t *
xvarray_branch(xvarray_t *xvarr)
{
	xvarray_t *ret;
	ret = xmalloc(sizeof(xvarray_t));
	xvarray_do_branch(ret, xvarr);
	return ret;
}

static inline xelem_t const *
xvarray_getchunk_rd(xvarray_t *xvarr, long idx, size_t *nelems)
{
	sla_node_t *node;
	size_t chunk_off;
	size_t elem_size;
	void *data;

	elem_size = xvarr->xarr->elem_size;
	idx       = xarr_idx(xvarr->xarr, idx);
	node      = sla_find(&xvarr->xarr->sla, idx*elem_size, &chunk_off);
	data      = vp_ptr(node->chunk, xvarr->xv_ver);

	if (nelems) {
		size_t chunk_len = node->chunk_size - chunk_off;
		assert(chunk_len % elem_size == 0);
		*nelems = chunk_len / elem_size;
	}

	return (xelem_t const *)((char *)data + chunk_off);
}


// you can't change this pointer
static inline xelem_t const *
xvarray_get_rd(xvarray_t *xvarr, long idx)
{
	return xvarray_getchunk_rd(xvarr, idx, NULL);
}

static inline xelem_t *
xvarray_getchunk_rdwr(xvarray_t *xvarr, long idx, size_t *nelems)
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
	if (nelems) {
		size_t chunk_len = node->chunk_size - chunk_off;
		assert(chunk_len % elem_size == 0);
		*nelems = chunk_len / elem_size;
	}

	return (xelem_t *)((char *)chunk + chunk_off);
}

// you can change this pointer
static inline xelem_t *
xvarray_get_rdwr(xvarray_t *xvarr, long idx)
{
	return xvarray_getchunk_rdwr(xvarr, idx, NULL);
}

#endif /* XARRAY_MV_SLA_H */
