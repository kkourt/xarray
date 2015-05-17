/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#include <string.h>

#include "rope_array.h"

// initialize an rpa
//  @alloc_grain: allocation grain in elements
void
rpa_init(struct rpa *rpa, size_t elem_size, size_t alloc_grain)
{
	struct rpa_leaf *leaf;

	assert(alloc_grain > 0);
	assert(elem_size > 0);
	rpa->elem_size   = elem_size;
	rpa->alloc_grain = alloc_grain;

	leaf = rpa_leaf_alloc(rpa);
	rpa->tail = leaf;
	rpa->root = &leaf->l_hdr;
}

struct rpa *
rpa_create(size_t elem_size, size_t alloc_grain)
{
	struct rpa *rpa;
	rpa = xmalloc(sizeof(*rpa));
	rpa_init(rpa, elem_size, alloc_grain);

	return rpa;
}


// allocate and initialize a concatenation node
struct rpa_node *
rpa_concat(struct rpa_hdr *left, struct rpa_hdr *right)
{
	struct rpa_node *conc;
	size_t nelems = left->nelems + right->nelems;

	conc = rpa_node_alloc();
	conc->n_hdr = RPA_NODE_HDR_INIT(nelems);
	conc->left  = left;
	conc->right = right;
	// We define [...] the depth of a concatenation to be one plus the
	// maximum depth of its children
	conc->depth = 1 + MAX(rpa_depth(left), rpa_depth(right));

	left->parent = right->parent = conc;

	return conc;
}

// @inc is in number of elements
//  update ->nelems for all nodes to the tail path (including tail)
static void
rpa_update_tailpath(struct rpa *rpa, ssize_t inc)
{
	struct rpa_leaf *tail = rpa->tail;

	assert((tail->d_size / rpa->elem_size) % rpa->alloc_grain == 0);
	assert(tail->l_hdr.nelems*rpa->elem_size <= tail->d_size);

	struct rpa_hdr *t_hdr = &tail->l_hdr;
	struct rpa_hdr *h     = rpa->root;
	for (;;) {
		#if !defined(NDEBUG)
		ssize_t new_nelems = (ssize_t)h->nelems + inc;
		assert(new_nelems >= 0);
		#endif
		h->nelems += inc;
		// we reached the tail node
		if (h == t_hdr) {
			assert(h->nelems*rpa->elem_size <= tail->d_size);
			break;
		}

		// XXX: what are the data structure invariants?
		//  a node with a single child can be turned into a leaf, so we
		//  assume that ALL nodes have two children. Hence, we must be
		//  careful when removing things.
		h = rpa_hdr_right(h);
	}
}

static struct rpa_leaf *
rpa_get_rightmost(struct rpa_hdr *hdr)
{
	struct rpa_hdr *ret;
	for (ret = hdr; ret->type != RPA_LEAF; ret = rpa_hdr_right(ret))
		;
	return rpa_hdr2leaf(ret);
}

// append @hdr onto the right of @rpa
static void
rpa_append_hdr(struct rpa *rpa, struct rpa_hdr *right)
{
	struct rpa_hdr  *left = rpa->root;

	// create a new concatenation node
	rpa->root = &rpa_concat(left, right)->n_hdr;

	// find the rightmost node of @right and set it as ->tail
	rpa->tail = rpa_get_rightmost(right);
}

// return remaining number of elements in rpa
static inline size_t
rpa_tail_remaining(struct rpa *rpa)
{
	struct rpa_leaf *tail = rpa->tail;
	size_t tail_elems_capacity = tail->d_size / rpa->elem_size;
	assert(tail->l_hdr.nelems <= tail_elems_capacity);
	return tail_elems_capacity - tail->l_hdr.nelems;
}

static inline bool
rpa_tail_full(struct rpa *rpa)
{
	struct rpa_leaf *tail = rpa->tail;
	return (tail->l_hdr.nelems*rpa->elem_size == tail->d_size);
}


