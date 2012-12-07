#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(__linux__)
#include <time.h>
#endif

#include "misc.h"
#include "sla-chunk.h"

/**
 *  |---------------------------------->|
 *  |------------------->|------------->|
 *  |--->|-------------->|------------->|
 *  |--->|------>|------>|------>|------|
 *  H  k0,v0   k1,v1   k2,v2   k3,v3    T
 *                               |<-----|
 *                       |<-------------|
 *                       |<-------------|
 *  |<----------------------------------|
 *
 *
 * http://en.wikipedia.org/wiki/Skip_list
 * http://algolist.ru/ds/skiplist.c
 */

#define MAX(x,y) (x > y ? x : y)
#define MIN(x,y) (x < y ? x : y)

/* randf(): return a random float number in [0,1] */
static inline float
randf(unsigned int *seed)
{
	return ((float)rand_r(seed))/((float)RAND_MAX);
}

static inline int
randi(unsigned int *seed, size_t i_max, size_t i_min)
{
	return (rand_r(seed) % (i_max - i_min)) + i_min;
}

/* sla_rand_level: return a random level for a new node */
unsigned
sla_rand_level(sla_t *sla)
{
	int ret;
	unsigned int *sd = &sla->seed;
	for (ret=1; (ret < sla->max_level) && (sla->p > randf(sd)); ret++)
		;
	return ret;
}

void
sla_node_init(sla_node_t *node, unsigned lvl,
              void *chunk, size_t chunk_size,
              size_t node_size)
{
	node->chunk = chunk;
	node->chunk_size = chunk_size;
	for (unsigned i=0; i < lvl; i++) {
		SLA_NODE_CNT(node, i) = node_size;
	}
}

/* allocate and initialize an sla node (takes level directly as argument) */
sla_node_t *
do_sla_node_alloc(unsigned lvl, void *chunk, size_t chunk_size)
{
	size_t size = sla_node_size(lvl);
	sla_node_t *ret = malloc(size);
	if (!ret)
		return NULL;
	sla_node_init(ret, lvl, chunk, chunk_size, 0);
	return ret;
}

/* allocate and initialize an sla node */
sla_node_t *
sla_node_alloc(sla_t *sla, void *chunk, size_t chunk_size, unsigned *lvl_ret)
{
	unsigned lvl = sla_rand_level(sla);
	if (lvl_ret)
		*lvl_ret = lvl;
	return do_sla_node_alloc(lvl, chunk, chunk_size);
}


void
sla_node_dealloc(sla_node_t *node)
{
	free(node);
}

/* initialize an sla, but without allocating head/tail or changing parameters */
static void
do_sla_init(sla_t *sla)
{
	sla->cur_level = 1;
	sla->total_size = 0;

	for (unsigned i=0; i<sla->cur_level; i++) {
		SLA_HEAD_NODE(sla, i) = sla->tail;
		SLA_TAIL_NODE(sla, i) = sla->head;
		SLA_TAIL_CNT(sla, i) = 0;
	}
}


void
sla_init_seed(sla_t *sla, unsigned max_level, float p, int seed)
{
	/* head and tail are guard nodes, so they should have pointers for all the
	 * levels. If we need to update the max_level, we need to update these as
	 * well. Since they are dummy nodes, they don't contain any items */
	sla->head = do_sla_node_alloc(max_level + 1, NULL, 0);
	sla->tail = do_sla_node_alloc(max_level + 1, NULL, 0);
	sla->p = p;
	sla->seed = seed;
	sla->max_level = max_level;
	do_sla_init(sla);
}

/* initialize a skiplist array */
void
sla_init(sla_t *sla, unsigned max_level, float p)
{
	sla_init_seed(sla, max_level, p, time(NULL));
}

void
sla_destroy(sla_t *sla)
{
	free(sla->head);
	free(sla->tail);
}

