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
//   @alloc_grain is the default leaf node (in elemenets)
//   @tail points always to the last leaf
struct rpa {
	size_t           elem_size;
	size_t           alloc_grain;
	struct rpa_hdr  *root;
	struct rpa_leaf *tail;
};

// common part for (intenal) nodes and leafs in the rope array
struct rpa_hdr {
	size_t          nelems;        // len in number of elements;
	struct rpa_node *parent;
	enum  {
		RPA_INVALID = 0,
		RPA_NODE    = 1,
		RPA_LEAF    = 2
	} type;
};

// rope array (internal) node. Note that this is also called concatenation.
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

#define RPA_LEAF_HDR_INIT         RPA_HDR_INIT(0, RPA_LEAF);
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
	assert(n == 1);
	rpa_append_finalize(rpa, n);

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


#endif /* ROPE_ARRAY_H__ */