void *
rpa_append_prepare(struct rpa *rpa, size_t *nelems)
{
	void *ret;
	struct rpa_leaf *tail;
	size_t tail_nelems, elem_size;

	// check if we need to allocate a new leaf
	if (rpa_tail_full(rpa)) {
		struct rpa_leaf *leaf = rpa_leaf_alloc(rpa);
		rpa_append_hdr(rpa, &leaf->l_hdr);
		assert(leaf == rpa->tail);
	}

	tail = rpa->tail;
	tail_nelems = tail->l_hdr.nelems;
	elem_size = rpa->elem_size;

	assert(tail_nelems*elem_size < tail->d_size);
	assert(tail->d_size % elem_size == 0); // not really needed

	ret = tail->data + (tail->l_hdr.nelems*elem_size);
	*nelems = (tail->d_size / elem_size) - tail_nelems;
	assert(*nelems > 0);
	return ret;
}

void
rpa_append_finalize(struct rpa *rpa, size_t nelems)
{
	rpa_update_tailpath(rpa, nelems);
}


static void
do_rpa_print(struct rpa_hdr *hdr, int ind, char *prefix)
{
	const int ind_inc = 4;
	printf(" %*s: nelems=%zd hdr=%p depth=%u\n",
	       ind, prefix, hdr->nelems, hdr,
	       hdr->type == RPA_NODE ? rpa_hdr2node(hdr)->depth : 0);
	if (hdr->type == RPA_NODE) {
		do_rpa_print(rpa_hdr2node(hdr)->left, ind+ind_inc, "L");
		do_rpa_print(rpa_hdr2node(hdr)->right, ind+ind_inc, "R");
	} else if (hdr->type == RPA_LEAF) {
		return;
	} else abort();
}

void
rpa_print(struct rpa *rpa)
{
	do_rpa_print(rpa->root, 0, "T");
}

// We define the depth of a leaf to be 0, and the depth of a concatenation to be
// one plus the maximum depth of its children.
unsigned int
rpa_depth_rec(struct rpa_hdr *hdr)
{
	unsigned int ret;

	if (hdr->type == RPA_LEAF) {
		ret = 0;
	} else if (hdr->type == RPA_NODE) {
		struct rpa_node *n = rpa_hdr2node(hdr);
		ret = 1 + MAX(rpa_depth_rec(n->left), rpa_depth_rec(n->right));
	} else abort();

	return ret;
}

/**
 * get the last node covering range @start_nelems, @len_nelems starting from @hdr.
 *   @elem_off will be set to the offset of the range in @hdr
 */
struct rpa_hdr *
rpa_gethdr_range(struct rpa_hdr *hdr, size_t start_nelems, size_t len_nelems,
                 size_t *elem_off)
{
	struct rpa_hdr *left, *right;

	// @hdr should contain the range
	#if !defined(NDEBUG)
	if (hdr->nelems < start_nelems + len_nelems) {
		fprintf(stderr,"node has less elements than requested:"
		               "hdr->nelems=%zd start_nelems=%zd len_nelems=%zd",
		               hdr->nelems, start_nelems, len_nelems);
		abort();
	}
	#endif
	// assert(len_nelems > 0);

	// eoff and len_nelems are the range we are looking for in @hdr
	size_t eoff = start_nelems;
	for (;;) {
		if (hdr->type == RPA_LEAF)
			break;

		left = rpa_hdr2node(hdr)->left;
		right = rpa_hdr2node(hdr)->right;
		/**
		 * Cases:
		 *
		 * A: range fits completely in @left
		 *  eoff    eoff+len_nelems
		 *   |-------->|
		 * [-----left-----|-----right-----]
		 *
		 * B: range spans both @left and @right
		 *  eoff          eoff+len_nelems
		 *   |--------------->|
		 * [-----left-----|-----right-----]
		 *
		 * C: range fits completely in @right
		 *                eoff     eoff+len_nelems
		 *                  |---------->|
		 * [-----left-----|-----right-----]
		 */
		if (left->nelems > eoff) {
			if (left->nelems >= eoff + len_nelems)
				hdr = left; // case A: descend left
			else
				break; // case B: we are done, return @hdr
		} else {
			hdr = right; // case C: descend right
			eoff -= left->nelems;
		}
	}

	assert(hdr->nelems >= eoff + len_nelems);
	*elem_off = eoff;
	return hdr;
}

/**
 * get the leaf for index @elem_idx inside @rpa_hdr
 *  @elem_off: the offset of @elem_idx inside the leaf
 */
