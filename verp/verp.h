#ifndef VERP_H
#define VERP_H

#include <stdint.h>
#include <pthread.h>

#include "ver.h"

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

verp_t *verp_allocate(unsigned int bits);

#define VERP_NOTFOUND ((void *)(-1))

void *verp_find_ptr_exact(verp_t *verp, ver_t *ver);
void *verp_find_ptr(verp_t *verp, ver_t *ver, ver_t **ver_found);
void  verp_insert_ptr(verp_t *verp, ver_t *ver, void *newp);
void *verp_update_ptr(verp_t *verp, ver_t *ver, void *newp);
void  verp_gc(verp_t *verp, ver_t *base);

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

// mapping from versions to actual pointers to implement fat nodes
struct verp_map;

// verp_map using hash table
#include "verp_htable.h"

/* versioned pointer structure */
struct verp {
	struct verp_map vpmap;
	struct verp     *__cache_next;
};


#endif /* VERP_H */
