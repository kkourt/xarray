#ifndef VER_H__
#define VER_H__

#include <stdbool.h>
#include <inttypes.h>

#include "refcnt.h"
#include "container_of.h"
#include "misc.h"

// Versions form a tree (partial order) as defined by the ->parent pointer.

// We use a ->parent pointer to track the partial order to enable for
// concurrency (instead of using children pointers)

/**
 * Garbage collecting versions
 *
 * versions are referenced by:
 *  - other versions (ver_t's ->parent)
 *  - mappings from versions to pointers in verp_map
 *
 *     ...
 *      |
 *      o
 *      |
 *      o Vj
 *      |\
 *      . .
 *      .  .
 *      .   .
 *      |    \
 *   Vg o     o Vp
 *
 * Since children point to parents, we can't use typical reference counting,
 * because reference counts will never reach zero. Instead, we need to collect
 * versions which form a chain that reaches the end (NULL) and all refcounts in
 * the chain are 1.  However, this might result in collecting all the chain,
 * which is not what we want.  Hence, we use a pin operation to pin a specific
 * version to the chain. GC guarantees that it won't try to collect versions
 * from the pinned version and beyond, while the user guarantees that it won't
 * try to branch off versions below the pinned version.
 */

/**
 * Interface overview
 *
 * ver_create():    create a new version out of nothing
 * ver_branch():    branch a version from another version
 *
 * ver_getref():    get a version reference
 * ver_putref():    put a version reference
 *
 * ver_pin():       pin a version
 *
 * ver_eq():        check two versions for equality
 * ver_leq_limit(): query partial order
 * ver_join():      find join point
 *
 */

#define V_LOG_UINTPTRS 8
struct ver {
	struct ver *parent;
	#ifndef NDEBUG
	size_t     v_id;
	#endif

	uint64_t   v_seq;
	refcnt_t   rfcnt;

	// this is to be used by the user of ver.h
	uintptr_t  v_log[V_LOG_UINTPTRS];
};
typedef struct ver ver_t;

ver_t *ver_mm_alloc(void);
void   ver_mm_free(ver_t *ver);

void ver_debug_init(ver_t *ver);
void ver_chain_print(ver_t *ver);

static inline void
ver_init__(ver_t *ver)
{
	refcnt_init(&ver->rfcnt, 1);

	#ifndef NDEBUG
	ver_debug_init(ver);
	#endif
}

static inline char *
ver_str(ver_t *ver)
{
	#define VERSTR_BUFF_SIZE 128
	#define VERSTR_BUFFS_NR   16
	static int i=0;
	static char buff_arr[VERSTR_BUFFS_NR][VERSTR_BUFF_SIZE];
	char *buff = buff_arr[i++ % VERSTR_BUFFS_NR];
	#ifndef NDEBUG
	/*
	snprintf(buff, VERSTR_BUFF_SIZE,
	         " [%p: ver:%3zd rfcnt_children:%3u rfcnt_total:%3u] ",
		 ver,
	         ver->v_id,
		 refcnt_get(&ver->rfcnt_children),
		 refcnt_get(&ver->rfcnt_total));
	*/
	snprintf(buff, VERSTR_BUFF_SIZE, " [ver:%3zd] ", ver->v_id);
	#else
	snprintf(buff, VERSTR_BUFF_SIZE, " (ver:%p ) ", ver);
	#endif
	return buff;
	#undef VERSTR_BUFF_SIZE
	#undef VERSTR_BUFFS_NR
}

static inline char *
ver_fullstr(ver_t *ver)
{
	#define VERSTR_BUFF_SIZE 128
	#define VERSTR_BUFFS_NR   16

	static int i=0;
	static char buff_arr[VERSTR_BUFFS_NR][VERSTR_BUFF_SIZE];
	char *buff = buff_arr[i++ % VERSTR_BUFFS_NR];

	#ifndef NDEBUG
	snprintf(buff, VERSTR_BUFF_SIZE,
	         " [%p: ver:%3zd rfcnt:%3u] ",
		 ver,
	         ver->v_id,
		 refcnt_get(&ver->rfcnt));
	#endif // !NDEBUG

	return buff;

	#undef VERSTR_BUFF_SIZE
	#undef VERSTR_BUFFS_NR
}

static inline void
ver_path_print(ver_t *v, FILE *fp)
{
	fprintf(fp, "ver path: ");
	do {
		fprintf(fp, "%s ->", ver_str(v));
	} while ((v = v->parent) != NULL);
	fprintf(fp, "NULL\n");
}

static inline ver_t *
refcnt2ver(refcnt_t *rcnt)
{
	return container_of(rcnt, ver_t, rfcnt);
}

/**
 * get a node reference
 */
static inline ver_t *
ver_getref(ver_t *ver)
{
	refcnt_inc(&ver->rfcnt);
	return ver;
}

static void *ver_release(refcnt_t *);

