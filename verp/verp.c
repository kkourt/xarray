#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "verp.h"

#include "hash.h"
#include "misc.h"

// ugly hack
#include "floorplan_stats.h"

// Cache for verp objects
static __thread struct {
	struct verp    *verp_list;
	size_t          verp_nr;
} __attribute__((aligned(128))) VerpCache = {0};


void
verp_mm_init(void)
{

	// prealloc here
}

struct verp *
verp_alloc(void)
{
	struct verp *ret;

	if (VerpCache.verp_nr > 0) {
		ret = VerpCache.verp_list;
		VerpCache.verp_list = ret->__cache_next;
		VerpCache.verp_nr--;
	} else {
		ret = xmalloc(sizeof(struct verp));
		verp_map_init(&ret->vpmap);
	}

	return ret;
}

void
verp_dealloc(struct verp *verp)
{
	verp->__cache_next = VerpCache.verp_list;
	VerpCache.verp_list = verp;
	VerpCache.verp_nr++;
}


void
verp_print(struct verp *verp)
{
	printf("verp: %p\n", verp);
	verpmap_print(&verp->vpmap);
}

void *
verp_find_ptr_exact(verp_t *verp, ver_t *ver)
{
	return verpmap_get(&verp->vpmap, ver);
}

/* return a pointer given a versioned pointer and a version */
void *
verp_find_ptr(verp_t *verp, ver_t *ver, ver_t **ver_found)
{
	ver_t *v = ver;
	void *ptr, *ret;
	//printf("\t  Searching for ptr=%p ver=%p\n", verp, ver);
	ret = VERP_NOTFOUND;
	FLOORPLAN_TIMER_START(verp_find_ptr);
	size_t cnt = 0;
	while (v != NULL) {
		ptr = verpmap_get(&verp->vpmap, v);
		if (ptr != VERP_NOTFOUND) {
			if (ver_found)
				*ver_found = v;
			ret = ptr;
			break;
		}
		v = v->parent;
		cnt++;
	}
	FLOORPLAN_XCNT_ADD(verp_find_ptr_iters, cnt);
	FLOORPLAN_XCNT_ADD(verpmap_size, verpmap_size(&verp->vpmap));
	FLOORPLAN_TIMER_PAUSE(verp_find_ptr);
	return ret;
}

/* add a new version to the versioned pointer */
void
verp_insert_ptr(verp_t *verp, ver_t *ver, void *newp)
{
	assert(newp != VERP_NOTFOUND);
	assert(verp_find_ptr_exact(verp, ver) == VERP_NOTFOUND);
	verp_log_add(ver, verp);
	verpmap_set(&verp->vpmap, ver, newp);
}

/* update a version of the versioned pointer. Return the old pointer if existed,
 * or NULL */
void *
verp_update_ptr(verp_t *verp, ver_t *ver, void *newp)
{
	void *ret;

	assert(newp != VERP_NOTFOUND);
	ret = verpmap_update(&verp->vpmap, ver, newp);
	if (ret == NULL)
		verp_log_add(ver, verp);
	return ret;
}

void
verp_gc(verp_t *verp, ver_t *base)
{
	// garbage collect versions <base

	// get all pointers that are <base
	// reclaim verp_vp_nodes
	// ->ptr_dealloc()
}


/* return the pointer of the given version */
void *
vp_ptr(void *vp, ver_t *ver)
{
	if (vp_is_normal_ptr(vp))
		return vp;

	verp_t *verp = vp_unmark_ptr(vp);
	return verp_find_ptr(verp, ver, NULL);
}

/* update the versioned pointer with a new value */
void
vp_update(verobj_t *obj, void **vp_pointer, ver_t *ver, void *newp)
{
	verp_t *verp;

	if (vp_is_normal_ptr(*vp_pointer)) {
		if (ver == obj->ver_base) {
			// save version, just update the value
			obj->ptr_dealloc(*vp_pointer);
			*vp_pointer = newp;
			return;
		}
		// we need a new verp
		verp = verp_alloc();
		// insert normal pointer as base version
		// XXX: maybe s/update/inserT?
		verp_update_ptr(verp, obj->ver_base, *vp_pointer);
	} else {
		verp = vp_unmark_ptr(*vp_pointer);
		// garbage collect older versions
		verp_gc(verp, obj->ver_base);
	}

	// we now have a verp objet that we need to update
	void *old_ptr = verp_update_ptr(verp, ver, newp);
	if (old_ptr)
		obj->ptr_dealloc(old_ptr);

	*vp_pointer = verp_mark_ptr(verp);
	return;
}


