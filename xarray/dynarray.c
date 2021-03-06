/*
 * dynnaray.c -- dynamically sized arrays
 *
 * Copyright (C) 2007-2011, Computing Systems Laboratory (CSLab), NTUA
 * Copyright (C) 2007-2011, Kornilios Kourtis
 * Copyright (C) 2010-2011, Vasileios Karakasis
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */
#include <assert.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>

#include "dynarray.h"

#define roundup(x, y) (                                 \
{                                                       \
        const typeof(y) __y = y;                        \
        (((x) + (__y - 1)) / __y) * __y;                \
}                                                       \
)


dynarray_t *dynarray_create(unsigned long elem_size,
                            unsigned long alloc_grain, unsigned long elems_nr)
{
	struct dynarray *da;
	da = malloc(sizeof(*da));
	if ( !da ) {
		fprintf(stderr, "dynarray_create: malloc\n");
		exit(1);
	}

	dynarray_init(da, elem_size, alloc_grain, elems_nr);
	return da;
}

void
dynarray_init(dynarray_t *da,
              unsigned long elem_size,
              unsigned long alloc_grain,
              unsigned long elems_nr)
{
	da->next_idx = 0;
	da->elem_size = elem_size;
	da->elems_nr = elems_nr ? roundup(elems_nr, alloc_grain) : alloc_grain;
	da->alloc_grain = alloc_grain;

	da->elems = malloc(elem_size*da->elems_nr);
	if ( !da->elems ){
		fprintf(stderr, "dynarray_create: malloc\n");
		exit(1);
	}
	//printf("da->elems: %p\n", da->elems);
}


/*
 * elems pointer should be subjectible to realloc() calls
 */
dynarray_t *dynarray_init_frombuff(unsigned long elem_size,
                                   unsigned long alloc_grain,
                                   void *elems, unsigned long elems_nr)
{
	struct dynarray *da = malloc(sizeof(struct dynarray));
	if (!da){
		fprintf(stderr, "dynarray_init_frombuff: malloc\n");
		exit(1);
	}

	da->next_idx = elems_nr;
	da->elem_size = elem_size;
	da->alloc_grain = alloc_grain;
	da->elems = elems;
	da->elems_nr = elems_nr;

	// fix buffer to be aligned with alloc_grain
	unsigned long rem = elems_nr % alloc_grain;
	if (rem != 0) {
		da->elems_nr += alloc_grain - rem;
		da->elems = realloc(da->elems, da->elem_size*da->elems_nr);
		if (!da->elems) {
			fprintf(stderr, "dynarray_init_from_buff: realloc failed\n");
			exit(1);
		}

		// In case da->elems is relocated.
		elems = da->elems;
	}

	return da;
}

void dynarray_seek(struct dynarray *da, unsigned long idx)
{
	if (idx >= da->next_idx) {
		fprintf(stderr, "dynarray_seek: out of bounds idx=%lu next_idx=%lu\n",
				idx, da->next_idx);
		exit(1);
	}

	da->next_idx = idx;
}

void *dynarray_get(struct dynarray *da, unsigned long idx)
{
	unsigned long addr = (unsigned long) da->elems;
	if (idx >= da->next_idx) {
		fprintf(stderr, "dynarray_get: out of bounds idx=%lu next_idx=%lu\n",
		                idx, da->next_idx);
		//print_trace();
		abort();
	}
	addr += da->elem_size*idx;
	//printf("dynarray_get: addr:%p next_idx:%lu idx:%lu returning 0x%lx\n", da->elems, da->next_idx, idx, addr);
	return (void *)addr;
}

void *dynarray_get_last(struct dynarray *da)
{
	return dynarray_get(da, da->next_idx-1);
}

void *dynarray_alloc(struct dynarray *da)
{
	void *ret;
	if (da->next_idx >= da->elems_nr) {
		assert(da->next_idx == da->elems_nr);
		dynarray_expand(da);
	}

	da->next_idx++;
	ret = dynarray_get_last(da);

	return ret;
}

void dynarray_dealloc(struct dynarray *da)
{
	da->next_idx--;
}

void *dynarray_alloc_nr(struct dynarray *da, unsigned long nr)
{
	void *ret;
	while (da->next_idx + nr >= da->elems_nr) {
		dynarray_expand(da);
	}

	unsigned long idx = da->next_idx;
	da->next_idx += nr;
	ret = dynarray_get(da, idx);

	return ret;
}

void dynarray_align(struct dynarray *da, unsigned long align)
{
	int nr_padd = (align - (da->next_idx  & (align-1))) & (align -1);
	while (da->next_idx + nr_padd >= da->elems_nr) {
		dynarray_expand(da);
	}

	da->next_idx += nr_padd;
}

void *dynarray_alloc_nr_aligned(struct dynarray *da,
                                unsigned long nr, unsigned long align)
{
	dynarray_align(da, align);
	return dynarray_alloc_nr(da, nr);
}

void dynarray_dealloc_nr(struct dynarray *da, unsigned long nr)
{
	if (da->next_idx < nr){
		fprintf(stderr, "dynarray_dealloc_nr: %lu %lu\n", da->next_idx, nr);
		exit(1);
	}
	da->next_idx -= nr;
}

void dynarray_dealloc_all(struct dynarray *da)
{
	da->next_idx = 0;
}

void *
dynarray_finalize(dynarray_t *da)
{
	void *ret = da->elems;
	ret = realloc(ret, da->next_idx*da->elem_size);
	return ret;
}

void *
dynarray_destroy(struct dynarray *da)
{
	//printf("destroy realloc: idx:%lu nr:%lu realloc size:%lu\n", da->next_idx, da->elems_nr, (da->next_idx+1)*da->elem_size);
	void *ret = dynarray_finalize(da);
	free(da);
	return ret;
}
