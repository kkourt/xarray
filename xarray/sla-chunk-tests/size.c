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
