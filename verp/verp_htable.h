/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#ifndef VERP_HTABLE_H
#define VERP_HTABLE_H

#include "ver.h"
#include "verp.h"
#include "hash.h"
#include "misc.h"
#include "processor.h"

#if !defined(VERP_H)
#error "Don't include this file directly, use verp.h"
#endif

// ugly hack
#include"floorplan_stats.h"

/*
 * low-level version-to-pointer mapping
 *   (uses simple hash table)
 */

// use static size for now
#define VERP_HTABLE_BITS  15
#define VERP_HTABLE_SIZE  (1UL<<VERP_HTABLE_BITS)

struct verp_hnode {
	ver_t             *ver;
	void              *ptr;
	struct verp_hnode *next;
} __attribute__ ((aligned(CACHELINE_BYTES)));

struct verp_hmeta {
	spinlock_t _lock;
}  __attribute__ (( aligned(CACHELINE_BYTES) ));

/**
 * @htable: table with buckets
 * @hmeta:  metadata for each table (locks, etc)
 *   We use statically sized htables for now
 */
struct verp_map {
	struct verp_hnode *htable[VERP_HTABLE_SIZE];
	struct verp_hmeta  hmeta[VERP_HTABLE_SIZE];
	#if 0
	unsigned       hsize;
	unsigned       hbits;
	#endif
	atomic_t           nmaps;
};

static inline uint32_t
verpmap_size(struct verp_map *vmap)
{
	return atomic_read(&vmap->nmaps);
}

static inline void
verp_map_init(struct verp_map *vmap)
{
	for (size_t i=0; i < VERP_HTABLE_SIZE; i++) {
		vmap->htable[i] = NULL;
		spinlock_init(&vmap->hmeta[i]._lock);
	}
	atomic_set(&vmap->nmaps, 0);
}

static inline struct verp_hnode **
verpmap_getchain(struct verp_map *vmap, unsigned int bucket)
{
	spin_lock(&vmap->hmeta[bucket]._lock);
	return &vmap->htable[bucket];
}

static inline void
verpmap_putchain(struct verp_map *vmap, unsigned int bucket)
{
	spin_unlock(&vmap->hmeta[bucket]._lock);
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

	//FLOORPLAN_TIMER_START(verpmap_get);
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

	//FLOORPLAN_XCNT_ADD(verpmap_get_iters, cnt);
	//FLOORPLAN_TIMER_PAUSE(verpmap_get);
	return ret;
}

/**
 * remove a mapping.
 *  return VERP_NOTFOUND if it does not exist, or the mapping
 */
static inline void *
verpmap_remove(struct verp_map *vmap, ver_t *ver)
{
	struct verp_hnode **chain, *curr;
	unsigned int bucket;
	void *ret;

	//FLOORPLAN_TIMER_START(verpmap_remove);
	bucket = hash_ptr(ver, VERP_HTABLE_BITS);
	chain  = verpmap_getchain(vmap, bucket);
	ret    = VERP_NOTFOUND;
	while ( (curr = *chain) != NULL) {
		if (ver_eq(curr->ver, ver)) {
			*chain = curr->next;
			atomic_dec(&vmap->nmaps);
			break;
		}

		chain = &curr->next;
	}
	verpmap_putchain(vmap, bucket);

	if (curr) {
		ret = curr->ptr;
		ver_putref(curr->ver);
		free(curr);
	}

	//FLOORPLAN_TIMER_PAUSE(verpmap_remove);
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

	//FLOORPLAN_TIMER_START(verpmap_set);
	newn = xmalloc(sizeof(struct verp_hnode));
	newn->ver = ver_getref(ver);
	newn->ptr  = newp;

	bucket = hash_ptr(ver, VERP_HTABLE_BITS);
	chain = verpmap_getchain(vmap, bucket);
	newn->next = *chain;
	*chain     = newn;
	atomic_inc(&vmap->nmaps);
	verpmap_putchain(vmap, bucket);
	//FLOORPLAN_TIMER_PAUSE(verpmap_set);
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

	//FLOORPLAN_TIMER_START(verpmap_update);
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
	atomic_inc(&vmap->nmaps);

	newn->next = *chain;
	*chain     =  newn;

end:
	verpmap_putchain(vmap, bucket);
	//FLOORPLAN_TIMER_PAUSE(verpmap_update);
	return ret;
}

static inline void
verpmap_reset(struct verp_map *vmap)
{
	struct verp_hnode **chain, *curr;

	//FLOORPLAN_TIMER_START(verpmap_reset);
	for (unsigned i=0; i<VERP_HTABLE_SIZE; i++) {
		chain = verpmap_getchain(vmap, i);
		for (curr = *chain; curr; curr = curr->next)
			ver_putref(curr->ver);
		*chain = NULL;
		atomic_set(&vmap->nmaps, 0);
		verpmap_putchain(vmap, i);
	}
	//FLOORPLAN_TIMER_PAUSE(verpmap_reset);
}


#endif /* VERP_HTABLE_H */
