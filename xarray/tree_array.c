/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

// note that this is a radix tree with one bit index
// btree stands for binary-tree, not for B-tree
union btree_node;
struct btree_inode;
struct btree_leaf;

struct tree_array {
	size_t            elems_nr;
	union btree_node  *root;
};

struct btree_inode {
	union btree_node *left, *right;
};

struct btree_leaf {
	unsigned long val;
};

union btree_node {
	struct btree_inode inode;
	struct btree_leaf  leaf;
};

/*
 * __trar_height returns the (binary) tree height required to hold x elements
 *   - We calculate this using the number of leading zeroes as return by clz gcc bultin
 *   - To correctly handle 2^n numbers we substract one from x.
 *     Note that height h should be returned for  2^(h-1) + 1 <= x <= 2^h
 *   - x == 0 and x == 1, are special cases for clz(x-1)
 */
static inline size_t
__trar_height(unsigned long x)
{
	if (x == 0 || x == 1)
		return x;

	return (8*sizeof(x) - __builtin_clzl(x-1));
}

static inline size_t
trar_height(struct tree_array *trar)
{
	return __trar_height(trar->elems_nr);
}

union btree_node *
trar_alloc_node()
{
	union btree_node *ret;

	ret = malloc(sizeof(union btree_node));
	if (ret == NULL){
		perror("trar_alloc_node");
		exit(1);
	}
	return ret;
}

struct tree_array *
trar_allocate(void)
{
	struct tree_array *trar;

	trar = malloc(sizeof(struct tree_array));
	if (!trar){
		perror("malloc");
		exit(1);
	}

	trar->root = NULL;
	trar->elems_nr = 0;

	return trar;
}

void
trar_free(struct tree_array *trar)
{
}

void
trar_resize(struct tree_array *trar, size_t elems_nr)
{
}

union btree_node *
trar_search(union btree_node *node, size_t idx, size_t hlimit)
{
	size_t i;
	unsigned long mask;
	union btree_node *ret;

	mask = 1<<(hlimit-1);
	ret = node;
	for (i=0; i<hlimit; i++) {
		ret = (idx & mask) ? ret->inode.right : ret->inode.left;
		mask >>= 1;
	}

	return ret;
}

union btree_node *
trar_getnode(struct tree_array *trar, size_t idx)
{
	size_t height;

	if (idx >= trar->elems_nr) {
		fprintf(stderr, "%s() ==> invalid idx: %zd\n", __FUNCTION__, idx);
		exit(1);
	}

	height = trar_height(trar);
	return trar_search(trar->root, idx, height);
}

unsigned long
trar_getval(struct tree_array *trar, size_t idx)
{
	union btree_node *node;

	node = trar_getnode(trar, idx);
	return node->leaf.val;
}

void
trar_setval(struct tree_array *trar, size_t idx, unsigned long val)
{
	union btree_node *node;

	node = trar_getnode(trar, idx);
	node->leaf.val = val;
}

void
trar_print(struct tree_array *trar)
{
	size_t elems_nr, i;
	elems_nr = trar->elems_nr;
	#if 0
	size_t height, total_space, max_nodes, spaces, parts;
	size_t j, k;
	union btree_node *n;

	height = trar_height(trar);
	total_space = (1<<(height))*7;

	printf("height:%zd total_space:%zd\n", height, total_space);
	for (i=0; i<height; i++){
		max_nodes = (1<<i);
		spaces = total_space/(max_nodes*2);
		printf("\tspaces:%lu max_nodes:%lu\n", spaces, max_nodes);
		for (j = 0; j < max_nodes; j++) {
			if (j >= elems_nr)
				break;
			n = trar_search(trar->root, j, i);
			for (k=0; k<spaces; k++){
				printf(" ");
			}
			printf("%s", "   o   ");
		}
		printf("\n");
	}
	#endif

	for (i=0; i<elems_nr; i++){
		printf("%7ld", trar_getval(trar, i));
	}
	printf("\n");
}


/*
 * To append an element to the tree array we need a chain of tree nodes,
 * connected on the left child:
 *
 *                     o
 *                    / \
 *                  o    /
 *                 / \
 *               o    /
 *              / \
 *            ...  /
 *            /
 *          val
 *
 * Essentially the last element included in the tree will be changed
 *  from xxxxx0 to xxxxx1  --> chain size: 1 element
 *  from xxxx01 to xxxx10  --> chain size: 2 elements
 *  from xxx011 to xxx100  --> chain size: 3 elements
 *  from xx0111 to xx1000  --> chain size: 4 elements
 *    ... and so on so forth ...
 *
 * The new last element is the number of elements currently in the tree.
 * Hence, the chain size is equal to the number of trailing zeroes plus one.
 *
 */
static unsigned long
__trar_append_chain_size(unsigned long elems_nr)
{
	return __builtin_ctzl(elems_nr) + 1;
}

union btree_node *
trar_alloc_chain(unsigned long chain_len, long val)
{
	union btree_node *chain, *prev;
	size_t i;

	assert(chain_len > 0);

	/* allocate leaf */
	chain = trar_alloc_node();
	chain->leaf.val = val;

	/* allocate internal nodes */
	for (i=1; i<chain_len; i++){
		prev = chain;
		chain = trar_alloc_node();
		chain->inode.left = prev;
		chain->inode.right = NULL;
	}

	return chain;
}

void
trar_append(struct tree_array *trar, long val)
{
	size_t height, elems_nr, chain_len, i, last_idx;
	union btree_node *chain, *n;
	unsigned long mask;

	elems_nr = trar->elems_nr;
	height = __trar_height(elems_nr);
	chain_len = __trar_append_chain_size(elems_nr);
	chain = trar_alloc_chain(chain_len, val);

	if (trar->root == NULL){
		assert(trar->elems_nr == 0);
		trar->root = trar_alloc_node();
		trar->root->inode.left = chain;
		trar->root->inode.right = NULL;
		trar->elems_nr++;
		return;
	}
	assert(trar->elems_nr != 0);

	if (chain_len > height) { /* we need a new root node */
		union btree_node *root;
		root = trar_alloc_node();

		root->inode.left  = trar->root;
		root->inode.right = NULL;
		trar->root = root;
		height++;
	}

	/*
	 * We need to place the chain on the right child of a node.
	 * The node is found by following the last available array index
	 * until we reach a height of: tree_height - chain_len
	 */
	assert(height >= chain_len);
	mask = 1<<(height-1);
	last_idx = elems_nr - 1;
	n = trar->root;
	for (i=0; i<height - chain_len; i++){
		n = (mask & last_idx) ? n->inode.right : n->inode.left;
		mask >>= 1;
	}

	assert(n->inode.right == NULL);
	n->inode.right = chain;
	trar->elems_nr++;
}

void
trar_rle_merge(struct tree_array *rle1, struct tree_array *rle2)
{
}

int main(int argc, const char *argv[])
{
	size_t i;
	struct tree_array *trar;

	trar = trar_allocate();
	for (i=0; i<5; i++){
		trar_append(trar, i);
		trar_print(trar);
	}
	return 0;
}