void sla_verify(sla_t *sla)
{
	for (unsigned int i = 0; i > sla->cur_level; i++) {
		unsigned int cnt = 0;
		sla_node_t *n;
		sla_for_each(sla, i, n) {
			cnt += SLA_NODE_CNT(n, i);
			assert(cnt <= sla->total_size);
		}

		cnt += SLA_TAIL_CNT(sla, i);
		assert(cnt == sla->total_size);
	}
	assert(SLA_TAIL_CNT(sla, 0) == 0);
}

void
sla_print(sla_t *sla)
{
	printf("sla: %p total_size: %lu cur_level: %u\n", sla, sla->total_size, sla->cur_level);

	for (int i = sla->cur_level - 1; i >= 0; i--) {
		unsigned long cnt __attribute__((unused));
		cnt = 0;

		printf("L%-4d    ", i);
		printf("|---------");

		sla_node_t *x = sla->head;
		while ((x = sla_node_next(x, 0)) != sla_node_next(sla->head, i)) {
			printf("----------------");
			//assert(cnt++ < sla->total_size);
		}
		printf(">");

		sla_node_t *n;
		cnt = 0;
		sla_for_each(sla, i, n) {
			printf("(%4u     )----", SLA_NODE_CNT(n, i));
			//assert(cnt++ < sla->total_size);
			sla_node_t *z = n;
			while ((z = sla_node_next(z, 0)) != sla_node_next(n, i)) {
			printf("----------------");
				//assert(cnt++ < sla->total_size);
			}
			printf(">");
		}
		printf("| %u\n", SLA_NODE_CNT(sla->tail, i));
	}

	sla_node_t *n;
	printf("    (%9p)", sla->head);
	sla_for_each(sla, 0, n) {
		printf("     (%9p)", n);
	}
	printf("     (%9p)", sla->tail);
	printf("\n");

}

static sla_node_t *
sla_do_find(sla_node_t *node, int lvl, size_t idx, size_t key, size_t *chunk_off)
{
	size_t next_e, next_s;

	do { /* iterate all levels */
		for (;;) {
			/* next node covers the range: [next_s, next_e) */
			sla_node_t *next = sla_node_next(node, lvl);
			next_e = idx + SLA_NODE_CNT(next, lvl);
			next_s = next_e - SLA_NODE_NITEMS(next);

			/* key is before next node, go down a level */
			if (key < next_s)
				break;

			/* sanity check */
			if (next->chunk == NULL) {
				printf("something's wrong: next:%p lvl:%d key:%lu\n", next, lvl, key);
				abort();
			}

			node = next;

			/* key is in [next_s, next_e), return node */
			if (key < next_e)
				goto end;

			/* key is after next node, continue iteration */
			idx = next_e;
		}
	} while (--lvl >= 0);
	assert(0 && "We didn't found a node -- something's wrong");
end:
	if (chunk_off)
		*chunk_off = key - next_s;
	return node;
}

/* find the node responsible for key.
 *  - return the node that contains the key
 *  - the offset of the value in this node is placed on offset */
sla_node_t *
sla_find(sla_t *sla, size_t key, size_t *chunk_off)
{
	if (key >= sla->total_size)
		return NULL;

	return sla_do_find(sla->head, sla->cur_level -1, 0, key, chunk_off);
}

/* sla_traverse set the update path for a specific key, and return count of the
 * last node */
static void
sla_traverse(sla_t *sla, unsigned long key, sla_fwrd_t update[])
{
	int i             = sla->cur_level - 1;
	sla_node_t *n     = sla->head;
	unsigned long idx = 0;

	assert(key < sla->total_size);
	do {
		for (;;) {
			sla_node_t *next = sla_node_next(n, i);
			unsigned long next_idx = idx + SLA_NODE_CNT(next, i);

			/*  we need to go down a level */
			if (key < next_idx)
				break;

			/* tail adds up to the number of nodes. If we've reached
			 * tail then it should be that next_idx > key  */
			assert(next != sla->tail);

			n = next;
			idx = next_idx;
		}
		update[i].node = n;
		update[i].cnt  = idx;
	} while(--i >= 0);
}

