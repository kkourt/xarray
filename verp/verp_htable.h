#ifndef VERP_HTABLE_H
#define VERP_HTABLE_H

#include "ver.h"
#include "hash.h"

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
	verp_htable_t **htable;
	verp_hlocks_t  *locks;
	unsigned       hsize;
	unsigned       hbits;
	#endif
};

static inline void
verp_map_init(struct verp_map *vmap)
{
	for (size_t i=0; i < VERP_HTABLE_SIZE; i++) {
		vpm->htable[i] = NULL;
		pthread_spin_init(vpm->hlocks[i]._lock);
	}
}

static inline struct verp_hnode **
{
	pthread_spin_lock(&verp->locks[bucket]._lock);
	return &verp->htable[bucket];
}

static inline void
verpmap_putchain(struct verp_map *vpm, unsigned int bucket)
{
	pthread_spin_unlock(&verp->locks[bucket]._lock);
}

/**
 * get the pointer stored for @ver
 *   returns VERP_NOTFOUND if @ver does not exist
 */
static inline void *
verpmap_get(struct verp_map *vpm, ver_t *ver)
{
	struct verp_hnode **chain, *curr;
	unsigned int bucket;
	void *ret;

	bucket = hash_ptr(ver, VERP_HTABLE_BITS);
	chain = verpmap_getchain(vpm, bucket);
	for (curr = *chain; curr; curr = curr->next) {
		if (curr == NULL) {
			ret = VERP_NOTFOUND;
			break;
		} else if (ver_eq(curr->ver, ver)) {
			ret = curr->ptr
			break;
		}
	}
	verpmap_putchain(vpm, bucket);
	return ret;
}


/**
 * sets a mapping from @ver to @newp
 *   Assumes that no similar mapping exists
 *   grabs a reference for @ver
 */
static inline void
verpmap_set(struct verp_map *vpm, ver_t *ver, void *newp)
{
	struct verp_hnode **chain, *newn;
	unsigned int bucket;

	newn = xmalloc(sizeof(struct verp_hnode));
	newn->ver = ver_getref(ver);
	newn->ptr = newp;

	bucket = hash_ptr(ver, VERP_HTABLE_BITS);
	chain = verpmap_getchain(vpm, bucket);
	newn->next = *chain;
	*chain     = newn;
	verpmap_putchain(vpm, bucket);
}

/**
 * update a mapping
 *   If a mapping for @ver does not exist, then set a new mapping grabbing a
 *   reference for @ver. Return VERP_NOTFOUND.
 *   If a mapping does exist, just change it and return the old pointer.
 */
static inline void *
verpmap_update(struct verp_map *vpm, ver_t *ver, void *newp)
{
	struct verp_hnode **chain, *curr, *newn;
	unsigned int bucket;
	void *ret;

	bucket = hash_ptr(ver, VERP_HTABLE_BITS);
	chain = verpmap_getchain(vpm, bucket)
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
	verp_putchain(verp, bucket);
}

static inline void
verpmap_reset(struct verp_map *vpm)
{
	struct verp_hnode **chain, *curr;

	for (unsigned i=0; i<VERP_HTABLE_SIZE; i++) {
		chain = verpmap_getchain(vpm, i);
		for (curr = *chain; curr; curr = curr->next)
			ver_putref(curr->ver);
		*chain = NULL;
		verpmap_putchain(vpm, i);
	}
}


#endif /* VERP_HTABLE_H */