struct rpa_leaf *
rpa_getleaf(struct rpa_hdr *hdr, size_t elem_idx, size_t *elem_off)
{

	hdr = rpa_gethdr_range(hdr, elem_idx, 1, elem_off);
	return rpa_hdr2leaf(hdr);
}


void *
rpa_getchunk(struct rpa *rpa, size_t elem_idx, size_t *ch_elems)
{
	struct rpa_leaf *leaf;
	size_t off;

	leaf = rpa_getleaf(rpa->root, elem_idx, &off);
	if (ch_elems)
		*ch_elems = leaf->l_hdr.nelems - off;

	return &leaf->data[off*rpa->elem_size];
}

static void
rpa_depth_recalc(struct rpa *rpa, struct rpa_node *node)
{
	for (;;) {
		// new depth
		unsigned depth = 1 + MAX(rpa_depth(node->left),
		                         rpa_depth(node->right));

		// if no change is needed, we are done
		if (node->depth == depth)
			break;

		node->depth = depth;

		// reached root, we are done
		if (&node->n_hdr == rpa->root) {
			assert(node->n_hdr.parent == NULL);
			break;
		}

		node = node->n_hdr.parent;
	}
}

void
rpa_pop(struct rpa *rpa, size_t *nelems_ptr)
{
	struct rpa_leaf *tail = rpa->tail;
	size_t tail_nelems    = tail->l_hdr.nelems;
	size_t nelems         = *nelems_ptr;

	// no need to pop a node
	if (tail_nelems > nelems) {
		rpa_update_tailpath(rpa, -nelems);
		assert(tail->l_hdr.nelems == (tail_nelems - nelems));
		return;
	}

	// we need to remove a node from the tree

	// We will just pop the tail node and return a partial result if needed
	if (tail_nelems < nelems)
		*nelems_ptr = nelems = tail->l_hdr.nelems;
	// update ->nelems for nodes in the tail path
	rpa_update_tailpath(rpa, -nelems);
	assert(tail->l_hdr.nelems == 0);

	struct rpa_node *p = tail->l_hdr.parent; // @tail's parent

	// rpa has a single leaf. keep root which points to an empty leaf
	if (p == NULL) {
		assert(rpa->root = &tail->l_hdr);
		return;
	}

	assert(p->right == &tail->l_hdr);
	// We need to deallocate @p and @tail,
	// and redirect what points to @p to point to @p->left.
	// @p is pointed to either by @p->parent->right or @rpa->root
	struct rpa_hdr *p_left = p->left;         // new tail node
	struct rpa_node *pp = p->n_hdr.parent; // p's parent
	if (pp != NULL) { // @p has a parent
		// @p should be @pp's right child
		assert(pp->right == &p->n_hdr);
		pp->right = p_left;
		p_left->parent = pp;
		rpa_depth_recalc(rpa, pp);
	} else {                       // @p is root
		assert(rpa->root = &p->n_hdr);
		rpa->root = p_left;
		p_left->parent = NULL;
	}

	// deallocate p and tail
	rpa_node_dealloc(p);
	assert(tail->l_hdr.nelems == 0);
	rpa_leaf_dealloc(tail);

	// update ->tail pointer
	rpa->tail = rpa_get_rightmost(p_left);
}

/*
 * (re)Balancing
 */

// These are Fibonacci numbers < 2**32.
// F_n = rpa_fib[n-2]
static const unsigned long rpa_fib[] = {
      /* 0 */1, /* 1 */2, /* 2 */3, /* 3 */5, /* 4 */8, /* 5 */13, /* 6 */21,
      /* 7 */34, /* 8 */55, /* 9 */89, /* 10 */144, /* 11 */233, /* 12 */377,
      /* 13 */610, /* 14 */987, /* 15 */1597, /* 16 */2584, /* 17 */4181,
      /* 18 */6765, /* 19 */10946, /* 20 */17711, /* 21 */28657, /* 22 */46368,
      /* 23 */75025, /* 24 */121393, /* 25 */196418, /* 26 */317811,
      /* 27 */514229, /* 28 */832040, /* 29 */1346269, /* 30 */2178309,
      /* 31 */3524578, /* 32 */5702887, /* 33 */9227465, /* 34 */14930352,
      /* 35 */24157817, /* 36 */39088169, /* 37 */63245986, /* 38 */102334155,
      /* 39 */165580141, /* 40 */267914296, /* 41 */433494437,
      /* 42 */701408733, /* 43 */1134903170, /* 44 */1836311903,
      /* 45 */2971215073u };