/* add a node to the front of the sla */
void
sla_add_node_head(sla_t *sla, sla_node_t *node, int node_lvl)
{
	if (sla->cur_level < node_lvl) {
		for (unsigned i=sla->cur_level; i < node_lvl; i++) {
			SLA_HEAD_NODE(sla, i) = node;
			SLA_NODE_NEXT(node, i) = sla->tail;
			SLA_TAIL_NODE(sla, i) = node;
			SLA_TAIL_CNT(sla, i) = sla->total_size;
		}
		sla->cur_level = node_lvl;
	}

	unsigned i;
	for (i=0; i<node_lvl; i++) {
		assert(SLA_HEAD_NODE(sla, i) != sla->tail);
		SLA_NODE_NEXT(node, i) = SLA_HEAD_NODE(sla, i);
		SLA_HEAD_NODE(sla, i) = node;
	};

	for (   ; i<sla->cur_level; i++) {
		sla_node_t *h = SLA_HEAD_NODE(sla, i);
		assert(h != sla->tail); // not an empty level
		SLA_NODE_CNT(h, i) += SLA_NODE_NITEMS(node);
	}

	sla->total_size += SLA_NODE_NITEMS(node);
}

/* decapitate sla */
sla_node_t *
sla_pop_head(sla_t *sla, unsigned *lvl_ret)
{
	sla_node_t *ret = SLA_HEAD_NODE(sla, 0);

	if (ret == sla->tail) // sla empty
		return NULL;

	// find node level first
	// (we could do it on the loops below, but it gets complicated)
	if (lvl_ret) {
		unsigned lvl = 0;
		while (SLA_HEAD_NODE(sla, ++lvl) == ret)
			;
		*lvl_ret = lvl;
	}

	if (SLA_NODE_NEXT(ret, 0) == sla->tail) { // ret the only node in sla
		assert(SLA_NODE_NITEMS(ret) == sla->total_size);
		do_sla_init(sla);
		return ret;
	}

	assert(SLA_NODE_NITEMS(ret) < sla->total_size);
	unsigned i;
	for (i=0; i<sla->cur_level; i++) { // iterate node levels
		if (SLA_HEAD_NODE(sla, i) != ret)
			break;
		sla_node_t *nxt = SLA_NODE_NEXT(ret, i);
		SLA_HEAD_NODE(sla, i) = nxt;
		if (nxt == sla->tail) {
			assert(i != 0);
			// level is empty, update current level
			sla->cur_level = i;
			break;
		}
	}

	for ( ;i<sla->cur_level; i++) { // itereate remaining levels
		struct sla_node *h __attribute__((unused)) = SLA_HEAD_NODE(sla, i);
		unsigned int ncnt = SLA_NODE_CNT(ret, i);
		assert(SLA_NODE_CNT(h,i) >= ncnt);
		SLA_NODE_CNT(SLA_HEAD_NODE(sla, i), i) -= ncnt;
	}

	sla->total_size -= SLA_NODE_NITEMS(ret);
	return ret;
}

/* append a node to the end of the sla */
void
sla_append_node(sla_t *sla, sla_node_t *node, int node_lvl)
{

	/* ugly trick that lazily sets up un-initialized levels */
	if (sla->cur_level < node_lvl) {
		for (unsigned i = sla->cur_level; i < node_lvl; i++) {
			SLA_HEAD_NODE(sla, i) = node;
			SLA_NODE_NEXT(node, i) = sla->tail;
			SLA_TAIL_NODE(sla, i)  = node;
			SLA_NODE_CNT(node, i) += sla->total_size;
			SLA_TAIL_CNT(sla, i) = 0;
		}
		unsigned int new_level = node_lvl;
		node_lvl = sla->cur_level; // set levels for next iteration
		sla->cur_level = new_level;
	}

	unsigned i;
	for (i=0; i<node_lvl; i++) {
	/* update links */
		sla_node_t *last_i = SLA_TAIL_NODE(sla, i);
		assert(SLA_NODE_NEXT(last_i, i) == sla->tail);
		SLA_NODE_NEXT(last_i, i) = node;
		SLA_NODE_NEXT(node, i) = sla->tail;
		SLA_TAIL_NODE(sla, i) = node;

		/* for each level of the node we increase its count by the count of the
		 * tail -- i.e., the number of items between this node and the previous
		 * one */
		SLA_NODE_CNT(node, i) += SLA_TAIL_CNT(sla, i);
		/* there is nothing between the tail and the last nodef or this level:
		 * tail's count is zero */
		SLA_TAIL_CNT(sla, i) = 0;
	}

	/* for the remaining levels -- the tail count should be increased by the
	 * node's count */
	for ( ; i<sla->cur_level; i++) {
		SLA_TAIL_CNT(sla, i) += SLA_NODE_NITEMS(node);
	}

	sla->total_size += SLA_NODE_NITEMS(node);
}


