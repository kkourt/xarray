#ifndef VERP_H
#define VERP_H

#include <stdint.h>
#include <pthread.h>

#include "ver.h"

/**
 * Notes on GC
 *
 * A version should be destroyed if is not needed any more. To destroy the
 * version, we decrement the refcount of @xv_ver. If the refcount reaches to
 * zero, it means that the version is no longer used and we can reclaim it.
 *
 * There are three types of objects that can reference a version:
 *   - other versions via ->parent pointer
 *   - versioned objects (e.g., xvarray)
 *   - mappings in VERPs
 *
 * An eager way to do that is to maintain a log on versions with the VERP that
 * contain mappings for the version. When a version is released, we can iterate
 * the VERP and remove  the mappings. A lazy way is described below.
 *
 * Forgetting the alredy destroyed versions for a moment, if we keep branching
 * off we will end up with a long chain of versions, which waste space. To
 * collect those we use a mechanism called the base version. The user defines a
 * base version, and essentially detaches the descendant chain from the base
 * version. The descendant chain can now be collected (note that if the user can
 * guarantee that no references to other versions exist this can be done without
 * iterating the chain).
 *
 * We can use the base version to do a lazy and less strict collection of
 * mappings in VERPs. If we go beyond the base version when looking for a valid
 * mapping -- we can destroy all the mappings and keep just one: the pointer we
 * eventually found mapped in the base version.
 */

/* versioned object structure
 *  We only need to keep versions that are >@ver_base in the partial order
 *  @ptr_dealloc is called to deallocate the object pointed by the pointer
 */
struct verobj {
	ver_t *ver_base;
	void  (*ptr_dealloc)(void *);
};
typedef struct verobj verobj_t;

/*
 *  VERSIONED POINTERS
 *
 */

struct verp;
typedef struct verp verp_t;

verp_t *verp_alloc();

#define VERP_NOTFOUND ((void *)(-1))

void *verp_find_ptr_exact(verp_t *verp, ver_t *ver);
void *verp_find_ptr(verp_t *verp, ver_t *ver, ver_t **ver_found);
void  verp_insert_ptr(verp_t *verp, ver_t *ver, void *newp);
void *verp_update_ptr(verp_t *verp, ver_t *ver, void *newp);
void  verp_gc(verp_t *verp, ver_t *base);
void verp_print(struct verp *verp);

/**
 * VERP LOG
 */

/**
 * We use the log to do eager garbage collection on the verp_t entries that are
 * part of this version.
 * ver->v_log is:
 *   v_log[0]:             Number of entries
 *   v_log[1]...v_log[-2]: Entries
 *   v_log[-1]:            ->next pointer (last)
 */
#define VERP_INLINE_LOG_SIZE (V_LOG_UINTPTRS-2)

// Is the log complete?

static inline void
verp_log_init(ver_t *v)
{
	v->v_log[0] = 0;
}

static inline size_t
verp_log_nadds(ver_t *v)
{
	return v->v_log[0];
}

static inline void
verp_log_add(ver_t *v, verp_t *verp)
{
	size_t size;

	size = v->v_log[0];
	v->v_log[0] += 1;
	// If we don't have enough space we can either allocate more or ignore
	// the additions.
	if (size == VERP_INLINE_LOG_SIZE) {
		NYI();
	}

	// update log
	size_t i = size + 1;
	v->v_log[i] = (uintptr_t)verp;
}

/**
 * VPs
 */

/**
 * To save  space and avoid excesive cache polution, we don't use versioned
 * pointer trees for all pointers:
 *  - if MSB is not set, we just return the pointer
 *  - if MSB is set, we clear the bit and we find the pointer by searching
 *    through the versions.
 * I'll try to use this notation:
 *  vp   -> pointers that are possibly marked, but we don't know for sure
 *  verp -> versioned pointers of verp_t *
 * We assume a 64-bit arch */
#ifndef __x86_64__
#error "we assume 64 bit pointers"
#endif
#define VERP_MARK_BIT 63
#define VERP_MARK_MASK (1UL<<VERP_MARK_BIT)

static inline int
vp_is_marked_ptr(void *vp)
{
	return !!((uintptr_t)vp & VERP_MARK_MASK);
}

static inline int
vp_is_normal_ptr(void *vp)
{
	return !vp_is_marked_ptr(vp);
}

static inline verp_t *
vp_unmark_ptr(void *vp)
{
	assert(!vp_is_normal_ptr(vp));
	void *ret = (void *)((uintptr_t)vp & ~VERP_MARK_MASK);
	return ret;
}

static inline void *
verp_mark_ptr(verp_t *verp)
{
	void *ret = (void *)((uintptr_t)verp | VERP_MARK_MASK);
	return ret;
}

void *vp_ptr(void *vp, ver_t *ver);
void vp_update(verobj_t *obj, void **vp_ptr, ver_t *ver, void *newp);

static inline void
vp_print(void *vp)
{
	if (vp_is_marked_ptr(vp)) {
		verp_t *verp = vp_unmark_ptr(vp);
		verp_print(verp);
	} else {
		printf("vp:%p\n", vp);
	}
}

// mapping from versions to actual pointers to implement fat nodes
struct verp_map;

// verp_map using hash table
#include "verp_htable.h"

/* versioned pointer structure */
struct verp {
	struct verp_map vpmap;
	struct verp     *__cache_next;
};

static inline void
verp_log_gc(ver_t *ver, void  (*ptr_dealloc)(void *))
{
	size_t size;

	size = ver->v_log[0];
	if (size > VERP_INLINE_LOG_SIZE) {
		NYI();
	}

	for (unsigned i = 1; i <= size; i++) {
		verp_t *verp;
		void   *ptr;

		verp = (verp_t *)ver->v_log[i];
		ptr = verpmap_remove(&verp->vpmap, ver);
		if (ptr == VERP_NOTFOUND) {
			fprintf(stderr, "BUG\n");
			abort();
		}

		ptr_dealloc(ptr);
	}
}


#endif /* VERP_H */
