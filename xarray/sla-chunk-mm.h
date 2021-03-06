/*
 * Copyright (c) 2012-2015, ETH Zurich.
 *
 * Released under a dual BSD 3-clause/GPL 2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 * http://opensource.org/licenses/BSD-3-Clause
 * http://opensource.org/licenses/GPL-2.0
 */

#ifndef SLA_CHUNK_MM
#define SLA_CHUNK_MM

#include "misc.h"
// interface

static void *sla_chunk_alloc(size_t size);
static void  sla_chunk_free(void *buff);

static sla_node_t *do_sla_node_alloc(unsigned lvl, void *chunk, size_t chunk_size);
static sla_node_t *sla_node_alloc(sla_t *sla, void *chunk, size_t chunk_size, unsigned *lvl_ret);
static void        sla_node_free(sla_node_t *node);

#if defined(SLA_MM_FREELISTS)

// list of nodes per level
struct sla_mm_node_list {
	sla_node_t *nodes_head;
	size_t      nodes_nr;
};

// one sla_mm per thread
struct sla_mm {
	// nodes
	struct sla_mm_node_list  mm_nodes_lists[];
	size_t                   mm_nodes_max_level;
	// chunks
	struct mm_chunk_list     mm_chunks;
};

static void
sla_mm_node_add(struct sla_mm_node_list *l, sla_node_t *node)
{
	SLA_NODE_NEXT(node, 0) = l->nodes_head;
	l->nodes_head = node;
	l->nodes_nr++;
}

static sla_node_t *
sla_mm_node_pop(struct sla_mm_node_list *l)
{
	sla_node_t *ret;

	if (l->nodes_nr == 0)
		return NULL;

	ret = l->nodes_head;
	l->nodes_head = SLA_NODE_NEXT(ret, 0);
	l->nodes_nr--;
	return ret;
}

static void
do_sla_mm_init(struct sla_mm *mm, size_t max_level, float p, size_t alloc_grain)
{
	// preload parameters
	size_t nodes_nr  = 1024;
	size_t chunks_nr = 1024;

	// initialize nodes
	mm->mm_nodes_lists     = xmalloc(max_level*sizeof(struct sla_mm_node_list));
	mm->mm_nodes_max_level = max_level;
	for (unsigned lvl=0; lvl < max_level; lvl++) {
		struct sla_mm_node_list *nlist = mm->mm_nodes_lists + lvl;
		nlist->head     = NULL;
		nlist->nodes_nr = 0;
		for (unsigned i=0; i<nodes_nr; i++) {
			sla_node_t *n = xmalloc(sla_node_size(lvl));
			n->lvl = lvl;
			sla_mm_node_add(nlist, n);
		}
		nodes_nr = MAX((size_t)ceilf((float)nodes_nr*p), 1);
	}

	// initialize chunks
	mm->mm_chunks.chunks_head = NULL;
	mm->mm_chunks.chunks_nr = 0;
	mm->mm_chunks.chunk_size = alloc_grain;
	for (unsigned i=0; i<chunks_nr; i++) {
		void *chunk = xmalloc(sizeof(alloc_grain));
		mm_chunk_add(&mm->mm_chunks, chunk);
	}
}

static __thread struct sla_mm Slamm;

#else /* malloc() implementation */

// ugly include for stat functions
//#include "rle_rec_stats.h"
//unsigned rle_getmyid(void);

static inline void *
sla_chunk_alloc(size_t size)
{
	void *ret;
	//RLE_TIMER_START(sla_chunk_alloc, rle_getmyid());
	ret = xmalloc(size);
	//RLE_TIMER_PAUSE(sla_chunk_alloc, rle_getmyid());
	return ret;
}

static inline void
sla_chunk_free(void *buff)
{
	free(buff);
}

/* allocate and initialize an sla node (takes level directly as argument) */
static inline sla_node_t *
do_sla_node_alloc(unsigned lvl, void *chunk, size_t chunk_size)
{
	//RLE_TIMER_START(sla_node_alloc, rle_getmyid());
	size_t size = sla_node_size(lvl);
	sla_node_t *ret = malloc(size);
	if (!ret)
		return NULL;
	sla_node_init(ret, lvl, chunk, chunk_size, 0);
	//RLE_TIMER_PAUSE(sla_node_alloc, rle_getmyid());
	return ret;
}

/* allocate and initialize an sla node */
static inline sla_node_t *
sla_node_alloc(sla_t *sla, void *chunk, size_t chunk_size, unsigned *lvl_ret)
{
	unsigned lvl = sla_rand_level(sla);
	if (lvl_ret)
		*lvl_ret = lvl;
	return do_sla_node_alloc(lvl, chunk, chunk_size);
}


static inline void
sla_node_free(sla_node_t *node)
{
	free(node);
}

#endif

#endif /* SLA_CHUNK_MM */
