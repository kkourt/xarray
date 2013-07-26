#include "rope_array.h"

void do_test(unsigned int cnt, unsigned int grain, bool bal)
{

	struct rpa *rpa = rpa_create(sizeof(long), grain);

	printf("Testing cnt=%u grain=%u bal=%u\n", cnt, grain, bal);
	printf(" Testing INSERT\n");
	for (unsigned int i=0; i<cnt; i++) {
		//printf("inserting i=%u\n", i);
		long *x = rpa_append(rpa);
		*x = (long)i;
	}
	rpa_check(rpa);

	printf(" Testing GET\n");
	for (unsigned int i=0; i<cnt; i++) {
		long x = *((long *)rpa_get(rpa, i));
		if (x != i) {
			printf("MISMATCH: i:%u rpa_get:%ld\n", i, x);
			abort();
		}
	}

	if (bal) {
		printf(" Trying Rebalancing [tree balanced? %d]\n",
			rpa_is_balanced(rpa->root));
		rpa_rebalance(rpa);
		assert(rpa_is_balanced(rpa->root));
		printf(" Testing GET\n");
		for (unsigned int i=0; i<cnt; i++) {
			long x = *((long *)rpa_get(rpa, i));
			if (x != i) {
				printf("MISMATCH: i:%u rpa_get:%ld\n", i, x);
				abort();
			}
		}
	}

	printf(" Testing POP\n");
	size_t pop = 1;
	for (unsigned int i=0; i<cnt; i++) {
		//printf("popping %u\n", i);
		rpa_pop(rpa, &pop);
		assert(pop == 1);
		rpa_check(rpa);
		//printf("popped %u\n", i);
		for (unsigned int j=0; j<cnt-i-1; j++) {
			//printf("  trying to get %d\n", j);
			long x = *((long *)rpa_get(rpa, j));
			if (x != j) {
				//printf("MISMATCH: j:%u rpa_get:%ld\n", j, x);
				abort();
			}
		}
	}
	printf("OK!\n");
}

int main(int argc, const char *argv[])
{
	do_test(32, 4, false);
	do_test(32, 4, true);

	return 0;
}
