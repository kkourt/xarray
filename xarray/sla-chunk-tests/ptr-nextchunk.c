#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include "sla-chunk.h"

static inline void
print_ptr(sla_fwrd_t ptr[], unsigned cur_level)
{
	for (unsigned i=cur_level - 1; i<cur_level; i--) // backwards
		printf("  p[%u].node=%p p[%u].cnt=%3u\n", i, ptr[i].node, i, ptr[i].cnt);
}

static inline int
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
test_ptr_nextchunk(sla_t *sla, char *buff, sla_fwrd_t ptr[], size_t start, size_t end)
{
	size_t idx=start;
	for (;;) {
		size_t node_key;
		sla_node_t *n = sla_ptr_nextchunk(sla, ptr, &node_key);
		if (n == NULL)
			break;
		assert(idx >= node_key);
		for (size_t k=idx-node_key; k<n->chunk_size; k++) {
			//printf("k=%lu idx=%lu\n", k, idx);
			char c1 = ((char *)n->chunk)[k];
			char c2 = buff[idx];
			if (c1 != c2) {
				printf("[start:%3lu,idx:%3lu] %c--%c\n", start, idx, c1, c2);
				fprintf(stderr, "FAIL!\n");
				abort();
			}
			idx++;
			if (idx == end)
				break;

		}

		if (idx < end) {
			printf("checking for idx=%lu\n", idx);
			sla_fwrd_t ptr2[sla->cur_level];
			sla_setptr(sla, idx, ptr2);
			if (sla_ptr_equal(ptr, ptr2, sla->cur_level) < 0) {
				printf("ERROR for idx=%lu\n", idx);
				printf("--------------------------------------------\n");
				printf("nextchunk pointer:\n"); sla_ptr_print(ptr,  sla->cur_level);
				printf("idx pointer:      \n"); sla_ptr_print(ptr2, sla->cur_level);
				printf("--------------------------------------------\n");
				abort();
			} else {
				printf("idx pointer:      \n"); sla_ptr_print(ptr2, sla->cur_level);
			}
		}
		//printf("\n");
	}
	assert(idx == end);
}

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

	sla_fwrd_t ptr[sla.cur_level];
	for (size_t ptr_i=0; ptr_i<sizeof(buff); ptr_i++) {
		sla_setptr(&sla, ptr_i, ptr);
		printf("ptr for %lu\n", ptr_i); print_ptr(ptr, sla.cur_level);
		test_ptr_nextchunk(&sla, buff, ptr, ptr_i, sizeof(buff));
	}

	printf("DONE!\n");
	return 0;
}
