#include <stdlib.h>
#include <time.h>

#include "xvarray.h"

// ugly include for stat functions
#include "rle_rec_stats.h"
DECLARE_RLE_STATS

// more ugly function definition
unsigned
rle_getmyid(void)
{
	return 0;
}

#define ALLOC_GRAIN 64
#define BUFF_SIZE   4096

int main(int argc, const char *argv[])
{
	char buff1[BUFF_SIZE];
	char buff2[BUFF_SIZE];
	xarray_t *xarr;

	rle_stats_create(1);
	rle_stats_init(1);

	memset(buff1, 'A', BUFF_SIZE);
	memcpy(buff2, buff1, BUFF_SIZE);

	xarr = xarray_create(&(struct xarray_init){
		.elem_size = sizeof(char),
		.da = {
			.elems_alloc_grain = ALLOC_GRAIN,
			.elems_init = BUFF_SIZE
		},
		.sla = {
			.p                =  .5,
			.max_level        =   5,
			.elems_chunk_size =  ALLOC_GRAIN,
		}
	});
	xarray_append_elems(xarr, buff1, BUFF_SIZE);

	xvarray_t *xv1 = xvarray_create(xarr);
	xvarray_t *xv2 = xvarray_branch(xv1);

	void do_test(void) {
		for (unsigned i=0; i<BUFF_SIZE; i++) {
			const char *c1, *c2;
			c1 = xvarray_get_rd(xv1, i);
			c2 = xvarray_get_rd(xv2, i);

			if (*c1 != buff1[i]) {
				printf("i=%u buff1=%c xv1=%c\n", i, buff1[i], *c1);
				abort();
			}
			if (*c2 != buff2[i]) {
				printf("i=%u buff2=%c xv2=%c\n", i, buff2[i], *c2);
				abort();
			}
		}
	}

	const int inserts = 32;
	unsigned seed = time(NULL);
	printf("initial seed = %u\n", seed);
	for (unsigned i=0; i<inserts; i++) {
		const char c = 'Z';
		char *ptr;
		int idx;

		idx = rand_r(&seed) % BUFF_SIZE;
		printf("inserting %c to %u\n", c, idx);
		ptr = xvarray_get_rdwr(xv2, idx);
		*ptr       = c;
		buff2[idx] = c;
		do_test();
	}

	return 0;
}