/**
 * sla_split_coarse: split a skip-list array on node level
 *  @sla:    initial sla -- if sla1 is NULL, on return contains first half
 *  @sla1:   uninitialized sla -- if not NULL, on return contains first half
 *  @sla2:   uninitialized sla -- on return contains the second half
 *  @offset: offset to do the splitting
 *  item at @offset will end up on the first node of sla2
 */
void
sla_split_coarse(sla_t *sla, sla_t *sla1, sla_t *sla2, size_t offset)
{
	assert(offset > 0); assert(offset < sla->total_size);

	sla_fwrd_t update[sla->max_level];
	sla_traverse(sla, offset, update);
	sla_init(sla2, sla->max_level, sla->p);
	/* current levels will (possibly) need re-adjustment */
	unsigned int sla1_cur_level=1, sla2_cur_level=1;
	assert(sla->cur_level > 0);
	/* to compute the current levels, we start from the top levels*/
	for (int i=sla->cur_level - 1; i >= 0; i--) {
		/* sla1: the last node for sla1 for this level
		 * sla2: the first node for sla2 for this level
		 * dcnt: cnt between sla1 and split point for this level */
		struct sla_node *sla1_n = update[i].node;
		struct sla_node *sla2_n = SLA_NODE_NEXT(sla1_n, i);
		unsigned int    dcnt = update[0].cnt - update[i].cnt;

		// update sla2 head node
		SLA_HEAD_NODE(sla2, i) = sla2_n;
		assert(SLA_NODE_CNT(sla2_n, i) > dcnt);
		SLA_NODE_CNT(sla2_n, i) -= dcnt;

		// The sla2 tail node for this level is the tail node of sla unless this
		// node belings to sla1, which means that this level is empty for sla2
		sla_node_t *ti = SLA_TAIL_NODE(sla, i);
		if (ti != sla1_n) {
			SLA_NODE_NEXT(ti, i) = sla2->tail;
			SLA_TAIL_NODE(sla2, i) = ti;
			SLA_TAIL_CNT(sla2, i) = SLA_TAIL_CNT(sla, i);
			// check if this is the first non-empty level from the top
			if (sla2_cur_level == 1)
				sla2_cur_level = i + 1;
		}

		// update sla1 tail and sla1 current level
		SLA_NODE_NEXT(sla1_n, i) = sla->tail;
		SLA_TAIL_NODE(sla, i) = sla1_n;
		SLA_TAIL_CNT(sla, i) = dcnt;
		if (sla1_cur_level == 1 && SLA_HEAD_NODE(sla, i) != sla->tail) {
			sla1_cur_level = i + 1;
		}
	}

	if (sla1 == NULL)
		sla1 = sla;
	else
		sla_init(sla1, sla->max_level, sla->p);

	assert((sla1_cur_level <= sla1->cur_level) && sla1_cur_level);
	assert(sla2_cur_level);
	sla1->cur_level = sla1_cur_level;
	sla2->cur_level = sla2_cur_level;

	sla2->total_size = sla1->total_size - update[0].cnt;
	sla1->total_size = update[0].cnt;

	return;
}

/* TODO: heuristically copy items */
/*  sla1 will contain the concatenated sla
 *  sla2 will be empty
 */
