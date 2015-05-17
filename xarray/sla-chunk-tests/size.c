/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include "sla-chunk.h"

int main(int argc, const char *argv[])
{
	for (unsigned i=1; i<16; i++) {
		size_t s = sla_node_size(i);
		void  *x = malloc(s);
		printf("%3u %4lu %4lu\n", i, s, malloc_usable_size(x));
		free(x);
	}
	return 0;
}
