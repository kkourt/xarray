#ifndef VERP_HTABLE_H
#define VERP_HTABLE_H

#include "ver.h"
#include "verp.h"
#include "hash.h"
#include "misc.h"

#if !defined(VERP_H)
#error "Don't include this file directly, use verp.h"
#endif

/*
 * low-level version-to-pointer mapping
 *   (uses simple hash table)
 */

// use static size for now
#define VERP_HTABLE_BITS  4
#define VERP_HTABLE_SIZE  (1UL<<VERP_HTABLE_BITS)

struct verp_hnode {
	ver_t             *ver;
	void              *ptr;
	struct verp_hnode *next;
} __attribute__ ((aligned(CACHELINE_BYTES)));

struct verp_hlock {
	spinlock_t _lock;
}  __attribute__ (( aligned(CACHELINE_BYTES) ));

/**
 * @htable: table with buckets
 * @hlocks: one lock per table
 *   We use statically sized htables for now
 */
struct verp_map {
	struct verp_hnode *htable[VERP_HTABLE_SIZE];
	struct verp_hlock  hlocks[VERP_HTABLE_SIZE];
	#if 0
	unsigned       hsize;
	unsigned       hbits;
	#endif
};

static inline void
verp_map_init(struct verp_map *vmap)
{
	for (size_t i=0; i < VERP_HTABLE_SIZE; i++) {
		vmap->htable[i] = NULL;
		spinlock_init(&vmap->hlocks[i]._lock);
	}
}

static inline struct verp_hnode **
verpmap_getchain(struct verp_map *vmap, unsigned int bucket)
{
	spin_lock(&vmap->hlocks[bucket]._lock);
	return &vmap->htable[bucket];
}

static inline void
verpmap_putchain(struct verp_map *vmap, unsigned int bucket)
{
	spin_unlock(&vmap->hlocks[bucket]._lock);
}

static inline void
verpmap_print(struct verp_map *vmap)
{
	struct verp_hnode **chain, *curr;

	for (size_t i=0; i < VERP_HTABLE_SIZE; i++) {
		chain = verpmap_getchain(vmap, i);
		for (curr = *chain; curr; curr = curr->next) {
			printf("  ver:%s ptr:%p\n",
			        ver_str(curr->ver), curr->ptr);
		}
		verpmap_putchain(vmap, i);
	}
}

/**
 * get the pointer stored for @ver
 *   returns VERP_NOTFOUND if @ver does not exist
 */
static inline void *
verpmap_get(struct verp_map *vmap, ver_t *ver)
{
	struct verp_hnode **chain, *curr;
	unsigned int bucket;
	void *ret;

	bucket = hash_ptr(ver, VERP_HTABLE_BITS);
	chain  = verpmap_getchain(vmap, bucket);
	ret    = VERP_NOTFOUND;
	for (curr = *chain; curr; curr = curr->next) {
		if (ver_eq(curr->ver, ver)) {
			ret = curr->ptr;
			break;
		}
	}
	verpmap_putchain(vmap, bucket);
	return ret;
}


/**
 * sets a mapping from @ver to @newp
 *   Assumes that no similar mapping exists
 *   grabs a reference for @ver
 */
static inline void
verpmap_set(struct verp_map *vmap, ver_t *ver, void *newp)
{
	struct verp_hnode **chain, *newn;
	unsigned int bucket;

	newn = xmalloc(sizeof(struct verp_hnode));
	newn->ver = ver_getref(ver);
	newn->ptr  = newp;

	bucket = hash_ptr(ver, VERP_HTABLE_BITS);
	chain = verpmap_getchain(vmap, bucket);
	newn->next = *chain;
	*chain     = newn;
	verpmap_putchain(vmap, bucket);
}


/**
 * update a mapping
 *   If a mapping for @ver does not exist, then set a new mapping grabbing a
 *   reference for @ver. Return VERP_NOTFOUND.
 *   If a mapping does exist, just change it and return the old pointer.
 */
static inline void *
verpmap_update(struct verp_map *vmap, ver_t *ver, void *newp)
{
	struct verp_hnode **chain, *curr, *newn;
	unsigned int bucket;
	void *ret;

	bucket = hash_ptr(ver, VERP_HTABLE_BITS);
	chain = verpmap_getchain(vmap, bucket);
	// check if version already exists
	for (curr=*chain; curr; curr = curr->next) {
		if (ver_eq(curr->ver, ver)) {
			ret = curr->ptr;
			curr->ptr = newp;
			goto end;
		}
	}

	// version does not exist, allocate node and insert it
	ret = VERP_NOTFOUND;
	newn = xmalloc(sizeof(struct verp_hnode));
	newn->ver = ver_getref(ver);
	newn->ptr = newp;

	newn->next = *chain;
	*chain     =  newn;

end:
	verpmap_putchain(vmap, bucket);
	return ret;
}

static inline void
verpmap_reset(struct verp_map *vmap)
{
	struct verp_hnode **chain, *curr;

	for (unsigned i=0; i<VERP_HTABLE_SIZE; i++) {
		chain = verpmap_getchain(vmap, i);
		for (curr = *chain; curr; curr = curr->next)
			ver_putref(curr->ver);
		*chain = NULL;
		verpmap_putchain(vmap, i);
	}
}


#endif /* VERP_HTABLE_H */
