#ifndef ROPE_ARRAY_H__
#define ROPE_ARRAY_H__

/**
 * Rope arrays
 *  see: Ropes: an Alternative to Strings, Boehm et al.
 *  (some paper text can be found in comments)
 */

#include <stddef.h>        // size_t
#include <assert.h>

#include "container_of.h"
#include "build_assert.h"

#include "misc.h"  // xmalloc

struct rpa_hdr;
struct rpa_leaf;
struct rpa_node;

// rope array
//   @elem_size is the size of the array's elements
//   @alloc_grain is the default leaf node size (in elements)
//   @tail always points to the last leaf
struct rpa {
	size_t           elem_size;
	size_t           alloc_grain;
	struct rpa_hdr  *root;
	struct rpa_leaf *tail;
};

// common part for intenal nodes and leafs in the rope array
struct rpa_hdr {
	size_t          nelems;        // len in number of elements;
	struct rpa_node *parent;
	enum  {
		RPA_INVALID = 0,
		RPA_NODE    = 1,
		RPA_LEAF    = 2
	} type;
};

// rope array internal node. Note that this is also called a concatenation.
struct rpa_node {
	struct rpa_hdr n_hdr;
	struct rpa_hdr *left, *right;
	unsigned depth;
};

// rope array leaf
struct rpa_leaf {
	struct rpa_hdr l_hdr;
	size_t d_size;           // size of data (in bytes)
	char   data[];
};

#define RPA_HDR_INIT(nelems_, type_) (struct rpa_hdr){ \
    .nelems = nelems_,                                 \
    .parent = NULL,                                    \
    .type = type_                                      \
}

#define RPA_LEAF_HDR_INIT         RPA_HDR_INIT(0, RPA_LEAF)
#define RPA_NODE_HDR_INIT(nelems) RPA_HDR_INIT(nelems, RPA_NODE)

/**
 * simple access wrappers
 */

static inline struct rpa_node *
rpa_hdr2node(struct rpa_hdr *hdr)
{
	assert(hdr->type == RPA_NODE);
	return container_of(hdr, struct rpa_node, n_hdr);
}

static inline struct rpa_hdr *
rpa_hdr_right(struct rpa_hdr *hdr)
{
	struct rpa_node *node = rpa_hdr2node(hdr);
	return node->right;
}

static inline struct rpa_leaf *
rpa_hdr2leaf(struct rpa_hdr *hdr)
{
	assert(hdr->type == RPA_LEAF);
	return container_of(hdr, struct rpa_leaf, l_hdr);
}

// rpa size in number of elements
static inline size_t
rpa_size(struct rpa *rpa)
{
	return rpa->root->nelems;
}

static inline size_t
rpa_elem_size(struct rpa *rpa)
{
	return rpa->elem_size;
}

// recersive computation of depth
unsigned int rpa_depth_rec(struct rpa_hdr *hdr);

static inline unsigned int
rpa_depth(struct rpa_hdr *hdr)
{
	unsigned int ret;
	if (hdr->type == RPA_LEAF)
		ret = 0;
	else if (hdr->type == RPA_NODE)
		ret = rpa_hdr2node(hdr)->depth;
	else abort();

	//assert(ret == rpa_depth_rec(hdr));
	return ret;
}

/**
 * mm helpers
 */

// dsize is ->data size in bytes
static inline struct rpa_leaf *
rpa_leaf_do_alloc(size_t d_size)
{
	struct rpa_leaf *leaf;
	size_t leaf_size = sizeof(*leaf) + d_size;

	leaf = xmalloc(leaf_size);
	leaf->l_hdr  = RPA_LEAF_HDR_INIT;
	leaf->d_size = d_size;

	return leaf;
}

// allocate and initialize a leaf
static inline struct rpa_leaf *
rpa_leaf_alloc(struct rpa *rpa)
{
	return rpa_leaf_do_alloc(rpa->elem_size*rpa->alloc_grain);
}

static inline void
rpa_leaf_dealloc(struct rpa_leaf *leaf)
{
	free(leaf);
}

static inline struct rpa_node *
rpa_node_alloc(void)
{
	return xmalloc(sizeof(struct rpa_node));
}

