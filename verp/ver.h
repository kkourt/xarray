#ifndef VER_H__
#define VER_H__

#include <stdbool.h>
#include <inttypes.h>

#include "refcnt.h"
#include "container_of.h"
#include "misc.h"

#include "vbpt_stats.h"

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
 * ver_rebase():    change parent
 *
 * ver_pin():       pin a version
 *
 * ver_eq():        check two versions for equality
 * ver_leq_limit(): query partial order
 * ver_join():      find join point
 *
 */

struct ver {

	struct ver *parent;
	#ifndef NDEBUG
	size_t     v_id;
	#endif
	refcnt_t   rfcnt;
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

	// XXX: ugly but useful
	ver->v_log.state = VBPT_LOG_UNINITIALIZED;
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
refcnt_total2ver(refcnt_t *rcnt)
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

static void ver_release(refcnt_t *);

/**
 * release a node reference
 */
static inline void
ver_putref(ver_t *ver)
{
	refcnt_dec(&ver->rfcnt, ver_release);
}

// grab a child reference
static inline void
ver_get_child_ref(ver_t *ver)
{
	refcnt_inc(&ver->rfcnt);
}

// put a child reference
static inline void
ver_put_child_ref(ver_t *ver)
{
	refcnt_dec(&ver->rfcnt, ver_release);
}

/**
 * release a version
 */
static void
ver_release(refcnt_t *refcnt)
{
	ver_t *ver = refcnt_total2ver(refcnt);
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
	if (ver->parent != NULL)
		ver_put_child_ref(parent);

	if (ver->v_log.state != VBPT_LOG_UNINITIALIZED)
		vbpt_log_destroy(&ver->v_log);

	ver_mm_free(ver);
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
	// maintain a chain and cal ver_put_child_ref() on allocation
	ver_t *v = ver->parent;
	while (v != NULL) {
		ver_t *tmp = v->parent;
		v->parent = NULL;       // remove @v from chain
		ver_put_child_ref(v);
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
	ver_get_child_ref(parent);
	v->parent = parent;
}

/**
 * prepare a version rebase.  at the new parent won't be removed from the
 * version chain under our nose
 */
static inline void
ver_rebase_prepare(ver_t *new_parent)
{
	ver_get_child_ref(new_parent);
}

static inline void
ver_rebase_commit(ver_t *ver, ver_t *new_parent)
{
	if (ver->parent)
		ver_put_child_ref(ver->parent);
	ver->parent = new_parent;
}

static inline void
ver_rebase_abort(ver_t *new_parent)
{
	ver_put_child_ref(new_parent);
}

/**
 * rebase: set a new parent to a version.
 *  If previous parent is not NULL, refcount will be decreased
 *  Will get a new referece of @new_parent
 */
static inline void __attribute__((deprecated))
ver_rebase(ver_t *ver, ver_t *new_parent)
{
	if (ver->parent) {
		ver_put_child_ref(ver->parent);
		//ver_tree_gc(ver);
	}
	ver_setparent__(ver, new_parent);
}

/**
 * detach: detach version from the chain
 *   sets parent to NULL
 */
static inline void
ver_detach(ver_t *ver)
{
	if (ver->parent) {
		ver_put_child_ref(ver->parent);
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

ver_t *
ver_join_slow(ver_t *gver, ver_t *pver, ver_t **prev_pver,
              uint16_t *gdist, uint16_t *pdist);

/**
 * find the join point (largest common ancestor) of two versions
 *
 * The main purpose of this is to find the common ancestor of two versions.
 * Given our model, however, it gets a bit more complicated than that.
 * We assume that the join is performed between two versions:
 *  - @gver: the current version of the object (read-only/globally viewable)
 *  - @pver: a diverged version, private to the transaction
 *
 * The actual join operation (find the common ancestor) is symmetric, but we
 * neeed to distinguish between the two versions because after we (possibly)
 * merge the two versions, we need to modify the version tree (see merge
 * operation), which requires more information than the common ancestor.
 * Specifically, we need to move @pver under @gver, so we need last node in the
 * path from @pver to the common ancestor (@prev_v). This node is returned in
 * @prev_v, if @prev_v is not NULL.
 *
 *        (join_v)    <--- return value
 *       /        \
 *  (prev_v)      ...
 *     |           |
 *    ...        (gver)
 *     |
 *   (pver)
 *
 * Furthermore, another useful property for the merge algorithm is to now the
 * distance of each version from the join point. This allows to have more
 * efficient checks on whether a version found in the tree is before or after
 * the join point. The distance of the join point from @gver (@pver) is returned
 * in @gdist (@pdist).
 */
static inline ver_t *
ver_join(ver_t *gver, ver_t *pver, ver_t **prev_v, uint16_t *gdist, uint16_t *pdist)
{
	/* this is the most common case, do it first */
	if (gver->parent == pver->parent) {
		assert(pver->parent != NULL);
		if (prev_v)
			*prev_v = pver;
		*gdist = *pdist = 1;
		return pver->parent;
	}
	return ver_join_slow(gver, pver, prev_v, gdist, pdist);

}

static inline ver_t *
ver_parent(ver_t *ver)
{
	return ver->parent;
}

/*
 * Log helpers
 */
static inline ver_t *
vbpt_log2ver(vbpt_log_t *log)
{
	return container_of(log, ver_t, v_log);
}

/**
 * return the parent log
 *  Assumption: this is a log embedded in a version
 */
static inline vbpt_log_t *
vbpt_log_parent(vbpt_log_t *log)
{
	ver_t *ver = vbpt_log2ver(log);
	ver_t *ver_p = ver_parent(ver);
	return (ver_p != NULL) ? &ver_p->v_log : NULL;
}



#endif