void
sla_concat(sla_t *sla1, sla_t *sla2)
{
	assert(sla1->max_level == sla2->max_level &&
	       "FIXME: realloc head and tail pointers");

	unsigned int new_level = MAX(sla1->cur_level, sla2->cur_level);
	for (unsigned int i = 0; i < new_level; i++) {
		if ( i >= sla1->cur_level ) {       /* invalid level for sla1 */
			sla_node_t *sla2_fst = SLA_HEAD_NODE(sla2, i);
			SLA_HEAD_NODE(sla1, i) = sla2_fst;
			SLA_NODE_CNT(sla2_fst, i) += sla1->total_size;
		} else if ( i >= sla2->cur_level) { /* invalid level for sla2 */
			sla_node_t *sla1_lst = SLA_TAIL_NODE(sla1, i);
			SLA_NODE_NEXT(sla1_lst, i) = sla2->tail;
			SLA_TAIL_NODE(sla2, i) = sla1_lst;
			SLA_TAIL_CNT(sla2, i) = sla2->total_size + SLA_TAIL_CNT(sla1, i);
		} else {                            /* valid level for both */
			sla_node_t *sla1_lst = SLA_TAIL_NODE(sla1, i);
			sla_node_t *sla2_fst = SLA_HEAD_NODE(sla2, i);
			SLA_NODE_NEXT(sla1_lst, i) = sla2_fst;
			SLA_NODE_CNT(sla2_fst, i) += SLA_TAIL_CNT(sla1, i);
		}
	}

	sla1->cur_level = new_level;
	sla_node_t *sla1t = sla1->tail;
	sla1->tail = sla2->tail;
	sla1->total_size += sla2->total_size;

	/* leave sla2 in a consistent state */
	sla2->tail = sla1t;
	do_sla_init(sla2);
}

/**
 * pop the tail node
 *  This is ugly and has performance issues. Hopefully it won't be called often
 *  Alternative options:
 *   - don't deallocate nodes => won't work: how would you do append?
 *   - double link list
 */
sla_node_t *
sla_pop_tail(sla_t *sla)
{
	sla_verify(sla);
	sla_node_t *n = SLA_TAIL_NODE(sla, 0);
	sla_node_t *h = SLA_HEAD_NODE(sla, 0);
	size_t nlen = SLA_NODE_NITEMS(n);

	if (h == n) {
		//only one node in the list
		do_sla_init(sla);
		return n;
	}

	/**
	 * ugly part: for all @n's levels -- i.e., i levels such for which:
	 *  SLA_TAIL_NODE(sla, i) == SLA_TAIL_NODE(sla, 0)
	 * We need to find the node before n, which will become the new tail
	 *
	 * To do that, we start from @p, which is the first tail node we can
	 * find (as we go up the levels) that is not @n. We then, do a linear
	 * search for @n starting from @p for all @n's levels. If @n has
	 * @cur_level leafs, we start with the head node.
	 */

	/* find @p and @n_levels -- i.e n's number of levels */
	sla_node_t *p = NULL;
	unsigned n_levels=1;
	for (unsigned i=1; i<sla->cur_level; i++) {
		sla_node_t *x = SLA_TAIL_NODE(sla, i);
		if (x != n) {
			p = x;
			break;
		}
		n_levels++;
	}

	if (p == NULL) {
		unsigned i;
		for (i=n_levels-1; i<n_levels; i--) { // backwards
			p = SLA_HEAD_NODE(sla, i);
			if (p != n)
				break;
		}

		if (n_levels == sla->cur_level) {
			sla->cur_level = i + 1;
			n_levels = sla->cur_level;
		}
	}
	assert(p != n && p != NULL);

	/**
	 * go over all @n's levels and fix pointers and counts.
	 * Update @p as we find nodes that are closer to @n.
	 */
	for (unsigned i=n_levels-1; i<n_levels; i--) { // backwards
		// find @n's previous node for this level.
		sla_node_t *x;
		for (x = p; x != n; x = sla_node_next(x, i)) {
			assert(x != sla->tail);
			assert(x != NULL);
			p = x;
		}

		assert(p != n);
		assert(SLA_NODE_NEXT(p, i)   == n);
		assert(SLA_TAIL_NODE(sla, i) == n);
		assert(SLA_NODE_NEXT(n, i)   == sla->tail);
		assert(SLA_TAIL_CNT(sla, i)  == 0);
		assert(nlen <= SLA_NODE_CNT(n, i));

		// fix tail count: there might be other nodes with smaller
		// levels than the one we currently are. We need to update tail
		// count to the total count of these nodes. We find this value
		// by substracting @n's elelemts for its count on this level
		SLA_TAIL_CNT(sla, i) = SLA_NODE_CNT(n, i) - nlen;
		// fix pointers: we need to fix tail pointers and node pointers
		SLA_TAIL_NODE(sla, i) = p;
		SLA_NODE_NEXT(p, i) = sla->tail;
	}

	/**
	 * for the remaining levels, we only need to fix tail count
	 */
	for (unsigned i=n_levels; i<sla->cur_level; i++) {
		assert(SLA_TAIL_NODE(sla, i) != n);
		assert(SLA_TAIL_CNT(sla, i) >= nlen);
		SLA_TAIL_CNT(sla, i) -= nlen;
	}

	sla->total_size -= nlen;
	sla_verify(sla);
	return n;
}

