#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "sla-chunk.h"

static void
print_ptr(sla_fwrd_t ptr[], unsigned cur_level)
{
	for (unsigned i=cur_level - 1; i<cur_level; i--) // backwards
		printf("  p[%u].node=%p p[%u].cnt=%3u\n", i, ptr[i].node, i, ptr[i].cnt);
}

static int
test_ptr_eq(sla_fwrd_t ptr1[], sla_fwrd_t ptr2[], unsigned cur_level)
{
	for (unsigned i=0; i<cur_level; i++) {
		if (ptr1[i].node != ptr2[i].node) {
			printf("ptr1[%u].node=%p =/= %p=ptr2[%u].node\n", i, ptr1[i].node, ptr2[i].node, i);
			return -1;
		}
		if (ptr1[i].cnt != ptr2[i].cnt) {
			printf("ptr1[%u].cnt=%u =/= %u=ptr2[%u].cnt\n", i, ptr1[i].cnt, ptr2[i].cnt, i);
			return -1;
		}
	}

	return 0;
}

static void
test_ptr(sla_t *sla, char *buff, sla_fwrd_t ptr[], size_t start, size_t end)
{
	for (size_t idx=start; idx < end; idx++) {
		size_t off;
		sla_node_t *n = sla_ptr_find(sla, ptr, idx, &off);
		char c1 = ((char *)n->chunk)[off];
		char c2 = buff[idx];
		//printf("[start:%3lu,idx:%3lu] %c--%c\n", start, idx, c1, c2);
		if (c1 != c2) {
			printf("[start:%3lu,idx:%3lu] %c--%c\n", start, idx, c1, c2);
			fprintf(stderr, "FAIL!\n");
			abort();
		}
	}
}

int main(int argc, const char *argv[])
{
	char buff[512];
	sla_t sla;

	sla_init_seed(&sla, 5, 0.5, 2);
	for (unsigned i=0; i<sizeof(buff); i++) {
		buff[i] = 'a' + (i % ('z' - 'a' + 1));
	}

	sla_copyto_rand(&sla, buff, sizeof(buff), 10, 100);
	//sla_print_chars(&sla);
	sla_print(&sla); printf("\n");

	sla_fwrd_t ptr[sla.cur_level];
	for (size_t ptr_i=0; ptr_i<sizeof(buff); ptr_i++) {
		sla_setptr(&sla, ptr_i, ptr);
		#if 0
		printf("PTR for index ptr_i=%lu:\n", ptr_i);
		for (unsigned i=sla.cur_level - 1; i<sla.cur_level; i--)
			printf("  p[%u].node=%p p[%u].cnt=%3u\n",
			       i, ptr[i].node,
			       i, ptr[i].cnt);
		#endif

		test_ptr(&sla, buff, ptr, ptr_i, sizeof(buff));

		sla_fwrd_t ptr2[sla.cur_level];
		sla_fwrd_t ptr1[sla.cur_level];
		for (size_t ptr2_i=ptr_i; ptr2_i<sizeof(buff); ptr2_i++) {
			sla_setptr(&sla, ptr2_i, ptr1);
			sla_ptr_setptr(&sla, ptr, ptr2_i, ptr2);
			if (test_ptr_eq(ptr1, ptr2, sla.cur_level) < 0) {
				printf("ptr_i=%lu ptr2_i=%lu\n", ptr_i, ptr2_i);
				printf("PTR:\n");  print_ptr(ptr, sla.cur_level);
				printf("PTR1:\n"); print_ptr(ptr1,sla.cur_level);
				printf("PTR2:\n"); print_ptr(ptr2,sla.cur_level);
				abort();
			}

			#if 0
			printf("PTR for index ptr_2=%lu:\n", ptr2_i);
			#endif
			//test_ptr(&sla, buff, ptr2, ptr2_i, sizeof(buff));
		}

	}

	printf("DONE!\n");
	return 0;
}
