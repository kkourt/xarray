#include <stdio.h>
#include <stddef.h> // size_t

#include "xarray.h"

// parameters
static unsigned long xarr_grain      = 64;
static unsigned long sla_max_level   = 5;
static float         sla_p = 0.5;

// create an xarray of integers
static xarray_t *
create_xarr_int(void)
{
	xarray_t *ret = xarray_create(&(struct xarray_init) {
		.elem_size = sizeof(int),
		.da = {
			.elems_alloc_grain = xarr_grain,
			.elems_init = xarr_grain,
		},
		.sla = {
			.p                =  sla_p,
			.max_level        =  sla_max_level,
			.elems_chunk_size =  xarr_grain,
		},
		.rpa = {
			.elems_alloc_grain = xarr_grain,
		}
	});
	assert(xarray_elem_size(ret) == sizeof(int));
	return ret;
}

static void
xarr_init(xarray_t *xarr, size_t count)
{
	// initialize array
	for (size_t  i=0; i<count; i++) {
		int *x_ptr = xarray_append(xarr);
		*x_ptr = (int)i;
	}
}

static void
xsl_test(xslice_t *xsl, size_t count, size_t sl_start, size_t sl_len)
{
	assert(xslice_size(xsl) == sl_len);

	size_t j0 = sl_start;
	for (;;) {
		size_t ch_nelems;
		int *chunk = xslice_getnextchunk(xsl, &ch_nelems);
		for (size_t j=0; j<ch_nelems; j++) {
			size_t idx = j0 + j;
			if (chunk[j] != idx) {
				printf("ERROR: chunk[%zd]=%d =/= %zd (=idx)\n",
				         j, chunk[j], idx);
				abort();
			}
		}

		if (ch_nelems == 0)
			break;

		j0 += ch_nelems;
	}

	assert(j0 == sl_len + sl_start);
}


int main(int argc, const char *argv[])
{
	const int count = 1024;
	xarray_t *xarr;

	xarr = create_xarr_int();

	xarr_init(xarr, count);
	assert(xarray_size(xarr) == count);

	// check slices
	for (size_t start=0; start<count; start++) {
		for (size_t len=1; len<=count-start; len++) {
			xslice_t xsl;
			xslice_init(xarr, start, len, &xsl);
			printf("checking slice: (%zd, +%zd)\n", start, len);
			xsl_test(&xsl, count, start, len);
		}
	}

	// check splits
	for (size_t start=0; start<count; start++) {
		for (size_t len=2; len<=count-start; len++) {
			xslice_t xsl, xsl1, xsl2;
			xslice_init(xarr, start, len, &xsl);

			printf("checking split on (%zd, +%zd)\n", start, len);
			xslice_split(&xsl, &xsl1, &xsl2);
			assert(xslice_size(&xsl1) == len / 2);
			assert(xslice_size(&xsl1) + xslice_size(&xsl2) == len);
			size_t start2 = start + xslice_size(&xsl1);

			printf("\t1st slice\n");
			xsl_test(&xsl1, count, start, xslice_size(&xsl1));

			printf("\t2nd slice\n");
			xsl_test(&xsl2, count, start2, xslice_size(&xsl2));
		}
	}

	printf("DONE!\n");
	return 0;
}