#ifdef VERP_TEST
#include <string.h>
#include "tsc.h"

static void
xptr_dealloc(void *xptr)
{
}

int
main(int argc, const char *argv[])
{
	if (argc < 3) {
		fprintf(stderr,
		        "Usage: %s <array_size> <block_size> <accesses>\n",
			argv[0]);
		exit(1);
	}

	unsigned int asize = atol(argv[1]);
	unsigned int bsize = atol(argv[2]);
	unsigned int accesses = atol(argv[3]);
	const unsigned int loops = 128;
	unsigned int blocks = asize / bsize;
	unsigned int baccesses = accesses / bsize;
	assert(blocks*bsize == asize);
	assert(baccesses*bsize == accesses);
	assert(asize >= accesses);

	tsc_t tc;
	/* normal pointers */
	printf("CoPy\n");
	tsc_init(&tc);
	tsc_start(&tc);
	unsigned int *p[loops + 1];
	p[0] = xmalloc(asize*sizeof(int));
	for (unsigned int i=0; i<asize; i++)
		p[0][i] = loops;

	for (unsigned int i=0; i<loops; i++) {
		unsigned int *newp = p[i+1] = xmalloc(asize*sizeof(int));
		unsigned int *oldp = p[i];
		for (unsigned int j=0; j<accesses; j++) {
			newp[j] = oldp[j] - 1;
		}
		memcpy(newp + accesses, oldp + accesses, asize-accesses);
	}
	tsc_pause(&tc);
	tsc_report(&tc);

	/* versioned pointers */
	tsc_t t;
	printf("VerSions\n");
	tsc_init(&t);
	tsc_start(&t);
	ver_t *vers[loops + 1];
	verobj_t vobj;
	vobj.ptr_dealloc = xptr_dealloc;
	vobj.ver_base = vers[0] = ver_alloc(NULL);
	//printf("Version 0 is %p\n", vers[0]);
	unsigned int *vptr[blocks];
	for (unsigned int i=0; i<blocks; i++) {
		vptr[i] = xmalloc(bsize*sizeof(int));
		for (unsigned int j=0; j<bsize; j++) {
			vptr[i][j] = loops;
		}
	}
	for (unsigned int i=0; i<loops; i++) {
		ver_t *newv = vers[i+1] = ver_alloc(vers[i]);
		ver_t *oldv = vers[i];
		//printf("i=%d newver=%p oldver=%p\n", i, newv, oldv);
		for (unsigned int b=0; b<baccesses; b++) {
			//printf("\tb=%d\n", b);
			unsigned int *newp = xmalloc(bsize*sizeof(int));
			unsigned int *oldp = vp_ptr(vptr[b], oldv);
			for (unsigned int k=0; k<bsize; k++) {
				*newp = *oldp - 1;
				//printf("\t    %d = %d -1\n", *newp, *oldp);
			}
			vp_update(&vobj, (void **)&vptr[b], newv, newp);
		}
	}
	tsc_pause(&t);
	tsc_report(&t);

	printf("\ntC/tV=%lf\n", (double)tsc_getticks(&tc)/(double)tsc_getticks(&t));
	/* check */
	ver_t *lastv = vers[loops];
	unsigned int idx=0;
	for (unsigned int b=0; b<baccesses; b++) {
		unsigned int *vp = vp_ptr(vptr[b], lastv);
		for (unsigned int i=0; i<bsize; i++) {
			if (vp[i] != p[loops][idx]) {
				printf("Error: vp[i]=%d p[loops][idx]=%d idx=%d p=%p\n",
				        vp[i], p[loops][idx], idx, &p[loops][idx]);
				assert(0);
			}
			idx++;
		}
	}
	return 0;
}
#endif