char *
sla_pop_tailnode(sla_t *sla, size_t *len)
{
	char *ret;
	sla_node_t *n = SLA_TAIL_NODE(sla, 0);
	size_t nlen = SLA_NODE_NITEMS(n);
	assert(nlen > 0);
	if (nlen <= *len) {
		// pop the tail node
		sla_pop_tail(sla);
		assert(n->chunk_size >= nlen);
		*len = nlen;
		sla_node_dealloc(n);
		ret = n->chunk;
	} else {
		// just update counters
		for (unsigned i=0; i<sla->cur_level; i++) {
			if (SLA_TAIL_NODE(sla, i) == n) {
				SLA_NODE_CNT(n, i) -= *len;
				assert(SLA_TAIL_CNT(sla, i) == 0);
			} else
				SLA_TAIL_CNT(sla, i) -= *len;
		}
		ret = n->chunk + (nlen - *len);
		sla->total_size -= *len;
	}

	return ret;
}

/**
 * return a pointer that can fit @len bytes
 *   @len initially contains the size requested by the user
 *   @len finally contains teh size that the user is allowed to write
 * if @len == 0, function returns NULL
 */
char *
sla_append_tailnode__(sla_t *sla, size_t *len)
{
	sla_node_t *n = SLA_TAIL_NODE(sla, 0);
	size_t nlen = SLA_NODE_NITEMS(n);
	if (n == sla->head || nlen == n->chunk_size) {
		*len = 0;
		return NULL;
	}

	assert(nlen < n->chunk_size);
	size_t clen = MIN(n->chunk_size - nlen, *len);

	for (unsigned i=0; i<sla->cur_level; i++) {
		if (SLA_TAIL_NODE(sla, i) == n)
			SLA_NODE_CNT(n, i) += clen;
		else
			SLA_TAIL_CNT(sla, i) += clen;
	}

	sla->total_size += clen;
	*len = clen;
	return n->chunk + nlen;
}

/* append only to the tail node. Return the amount of data copied */
size_t
sla_append_tailnode(sla_t *sla, char *buff, size_t len)
{
	size_t clen = len;
	char *dst = sla_append_tailnode__(sla, &clen);

	if (dst != NULL) {
		assert(clen > 0);
		memcpy(dst, buff, clen);
	} else
		assert(clen == 0);

	return clen;
}

void
sla_copyto(sla_t *sla, char *src, size_t len, size_t alloc_grain)
{
	size_t clen;

	clen = sla_append_tailnode(sla, src, len);
	src += clen;
	len -= clen;

	while (len > 0) {
		assert(sla_tailnode_full(sla));
		char *buff = xmalloc(alloc_grain);
		unsigned lvl;
		sla_node_t *node = sla_node_alloc(sla, buff, alloc_grain, &lvl);
		sla_append_node(sla, node, lvl);
		clen = sla_append_tailnode(sla, src, len);
		assert(clen > 0);
		src += clen;
		len -= clen;
	}
}

