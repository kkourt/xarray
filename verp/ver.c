/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */


#include "refcnt.h"
#include "ver.h"
#include "misc.h"

#define VERS_MM

#if !defined(VERS_MM)
#error "NYI"
#endif

#if defined(VER_VREF)
// on free(), update (atomically) ver_seq_max = MAX(ver_seq_max, v->v_seq +1)
// Curently, we don't use the above since we dont free() versions
static uint64_t ver_seq_max = 0;
#endif

#if defined(VERS_MM)

static __thread struct {
	ver_t     *vers;      // chain of free versions
	size_t    vers_nr;    // number of free versions
} Ver_mm;
#endif // VERS_MM

ver_t *
ver_mm_alloc(void)
{
	ver_t *ret;
	#if defined(VERS_MM)
	if (Ver_mm.vers_nr == 0) {
		ret = xmalloc(sizeof(*ret));
		#if defined(VER_VREF)
		ret->v_seq = ver_seq_max;
		#endif
	} else {
		ret = Ver_mm.vers;
		Ver_mm.vers = ret->parent;
		Ver_mm.vers_nr--;
		#if defined(VER_VREF)
		ret->v_seq++;
		#endif
	}
	#else // !VERS_MM
	ret = xmalloc(sizeof(*ret));
	#endif

	return ret;
}

void
ver_mm_free(ver_t *ver)
{
	#if defined(VERS_MM)
	ver->parent = Ver_mm.vers;
	Ver_mm.vers = ver;
	Ver_mm.vers_nr++;
	#else // !VERS_MM
	free(ver);
	#endif
}

void
ver_debug_init(ver_t *ver)
{
	#ifndef NDEBUG
	static size_t id = 0;
	spinlock_t *lock_ptr = NULL;
	spinlock_t lock;

	if (lock_ptr == NULL) { // XXX: race
		spinlock_init(&lock);
		lock_ptr = &lock;
	}
	spin_lock(lock_ptr);
	ver->v_id = id++;
	spin_unlock(lock_ptr);
	#endif
}

void
ver_chain_print(ver_t *ver)
{
	printf("=== Printing chain ============================== %p\n", ver);
	while (ver != NULL) {
		printf(" %s\n", ver_fullstr(ver));
		ver = ver->parent;
	}
	printf("=========================================================\n");
}