#include "build_assert.h"
#include "array_size.h"

#define RPA_FIB_SIZE 46
STATIC_ASSERT(ARRAY_SIZE(rpa_fib) == RPA_FIB_SIZE);

// see SGI's rope class implementation
// in gcc: libstdc++-v3/include/ext/ropeimpl.h
//
// @forest:
// the rebalancing operation maintains an ordered sequence of (empty or)
// balanced ropes, one for each length interval [F_n, F_{n+1}), for n>=2
// [...] The concatenation of the sequence of ropes in order of
// decreasing length is equivalent to the prefix of the rope we have
// traversed so far. Eacn new leaf is inserted into the appropriate
// entry of the sequence.
//
//
// about static in @forest:
// http://hamberg.no/erlend/posts/2013-02-18-static-array-indices.html
static void
rpa_add_to_forest(struct rpa_hdr *hdr,
                  struct rpa_hdr *forest[static RPA_FIB_SIZE])
{

	// we traverse the rope from left to right, inserting each leaf into the
	// appropriate sequence position, depending on its length
	if (hdr->type == RPA_NODE) {
		//printf(" hdr=%p (nelems=%zd) not balanced\n", hdr, hdr->nelems);
		struct rpa_node *n = rpa_hdr2node(hdr);
		struct rpa_hdr *left = n->left, *right = n->right;
		rpa_node_dealloc(n); // do deallocation first, to aid mm
		rpa_add_to_forest(left,  forest);
		rpa_add_to_forest(right, forest);
		return;
	}

	struct rpa_hdr *prefix = NULL, *tmp;
	size_t nelems = hdr->nelems;
	unsigned int i;

	// find @hdr's slot in the forest, and concatenate all nodes in the
	// previous slots into @prefix
	for (i=0; i<RPA_FIB_SIZE-1; i++) {
		// add to prefix if slot is not empty
		//  Note that we concatenate ***onto the left***
		if ((tmp = forest[i]) != NULL) {
			prefix = (prefix == NULL) ? tmp : &rpa_concat(tmp, prefix)->n_hdr;
			forest[i] = NULL;
		}
		// found slot
		if (nelems < rpa_fib[i+1])
			break;
		// We 've reached the end of rpa_fib,
		// but did not find a proper slot
		if (i == RPA_FIB_SIZE -2) {
			fprintf(stderr,
			        "%s (%s:%d): Rope too deep: "
			        "nelems=%zd i=%u rpa_fib[i]=%zd Extend rpa_fib?\n",
			        __FUNCTION__, __FILE__, __LINE__,
			        nelems, i, rpa_fib[i]);
			abort();
		}
	}

	// if there is a prefix, add it to our node
	if (prefix != NULL) {
		hdr = &rpa_concat(prefix, hdr)->n_hdr;
		nelems = hdr->nelems;
	}

	// try to add @hdr to a slot
	for (;; i++) {
		// if slot is occupied concatenate it onto the left of @hdr
		if ( (tmp = forest[i]) != NULL) {
			hdr = &rpa_concat(tmp, hdr)->n_hdr;
			nelems = hdr->nelems;
			forest[i] = NULL;
		}
		// can hdr be placed in @i?
		if (nelems < rpa_fib[i+1]) {
			forest[i] = hdr;
			break;
		}
		if (i == RPA_FIB_SIZE - 1) {
			fprintf(stderr,
			        "%s (%s:%d): Rope too deep. Extend rpa_fib?\n",
			        __FUNCTION__, __FILE__, __LINE__);
			abort();
		}
	}
}

// A rope of depth n is balanced if its length is at least F_{n+2}, e.g., a
// balanced rope of depth 1 must have length at least 2. Note that balanced
// ropes may contain unbalanced subropes
bool
rpa_is_balanced(struct rpa_hdr *hdr)
{
	unsigned d = rpa_depth(hdr);

	if (d >= ARRAY_SIZE(rpa_fib)) {
		assert(rpa_fib[ARRAY_SIZE(rpa_fib) -1] > hdr->nelems);
		return false;
	}

	// leafs have a depth of 0, so non-empty leafs are always balanced
	return hdr->nelems >= rpa_fib[d];
}

