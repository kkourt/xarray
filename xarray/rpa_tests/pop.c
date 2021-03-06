/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#include "rope_array.h"

/**
 *       (n1)
 *      /   \
 *   (l1)  (n2)
 *         /  \
 *      (l2)  (l3)
 */

int main(int argc, const char *argv[])
{
	struct rpa_leaf *l1, *l2, *l3;
	struct rpa_node *n1, *n2;

	l1 = rpa_leaf_do_alloc(1);
	l2 = rpa_leaf_do_alloc(1);
	l3 = rpa_leaf_do_alloc(1);

	l1->l_hdr.nelems = l2->l_hdr.nelems = l3->l_hdr.nelems = 1;

	n2 = rpa_concat(&l2->l_hdr, &l3->l_hdr);
	n1 = rpa_concat(&l1->l_hdr, &n2->n_hdr);
	n1->n_hdr.parent = NULL;

	struct rpa *rpa;
	rpa = xmalloc(sizeof(*rpa));
	rpa->elem_size = 1;
	rpa->alloc_grain = 1;
	rpa->tail = l3;
	rpa->root = &n1->n_hdr;

	rpa_print(rpa);
	rpa_verify(rpa);

	size_t nelems = 1;
	rpa_pop(rpa, &nelems);
	assert(nelems == 1);

	rpa_print(rpa);
	rpa_verify(rpa);
	printf("OK\n");

	return 0;
}
