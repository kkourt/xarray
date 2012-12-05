/*
 * dynnaray.h -- dynamically sized arrays
 *
 * Copyright (C) 2007-2011, Computing Systems Laboratory (CSLab), NTUA
 * Copyright (C) 2007-2011, Kornilios Kourtis
 * Copyright (C) 2010-2011, Vasileios Karakasis
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */
#ifndef DYNARRAY_H__
#define DYNARRAY_H__

struct dynarray;
typedef struct dynarray dynarray_t;

#ifdef __cplusplus
extern "C" {
#endif

dynarray_t *dynarray_create(unsigned long elem_size,
                            unsigned long alloc_grain,
                            unsigned long elems_nr);
dynarray_t *dynarray_init_frombuff(unsigned long elem_size,
                                   unsigned long alloc_grain,
                                   void *elems, unsigned long elems_nr);
void *dynarray_destroy(struct dynarray *da);

void dynarray_init(dynarray_t *da,
                   unsigned long elem_size,
                   unsigned long alloc_grain,
                   unsigned long elems_nr);
void *dynarray_finalize(dynarray_t *da);

void *dynarray_alloc(struct dynarray *da);
void dynarray_dealloc(struct dynarray *da);
void *dynarray_alloc_nr(struct dynarray *da, unsigned long nr);
void dynarray_align(struct dynarray *da, unsigned long align);
void *dynarray_alloc_nr_aligned(struct dynarray *da, unsigned long nr,
                                unsigned long align);
void dynarray_dealloc(struct dynarray *da);
void dynarray_dealloc_nr(struct dynarray *da, unsigned long nr);
void dynarray_dealloc_all(struct dynarray *da);
void *dynarray_get(struct dynarray *da, unsigned long idx);
void *dynarray_get_last(struct dynarray *da);
void dynarray_seek(struct dynarray *da, unsigned long idx);
static inline unsigned long dynarray_size(struct dynarray *da);
static inline unsigned long dynarray_elem_size(struct dynarray *da);

#ifdef __cplusplus
}
#endif

// private interface but we expose it here to get some fast inlines
struct dynarray {
	void *elems;
	unsigned long elems_nr;
	unsigned long elem_size;
	unsigned long next_idx;
	unsigned long alloc_grain;
};

static inline unsigned long
dynarray_size(struct dynarray *da)
{
	return da->next_idx;
}

static inline unsigned long
dynarray_elem_size(struct dynarray *da)
{
	return da->elem_size;
}

static inline void
dynarray_expand(struct dynarray *da)
{
	size_t total;
	da->elems_nr += da->alloc_grain;
	//printf("old addr: %lu	 ", (unsigned long)da->elems);
	//printf("expand realloc: %lu %lu %lu\n", da->next_idx, da->elems_nr, (da->next_idx+1)*da->elem_size);
	total = da->elem_size*da->elems_nr;
	da->elems = realloc(da->elems, total);
	if (!da->elems) {
		fprintf(stderr, "dynarray_expand: realloc failed [%lu]\n", total);
		abort();
	}
	//printf("new addr: %lu\n", (unsigned long)da->elems);
}


// only check out of bounds for actual memory allocated
static inline void *
dynarray_get__(struct dynarray *da, unsigned long idx)
{
	unsigned long addr = (unsigned long) da->elems;
	if (idx >= da->elems_nr) {
		fprintf(stderr, "dynarray_get__: out of bounds idx=%lu next_idx=%lu\n",
		                idx, da->next_idx);
		//print_trace();
		abort();
	}
	addr += da->elem_size*idx;
	//printf("dynarray_get: addr:%p next_idx:%lu idx:%lu returning 0x%lx\n", da->elems, da->next_idx, idx, addr);
	return (void *)addr;
}
#endif	/* DYNARRAY_H__ */