void
rpa_rebalance(struct rpa *rpa)
{
	struct rpa_hdr *res = NULL, *tmp;
	struct rpa_hdr *forest[RPA_FIB_SIZE] = {NULL};
	struct rpa_hdr *root = rpa->root;

	#if 0
	if (root->type == RPA_NODE) {
		unsigned d = rpa_depth(root);
		unsigned d1 = rpa_depth(rpa_hdr2node(root)->right);
		unsigned d2 = rpa_depth(rpa_hdr2node(root)->left);
		printf("d=%u right:%d left:%d length:%zd\n",
		        d, d1, d2, root->nelems);
	}
	#endif

	if (rpa_is_balanced(root))
		return;

	// recursively add nodes to forest
	rpa_add_to_forest(root, forest);

	for (unsigned i=0; i<RPA_FIB_SIZE; i++) {
		if ( (tmp = forest[i]) == NULL)
			continue;
		res = (res == NULL) ? tmp : &rpa_concat(tmp, res)->n_hdr;
	}

	// Note: ->tail should remain the same

	assert(res != NULL);
	rpa->root = res;

	#if 0
	printf("AFTER REBAL\n");
	if (res->type == RPA_NODE) {
		unsigned d = rpa_depth(res);
		unsigned d1 = rpa_depth(rpa_hdr2node(res)->right);
		unsigned d2 = rpa_depth(rpa_hdr2node(res)->left);
		printf("d=%u right:%d left:%d length:%zd\n",
		        d, d1, d2, res->nelems);
	}
	#endif
}

void
do_rpa_verify(struct rpa_hdr *hdr)
{
	if (hdr->type == RPA_NODE) {
		struct rpa_hdr *l = rpa_hdr2node(hdr)->left;
		struct rpa_hdr *r = rpa_hdr2node(hdr)->right;
		if (l->nelems + r->nelems != hdr->nelems) {
			fprintf(stderr,
			        "verify failed: l->nelems=%zd r->nelems=%zd "
			        "hdr->nelems=%zd",
			        l->nelems, r->nelems, hdr->nelems);
			abort();
		}

		unsigned int d = rpa_depth(hdr);
		unsigned int d_rec = rpa_depth_rec(hdr);
		if (d != d_rec) {
			fprintf(stderr,
			        "verify failed: depth=%u depth_rec=%u\n",
				d, d_rec);
			do_rpa_print(hdr, 2, "");
			abort();
		}
	} else if (hdr->type == RPA_LEAF) {
		return;
	} else abort();
}

void
rpa_verify(struct rpa *rpa)
{
	#if !defined(NDEBUG)
	do_rpa_verify(rpa->root);
	#endif
}


void
do_rpa_concatenate(struct rpa *rpa1, struct rpa *rpa2)
{
	struct rpa_hdr *n1, *n2;
	size_t elem_size;

	elem_size = rpa1->elem_size;
	if (elem_size != rpa2->elem_size) {
		fprintf(stderr, "Incompatible element sizes (%zd != %zd)\n",
		        rpa1->elem_size, rpa2->elem_size);
		abort();
	}

	n1 = rpa1->root;
	n2 = rpa2->root;
	if (n2->type == RPA_LEAF) {
		struct rpa_leaf *leaf1 = rpa1->tail;
		struct rpa_leaf *leaf2 = rpa2->tail;
		assert(&leaf2->l_hdr == rpa2->root);

		size_t l2_elems    = leaf2->l_hdr.nelems;
		size_t l1_elems    = leaf1->l_hdr.nelems;
		size_t l1_capacity = leaf1->d_size / elem_size;

		if (l1_elems + l2_elems <= l1_capacity) {
			void *dst = leaf1->data + (l1_elems*elem_size);
			void *src = leaf2->data;
			memcpy(dst, src, l2_elems*elem_size);
			rpa_update_tailpath(rpa1, l2_elems);
			rpa_leaf_dealloc(leaf2);
			return;
		}
	}

	rpa1->tail = rpa2->tail;
	rpa1->root = &rpa_concat(n1,n2)->n_hdr;
}

struct rpa *
rpa_concatenate(struct rpa *rpa1, struct rpa *rpa2)
{
	do_rpa_concatenate(rpa1, rpa2);
	free(rpa2);
	return rpa1;
}

