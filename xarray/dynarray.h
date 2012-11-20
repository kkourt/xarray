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

#endif	/* DYNARRAY_H__ */
