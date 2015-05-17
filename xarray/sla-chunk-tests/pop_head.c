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
#include <assert.h>

#include "sla-chunk.h"
#include "sla-chunk-mm.h"

int main(int argc, const char *argv[])
{
	sla_t sla;
	sla_init(&sla, 5, 0.5);

	unsigned lvl;
	char *b;
	size_t l;

	char chunk1[16];
	unsigned lvl1 = 5;
	sla_node_t *n1 = do_sla_node_alloc(lvl1, chunk1, sizeof(chunk1));
	memset(chunk1, 'a', sizeof(chunk1));
	sla_append_node(&sla, n1, lvl1);
	l = sizeof(chunk1);
	b = sla_append_tailnode__(&sla, &l);
	assert(b == chunk1);
	assert(l == sizeof(chunk1));

	char chunk2[16];
	unsigned lvl2 = 3;
	sla_node_t *n2 = do_sla_node_alloc(lvl2, chunk2, sizeof(chunk2));
	memset(chunk1, 'a', sizeof(chunk2));
	sla_append_node(&sla, n2, lvl2);
	l = sizeof(chunk2);
	b = sla_append_tailnode__(&sla, &l);
	assert(b == chunk2);
	assert(l == sizeof(chunk2));

	sla_print(&sla);

	sla_node_t *n = sla_pop_head(&sla, &lvl);
	assert(n == n1);
	assert(lvl == lvl1);

	sla_print(&sla);
	return 0;
}
