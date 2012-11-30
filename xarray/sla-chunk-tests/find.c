#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "sla-chunk.h"


int main(int argc, const char *argv[])
{
	char buff[1024];
	sla_t sla;
	sla_init_seed(&sla, 5, 0.5, 2);
	for (unsigned i=0; i<sizeof(buff); i++) {
		buff[i] = 'a' + (i % ('z' - 'a' + 1));
	}

	sla_copyto_rand(&sla, buff, sizeof(buff), 10, 100);
	//sla_print_chars(&sla);
	sla_print(&sla); printf("\n");

	for (size_t idx=0; idx < sizeof(buff); idx++) {
		size_t off;
		sla_node_t *n = sla_find(&sla, idx, &off);
		char c1 = ((char *)n->chunk)[off];
		char c2 = buff[idx];
		printf("[idx:%3lu] %c--%c\n", idx, c1, c2);
		if (c1 != c2) {
			fprintf(stderr, "FAIL!\n");
			abort();
		}
	}

	return 0;
}