static inline void
rpa_node_dealloc(struct rpa_node *node)
{
	free(node);
}

/**
 * rpa interface
 */

struct rpa *rpa_create(size_t elem_size, size_t alloc_grain);
void        rpa_init(struct rpa *rpa, size_t elem_size, size_t alloc_grain);

void rpa_print(struct rpa *rpa);

void *rpa_getchunk(struct rpa *rpa, size_t elem_idx, size_t *ch_elems);
struct rpa_leaf *rpa_getleaf(struct rpa_hdr *hdr, size_t eidx, size_t *eoff);
struct rpa_hdr  *rpa_gethdr_range(struct rpa_hdr *hdr,
                                  size_t off_nelems, size_t len_nelems,
                                  size_t *elem_off);

void *rpa_append_prepare(struct rpa *rpa, size_t *nelems);
void  rpa_append_finalize(struct rpa *rpa, size_t nelems);

void rpa_pop(struct rpa *rpa, size_t *nelems_ptr);

struct rpa_node *rpa_concat(struct rpa_hdr *left, struct rpa_hdr *right);

bool rpa_is_balanced(struct rpa_hdr *hdr);
void rpa_rebalance(struct rpa *rpa);

void rpa_verify(struct rpa *rpa);

/**
 * simple helpers based on above interface
 */

static inline void *
rpa_append(struct rpa *rpa)
{
	void *ret;

	size_t n = 1;
	ret = rpa_append_prepare(rpa, &n);
	assert(n >= 1);
	rpa_append_finalize(rpa, 1);

	return ret;
}

static inline void *
rpa_getlast(struct rpa *rpa)
{
	struct rpa_leaf *tail = rpa->tail;
	size_t d_idx;

	assert(tail->l_hdr.nelems > 0);

	d_idx = (tail->l_hdr.nelems - 1)*rpa->elem_size;
	return &tail->data[d_idx];
}


static inline void *
rpa_get(struct rpa *rpa, size_t elem_idx)
{
	return rpa_getchunk(rpa, elem_idx, NULL);
}

/**
 * rope array slice
 *   @rpa:              rpa the slice belongs to
 *   @sl_start/@sl_len: start and len of the slice in @rpa
 *   @hdr0:             smallest node that contains the whole slice
 *   @leaf0:            first leaf of the slice
 *   @leaf0_off:        offset of slice in first leaf
 *
 * hdr0 and leaf0 are an optimization to avoid searching the whole
 * rpa.
 * ->leaf0 is used to access the start of the slice.
 * ->hdr0 is used for random access.
 *
 *  NB: Initially, I only used leaf0. Each lookup went up from leaf0 to find
 *  hdr0 using ->parent and then down to find the desired index. Starting from
 *  leaf0 might make sense for some cases, e.g., for getnextchunk(), but lookups
 *  do not currently do it to keep things simple.
 *
 *  In addition, we could also maintain the last leaf, but we currently don't,
 */
struct rpa_slice {
	struct rpa *rpa;
	size_t sl_start, sl_len;

	struct rpa_hdr  *hdr0;
	size_t hdr0_off;

	struct rpa_leaf *leaf0;
	size_t leaf0_off;
};

// initialize a slice from an rpa
static inline void
rpa_init_slice(struct rpa_slice *sl, struct rpa *rpa, size_t start, size_t len)
{
	sl->rpa = rpa;
	sl->sl_start = start;
	sl->sl_len = len;

	sl->hdr0 = rpa_gethdr_range(rpa->root, start, len, &sl->hdr0_off);

	// leaf0 is initialized lazily
	sl->leaf0 = NULL;
	sl->leaf0_off = 0;
}


// init slice @sl from slice @sl_orig.
//  @sl is (@off_nelems, @len_nelems) relatively to @sl_orig
static inline void
rpa_slice_init_slice(struct rpa_slice *sl,
                     struct rpa_slice const *sl_orig,
                     size_t off_nelems, size_t len_nelems)
{
	// assert(sl_orig->sl_len >= idx + len);

	sl->rpa = sl_orig->rpa;
	sl->sl_len = len_nelems;
	sl->sl_start = sl_orig->sl_start + off_nelems;

	sl->hdr0 = rpa_gethdr_range(sl_orig->hdr0,
	                            sl_orig->hdr0_off + off_nelems,
	                            len_nelems,
	                            &sl->hdr0_off);

	// leaf0 is initialized lazily
	sl->leaf0 = NULL;
	sl->leaf0_off = 0;
}

