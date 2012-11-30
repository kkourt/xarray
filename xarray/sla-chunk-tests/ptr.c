#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "sla-chunk.h"


int main(int argc, const char *argv[])
{
	char buff[256];
	sla_t sla;

	sla_init_seed(&sla, 5, 0.5, 2);
	for (unsigned i=0; i<sizeof(buff); i++) {
		buff[i] = 'a' + (i % ('z' - 'a' + 1));
	}

	sla_copyto_rand(&sla, buff, sizeof(buff), 10, 100);
	//sla_print_chars(&sla);
	sla_print(&sla); printf("\n");

	sla_node_t *ptr;
	ptr = do_sla_node_alloc(sla.cur_level, NULL, 0);

	for (size_t ptr_i=0; ptr_i<sizeof(buff); ptr_i++) {
		sla_ptr_set(&sla, ptr_i, ptr);
		#if 0
		printf("PTR:\n");
		for (unsigned i=sla.cur_level - 1; i<sla.cur_level; i--)
			printf("  p[%u].node=%p p[%u].cnt=%3u\n",
			       i, ptr->forward[i].node,
			       i, ptr->forward[i].cnt);
		#endif
		for (size_t idx=ptr_i; idx < sizeof(buff); idx++) {
			size_t off;
			sla_node_t *n = sla_ptr_find(&sla, ptr, idx, &off);
			char c1 = ((char *)n->chunk)[off];
			char c2 = buff[idx];
			printf("[ptr_i:%3lu,idx:%3lu] %c--%c\n", ptr_i, idx, c1, c2);
			if (c1 != c2) {
				fprintf(stderr, "FAIL!\n");
				abort();
			}
		}
	}

	return 0;
}