/**
 * release a node reference
 *  returns the last version that was not released
 *  Note that if everything is released, this returns NULL
 */
static inline void *
ver_putref(ver_t *ver)
{
	void *ret;
	ret = refcnt_dec(&ver->rfcnt, ver_release);
	if (ret == (void *)-1) {
		ret = ver;
	}
	return ret;
}

/**
 * release a version
 */
static void *
ver_release(refcnt_t *refcnt)
{
	void  *ret;
	ver_t *ver = refcnt2ver(refcnt);
	#if 0
	// this is special case where a version is no longer references in a
	// tree, but is a part of the version tree. I think the best solution is
	// to have the tree grab a reference. Want to test the current gc sceme
	// before applying this change though, so for know just a warning.
	if (refcnt_get(&ver->rfcnt_children) != 0) {
		//assert(0 && "FIXME: need to handle this case");
		//printf("We are gonna leak one version\n");
		return;
	}
	#endif

	ver_t *parent = ver->parent;
	if (ver->parent != NULL) {
		ret = ver_putref(parent);
	} else {
		ret = NULL;
	}

	ver_mm_free(ver);
	return ret;
}

/* create a new version */
static inline ver_t *
ver_create(void)
{
	ver_t *ret = ver_mm_alloc();
	ret->parent = NULL;
	ver_init__(ret);
	return ret;
}


/**
 * garbage collect the tree from @ver's parent and above
 *  Find the longer chain that ends at NULL and all nodes have a rfcnt_children
 *  of 1. This chain can be detached from the version tree.
 *
 * This function is not reentrant. The caller needs to make sure that it won't
 * be run on the same chain.
 */
static void inline
ver_tree_gc(ver_t *ver)
{
	//VBPT_START_TIMER(ver_tree_gc);
	ver_t *ver_p = ver->parent;
	#if defined(VBPT_STATS)
	uint64_t count = 0;
	#endif
	while (true) {
		// reached bottom
		if (ver_p == NULL)
			break;

		uint32_t children;
		#if 0
		// try to get a the refcount. If it's not possible somebody else
		// is using the reference count lock (is that possible?)
		if (!refcnt_try_get(&ver_p->rfcnt_children, &children)) {
			VBPT_STOP_TIMER(ver_tree_gc);
			goto end;
		}
		#endif

		// Semantically, this number might not be the number of
		// children, since someone might hold a referrence to this
		// version. However, the effect should be the same
		children = refcnt_(&ver_p->rfcnt);
		assert(children > 0);

		// found a branch, reset the head of the chain
		if (children > 1)
			ver = ver_p;

		ver_p = ver_p->parent;
		#if defined(VBPT_STATS)
		count++;
		#endif
	}
	//VBPT_STOP_TIMER(ver_tree_gc);
	//VBPT_XCNT_ADD(ver_tree_gc_iters, count);
	//tmsg("count=%lu ver->parent=%p\n", count, ver->parent);

	// do it lazily?
	// maintain a chain and cal ver_putref() on allocation
	ver_t *v = ver->parent;
	while (v != NULL) {
		ver_t *tmp = v->parent;
		v->parent = NULL;       // remove @v from chain
		ver_putref(v);
		v = tmp;
	}

	// everything below ver->parent is stale, remove them from the tree
	// ASSUMPTION: this assignment is atomic.
	ver->parent = NULL;
}


/**
 * pin a version from the tree:
 *  just take/drop a reference
 *  See comment at begining of file for details
 */
static inline void
ver_pin(ver_t *pinned_new, ver_t *pinned_old)
{
	ver_getref(pinned_new);
	if (pinned_old) {
		ver_putref(pinned_old);
	}

	// Since we require serialization for running ver_tree_gc(), we do not
	// run it here. Instead, we leave it up to the caller to run it while
	// making sure that only one ver_tree_gc() runs at any given time.
	// Note, however, that pinning essentially marks pinned_old as eligible
	// for garbage collection.
	//ver_tree_gc(pinned_new);
}

static inline void
ver_unpin(ver_t *ver)
{
	ver_putref(ver);
}


/**
 * set parent without checking for previous parent
 */
static inline void
ver_setparent__(ver_t *v, ver_t *parent)
{
	ver_getref(parent);
	v->parent = parent;
}

/**
 * detach: detach version from the chain
 *   sets parent to NULL
 */
static inline void
ver_detach(ver_t *ver)
{
	if (ver->parent) {
		ver_putref(ver->parent);
		//ver_tree_gc(ver);
	}
	ver->parent = NULL;
}

/* branch (i.e., fork) a version */
static inline ver_t *
ver_branch(ver_t *parent)
{
	/* allocate and initialize new version */
	ver_t *ret = ver_mm_alloc();
	ver_init__(ret);
	/* increase the reference count of the parent */
	ver_setparent__(ret, parent);
	return ret;
}