static inline void
rpa_slice_leaf0_init(struct rpa_slice *sl)
{
	sl->leaf0 = rpa_getleaf(sl->hdr0, sl->hdr0_off, &sl->leaf0_off);
}

static inline size_t
rpa_slice_get_firstchunk_nelems__(struct rpa_slice *sl)
{
	assert(sl->leaf0->l_hdr.nelems > sl->leaf0_off);
	return MIN(sl->sl_len, sl->leaf0->l_hdr.nelems - sl->leaf0_off);
}

static inline void *
rpa_slice_get_firstchunk__(struct rpa_slice *sl, size_t *nelems)
{
	size_t d_off = sl->leaf0_off*sl->rpa->elem_size;
	if (nelems)
		*nelems = rpa_slice_get_firstchunk_nelems__(sl);
	return &sl->leaf0->data[d_off];
}

static inline void *
rpa_slice_get_firstchunk(struct rpa_slice *sl, size_t *nelems)
{
	if (sl->leaf0 == NULL)
		rpa_slice_leaf0_init(sl);

	return rpa_slice_get_firstchunk__(sl, nelems);
}

static inline void *
rpa_slice_getchunk(struct rpa_slice *sl, size_t idx, size_t *nelems)
{
	struct rpa_leaf *leaf;
	size_t leaf_off, d_off;

	leaf = rpa_getleaf(sl->hdr0, sl->hdr0_off, &leaf_off);

	assert(sl->sl_len > idx);
	//assert(leaf->l_ndr.nelems > leaf_off);
	*nelems = MIN(sl->sl_len - idx, leaf->l_hdr.nelems - leaf_off);

	d_off = leaf_off*sl->rpa->elem_size;
	return &leaf->data[d_off];
}


// move the start of a slice
static inline void
rpa_slice_move_start(struct rpa_slice *sl, size_t nelems)
{

	assert(sl->sl_len >= nelems);
	sl->sl_start += nelems;
	sl->sl_len   -= nelems;

	// slice size is zero: slice can no longer be accssed
	if (sl->sl_len == 0)
		return;

	//printf("%s: sl->sl_len=%zd\n", __FUNCTION__, sl->sl_len);
	sl->hdr0 = rpa_gethdr_range(sl->hdr0,
	                            sl->hdr0_off + nelems,
	                            sl->sl_len,
	                            &sl->hdr0_off);

	if (sl->leaf0 != NULL)
		rpa_slice_leaf0_init(sl);
}

static inline void
rpa_slice_split(struct rpa_slice const *sl,
                struct rpa_slice *sl1, struct rpa_slice *sl2)
{
	size_t l1 = sl->sl_len / 2;
	size_t l2 = sl->sl_len - l1;

	sl1->rpa = sl2->rpa = sl->rpa;

	sl1->sl_start = sl->sl_start;
	sl1->sl_len = l1;
	sl1->hdr0 = rpa_gethdr_range(sl->hdr0, sl->hdr0_off + 0, l1, &sl1->hdr0_off);
	sl1->leaf0 = NULL; //sl->leaf0;
	sl1->leaf0_off = 0; //sl->leaf0_off;

	sl2->sl_start = sl->sl_start + l1;
	sl2->sl_len = l2;
	sl2->hdr0 = rpa_gethdr_range(sl->hdr0, sl->hdr0_off + l1, l2, &sl2->hdr0_off);
	sl2->leaf0 = NULL;
	sl2->leaf0_off = 0;

}

static inline void
rpa_slice_shrink(struct rpa_slice *sl, ssize_t nelems)
{
	assert(sl->sl_len >= nelems);
	sl->sl_len -= nelems;
	sl->hdr0 = rpa_gethdr_range(sl->hdr0, 0, sl->sl_len, &sl->hdr0_off);
}

#endif /* ROPE_ARRAY_H__ */