// this is for testing
void
sla_copyto_rand(sla_t *sla, char *src, size_t len,
                size_t alloc_grain_min, size_t alloc_grain_max)
{
	size_t clen;

	clen = sla_append_tailnode(sla, src, len);
	src += clen;
	len -= clen;

	unsigned int seed = sla->seed;
	while (len > 0) {
		assert(sla_tailnode_full(sla));
		size_t size = randi(&seed, alloc_grain_max, alloc_grain_min);
		char *buff = xmalloc(size);
		unsigned lvl;
		sla_node_t *node = sla_node_alloc(sla, buff, size, &lvl);
		sla_append_node(sla, node, lvl);
		clen = sla_append_tailnode(sla, src, len);
		assert(clen > 0);
		src += clen;
		len -= clen;
	}
}

/**
 * Ponters
 *
 * Ponters are special forward pointer structures, used to implement slices.
 * Essentially they allow to start a search from a different point in the SLA.
 * They are similar to the forward pointers in head nodes, but they have a
 * meaningful cnt. The cnt is the _total_ cnt from the original head of the sla.
 *
 * head nodes have a size to accomodate ->max_level, since pointers do not
 * change, having a size to accomodate ->cur_level should work OK.
 *
 * Pointers will not work correctly if you break the structure of an SLA --
 * i.e., by doing an SLA split. They will probably work OK if you just append
 * data. Note that in this case, there will a benign incosistency when nodes are
 * added with a level for which the pointer points to tail.
 * XXX: Another structural issue seems to be changing ->cur_level. Need to check
 * up on that...
 */

static void
sla_dosetptr(sla_node_t *node, size_t key, int lvl, size_t idx, sla_fwrd_t ptr[])
{
	size_t next_e;
	int i = lvl;
	do { // iterate all levels
		for (;;) {
			/* next node covers the range: [next_s, next_e) */
			sla_node_t *next = sla_node_next(node, i);
			next_e = idx + SLA_NODE_CNT(next, i);
			//next_s = next_e - SLA_NODE_NITEMS(next);
			// key is before next node, go down a level
			if (key < next_e) {
				ptr[i].node = next;
				ptr[i].cnt = idx;
				break;
			}
			// not-so-sane way of checking whether we reached tail
			if (next->chunk == NULL) {
				printf("something's wrong: next:%p i:%d key:%lu\n", next, i, key);
				abort();
			}

			/* key is after next node, continue */
			node = next;
			idx = next_e;
		}
	} while (--i >=0);
	return;
}

// set a pointer to start at @idx
void
sla_setptr(sla_t *sla, size_t key, sla_fwrd_t ptr[])
{
	assert(key < sla->total_size);
	sla_dosetptr(sla->head, key, sla->cur_level - 1, 0, ptr);
}

// find a node based on a pointer
sla_node_t *
sla_ptr_find(sla_t *sla, sla_fwrd_t ptr[], size_t key, size_t *chunk_off)
{
	assert(key >= ptr[0].cnt);
	assert(key <  sla->total_size);

	size_t x_start, x_end;
	sla_node_t *ret, *x=NULL;

	// find an actual node to start search from
	int cur_lvl = sla->cur_level, lvl;
	for (lvl = cur_lvl -1; lvl < cur_lvl; lvl--) { // backwards
		x = ptr[lvl].node;
		x_end = ptr[lvl].cnt + SLA_NODE_CNT(x, lvl);
		x_start = x_end - SLA_NODE_CNT(x, 0);
		//printf("x_start=%lu x_end=%lu lvl=%d\n", x_start, x_end, lvl);
		if (key >= x_end) { // key beyond x
			break;
		} else if (key >= x_start) { // key in this node
			if (chunk_off)
				*chunk_off = key - x_start;
			ret = x;
			goto end;
		}
	}
	size_t idx = x_end;
	//printf("x=%p lvl=%d idx=%lu\n", x, lvl, idx);
	ret = sla_do_find(x, lvl, idx, key, chunk_off);
	assert(lvl < cur_lvl); // we should not run out of levels
end:
	return ret;
}