/*
 * versions form a partial order, based on the version tree that arises from
 * ->parent: v1 < v2 iff v1 is an ancestor of v2
 */

static inline bool
ver_eq(ver_t *ver1, ver_t *ver2)
{
	return ver1 == ver2;
}

/**
 * check if ver1 <= ver2 -- i.e., if ver1 is ancestor of ver2 or ver1 == ver2
 *  moves upwards (to parents) from @ver2, until it encounters @ver1 or NULL
 */
static inline bool
ver_leq(ver_t *ver1, ver_t *ver2)
{
	for (ver_t *v = ver2; v != NULL; v = v->parent)
		if (v == ver1)
			return true;
	return false;
}

/**
 * check if @v_p is an ancestor of v_ch.
 *   if @v_p == @v_ch the function returns true
 */
static inline bool
ver_ancestor(ver_t *v_p, ver_t *v_ch)
{
	for (ver_t *v = v_ch; v != NULL; v = v->parent) {
		if (v == v_p)
			return true;
	}
	return false;
}

/**
 * check if @v_p is an ancestor of @v_ch, assuming they have no more of @max_d
 * distance.
 *  if @v_p == @v_ch the function returns true
 */
static inline bool
ver_ancestor_limit(ver_t *v_p, ver_t *v_ch, uint16_t max_d)
{
	ver_t *v = v_ch;
	for (uint16_t i=0; v != NULL && i < max_d + 1; v = v->parent, i++) {
		if (v == v_p)
			return true;
	}
	return false;
}

/**
 * check if @v_p is a strict ancestor of @v_ch
 *   if @v_p == @v_ch, the function returns false
 */
static inline bool
ver_ancestor_strict(ver_t *v_p, ver_t *v_ch)
{
	for (ver_t *v = v_ch->parent; v != NULL; v = v->parent) {
		if (v == v_p)
			return true;
	}
	return false;
}

/* check if @v_p is an ancestor of @v_ch, assuming they have no more of @max_d
 * distance.
 *  if @v_p == @v_ch the function returns false
 */
static inline bool
ver_ancestor_strict_limit(ver_t *v_p, ver_t *v_ch, uint16_t max_d)
{
	if (v_p == v_ch)
		return false;
	ver_t *v = v_ch->parent;
	for (uint16_t i=0; v != NULL && i < max_d; v = v->parent, i++) {
		if (v == v_p)
			return true;
	}
	return false;
}

#define VER_JOIN_FAIL ((ver_t *)(~((uintptr_t)0)))
#define VER_JOIN_LIMIT 64

static inline ver_t *
ver_parent(ver_t *ver)
{
	return ver->parent;
}

/**
 * Version references.
 */

// a reference to a version
struct vref {
	struct ver *ver_;
	uint64_t    ver_seq;
	#ifndef NDEBUG
	size_t      vid;
	#endif
};
typedef struct vref vref_t;

static inline vref_t
vref_get(ver_t *ver)
{
	vref_t ret;
	ret.ver_    = ver;
	ret.ver_seq = ver->v_seq;
	#if !defined(NDEBUG)
	ret.vid = ver->v_id;
	#endif
	return ret;
}

static inline void
vref_put(vref_t vref)
{
}

static inline bool
vref_eq(vref_t vref1, vref_t vref2)
{
	bool ret = (vref1.ver_ == vref2.ver_)
	           && (vref1.ver_seq == vref2.ver_seq);
	return ret;
}

enum {
	VREF_CMP_EQ,      // equal
	VREF_CMP_NEQ,     // not equal: ->ver does not match
	VREF_CMP_INVALID  // invalid ->ver matches, but has older seq number
};
static inline int
vref_cmpver(vref_t vref, ver_t *ver)
{
	if (vref.ver_ != ver)
		return VREF_CMP_EQ;
	if (vref.ver_seq != ver->v_seq)
		return VREF_CMP_INVALID;

	return VREF_CMP_EQ;
}

static inline bool
vref_eqver(vref_t vref, ver_t *ver)
{
	bool ret = (vref.ver_ == ver)
	           && (vref.ver_seq == ver->v_seq);
	return ret;
}

static inline char *
vref_str(vref_t vref)
{
	#define VREFSTR_BUFF_SIZE 128
	#define VREFSTR_BUFFS_NR   16
	static int i=0;
	static char buff_arr[VREFSTR_BUFFS_NR][VREFSTR_BUFF_SIZE];
	char *buff = buff_arr[i++ % VREFSTR_BUFFS_NR];
	#ifndef NDEBUG
	snprintf(buff, VREFSTR_BUFF_SIZE, " [ver:%3zd] ", vref.vid);
	#else
	snprintf(buff, VREFSTR_BUFF_SIZE, " (ver:%p ) ", vref.ver_);
	#endif
	return buff;
	#undef VREFSTR_BUFF_SIZE
	#undef VREFSTR_BUFF_NR
}

#endif