sla_node_t *
sla_ptr_nextchunk(sla_t *sla, sla_fwrd_t ptr[], size_t *node_key)
{
	sla_node_t *node = ptr[0].node, *next;
	if (node == sla->tail)
		return NULL;

	*node_key = ptr[0].cnt;
	for (int lvl=0; lvl < sla->cur_level; lvl++) {
		if (ptr[lvl].node != node)
			break;

		ptr[lvl].node = next = SLA_NODE_NEXT(node, lvl);
		ptr[lvl].cnt += SLA_NODE_CNT(node, lvl);
		               //+ SLA_NODE_CNT(next, lvl) - SLA_NODE_CNT(next, 0);
	}

	return node;
}

// ptr_out == ptr_in seems be to OK, but need to test it
void
sla_ptr_setptr(sla_t *sla, const sla_fwrd_t ptr_in[], size_t key,
               sla_fwrd_t ptr_out[])
{
	assert(key >= ptr_in[0].cnt);
	assert(key <  sla->total_size);

	size_t x_start, x_end, idx;
	sla_node_t *x=NULL;

	// find an actual node to start search from
	int lvl = sla->cur_level - 1;
	do { // iterate all levels
		x   = ptr_in[lvl].node;
		idx = ptr_in[lvl].cnt;
		for (;;) {
			x_end = idx + SLA_NODE_CNT(x, lvl);
			x_start = x_end - SLA_NODE_CNT(x, 0);
			if (key >= x_end) {
				assert(x != sla->tail);
				x   = SLA_NODE_NEXT(x, lvl);
				idx = x_end;
				continue;
			} else if (key >= x_start) {
				ptr_out[lvl].node = x;
				ptr_out[lvl].cnt = idx;
				break;
			} else {
				ptr_out[lvl].node = x;
				ptr_out[lvl].cnt = idx;
				break;
			}
		}
	} while (--lvl >= 0);

	return;
}

int
sla_ptr_equal(sla_fwrd_t ptr1[], sla_fwrd_t ptr2[], unsigned cur_level)
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

void
sla_ptr_print(sla_fwrd_t ptr[], unsigned cur_level)
{
	for (unsigned i=cur_level - 1; i<cur_level; i--) // backwards
		printf("  p[%u].node=%p p[%u].cnt=%3u\n", i, ptr[i].node, i, ptr[i].cnt);
}

void
sla_print_chars(sla_t *sla)
{
	sla_node_t *node;
	sla_for_each(sla, 0, node) {
		for (unsigned i=0; i<SLA_NODE_NITEMS(node); i++) {
			printf("%c", *((char *)node->chunk + i));
		}
	}
	printf("\n");
}

#ifdef SLA_TEST

int main(int argc, const char *argv[])
{
	sla_t sla;
	sla_init(&sla, 10, .5);
	printf("sla initialized\n");

	char buff[64];
	for (unsigned i=0; i<sizeof(buff); i++)
		buff[i] = '0' + (i % 10);

	sla_copyto(&sla, buff, sizeof(buff), 16);
	printf("[0] "); sla_print_chars(&sla);

	//sla_print(&sla);
	for (unsigned i=1; i<sizeof(buff); i++) {
		sla_t sla2;
		printf("split at %u\n", i);
		sla_split_coarse(&sla, &sla2, i);
		sla_verify(&sla);
		sla_verify(&sla2);
		printf("[1] "); sla_print_chars(&sla);
		printf("[2] "); sla_print_chars(&sla2);
		sla_concat(&sla, &sla2);
		printf("[0] "); sla_print_chars(&sla);
		sla_verify(&sla);
	}

	return 0;
}

#endif /* SLA_TEST */
