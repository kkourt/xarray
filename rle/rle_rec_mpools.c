#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#ifdef NO_CILK
#define cilk_spawn
#define cilk_sync
#define Self 0
#else
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#define YES_CILK
#endif

static unsigned long
min_ul(unsigned long x, unsigned long y)
{
	return (x < y) ? x: y;
}

struct rle_node {
	char   symbol;
	unsigned long freq;
	struct rle_node *next;
};

struct rle_head {
	struct rle_node *first, *last;
};

static unsigned rle_rec_limit = 64;
static int nrthreads__ = 1;


struct rle_mempool {
	struct rle_node *rle_nodes;
	struct rle_head *rle_heads;
	unsigned long rle_nodes_nr, rle_heads_nr;
	unsigned long rle_nodes_al, rle_heads_al;
	char padding[80];
};
static struct rle_mempool *__mempools;

static inline struct rle_mempool *
rle_mempool_get(void)
{
	struct rle_mempool *ret;
	ret = __mempools;
	#if defined(YES_CILK)
	ret += __cilkrts_get_worker_number();
	#endif
	return ret;
}

static void
rle_pool_free_nodes(struct rle_mempool *pool)
{
	unsigned long cnt;
	struct rle_node *temp, *node;

	cnt = 0;
	node = pool->rle_nodes;
	while (node){
		temp = node;
		node = node->next;
		free(temp);
		cnt++;
	}

	if (cnt != pool->rle_nodes_nr){
		printf("[pool:%lu] freed %lu nodes, while had %lu nodes\n", pool - __mempools, cnt, pool->rle_nodes_nr);
		assert(0);
	}

	pool->rle_nodes_nr -= cnt;
	pool->rle_nodes = NULL;
}

static void
rle_pool_free_heads(struct rle_mempool *pool)
{
	unsigned long cnt;
	struct rle_head *temp, *head;

	cnt = 0;
	head = pool->rle_heads;
	while (head){
		temp = head;
		head = (void *)head->first;
		free(temp);
		cnt++;
	}
	assert(cnt == pool->rle_heads_nr);
	pool->rle_heads = NULL;
	pool->rle_heads_nr -= cnt;
	pool->rle_heads_al -= cnt;
}

static void
rle_pools_free()
{
	unsigned i;
	struct rle_mempool *pool;

	for (i=0; i<nrthreads__;  i++){
		pool = __mempools + i;
		rle_pool_free_nodes(pool);
		rle_pool_free_heads(pool);
	}
}

void
do_rle_pool_prealloc_nodes(struct rle_mempool *pool, unsigned long rle_nodes)
{
	unsigned long i;

	if (pool->rle_nodes_nr >= rle_nodes)
		return;
	rle_nodes = rle_nodes - pool->rle_nodes_nr;

	for (i=0; i<rle_nodes; i++){
		struct rle_node *rle, *rle_prev;

		rle = malloc(sizeof(struct rle_node));
		if (rle == NULL){
			fprintf(stderr, "%s: malloc failed\n", __FUNCTION__);
			exit(1);
		}

		rle_prev = pool->rle_nodes;
		rle->next = rle_prev;
		pool->rle_nodes = rle;
	}
	pool->rle_nodes_nr += rle_nodes;
	pool->rle_nodes_al += rle_nodes;
}

void
rle_pool_prealloc_nodes(unsigned long rle_nodes)
{
	unsigned long i;

	rle_nodes = rle_nodes / nrthreads__;
	for (i=0; i<nrthreads__; i++){
		do_rle_pool_prealloc_nodes(__mempools + i, rle_nodes);
	}
}


void
rle_init_pools()
{
	unsigned long i;

	assert(sizeof(struct rle_mempool) == 128);
	__mempools = malloc(sizeof(struct rle_mempool)*nrthreads__);
	if (__mempools == NULL){
		fprintf(stderr, "%s: malloc failed\n", __FUNCTION__);
		exit(1);
	}

	for (i=0; i<nrthreads__; i++){
		__mempools[i].rle_nodes = NULL;
		__mempools[i].rle_heads = NULL;
		__mempools[i].rle_heads_nr = 0;
		__mempools[i].rle_nodes_nr = 0;
		__mempools[i].rle_heads_al = 0;
		__mempools[i].rle_nodes_al = 0;
	}

}

static void __attribute__((unused))
rle_print_pool_stats(void)
{
	struct rle_mempool *pool;
	unsigned i;
	unsigned long total_heads, total_nodes, avail_nodes, avail_heads;

	total_heads = total_nodes = avail_nodes = avail_heads = 0;
	for (i=0; i<nrthreads__; i++){
		pool = __mempools + i;
		printf("id: %d", i);
		printf(" avail: nodes:%8lu heads:%8lu\n", pool->rle_nodes_nr, pool->rle_heads_nr);
		avail_nodes += pool->rle_nodes_nr;
		avail_heads += pool->rle_heads_nr;
		total_nodes += pool->rle_nodes_al;
		total_heads += pool->rle_heads_al;
	}

	printf("total: nodes:%8lu heads:%8lu\n", total_nodes, total_heads);
	printf("avail: nodes:%8lu heads:%8lu\n", avail_nodes, avail_heads);
	printf("\n");

}


/*
 * do_loops: keep the processor busy, doing nothing
 */
void do_loops(unsigned cnt)
{
	volatile unsigned i;
	for (i=0; i<cnt; i++)
		;
}

struct rle_head *
rle_head_alloc(struct rle_mempool *pool)
{
	struct rle_head *ret;

	//assert(pool - __mempools <= nrthreads__);
	ret = pool->rle_heads;
	if (ret != NULL) {
		 pool->rle_heads = (void *)ret->first;
		 pool->rle_heads_nr--;
		 return ret;
	}

	ret = malloc(sizeof(struct rle_head));
	if (ret == NULL) {
		fprintf(stderr, "%s: malloc failed\n", __FUNCTION__);
		exit(1);
	}
	pool->rle_heads_al++;

	return ret;
}

static void
rle_head_dealloc(struct rle_mempool *pool, struct rle_head *rle_head)
{
	rle_head->first = (void *)pool->rle_heads;
	pool->rle_heads = rle_head;
	pool->rle_heads_nr++;
}

static struct rle_node *
rle_alloc(struct rle_mempool *pool)
{
	struct rle_node *rle;
	rle = pool->rle_nodes;
	if (rle != NULL) {
		 pool->rle_nodes = rle->next;
		 pool->rle_nodes_nr--;
		 return rle;
	}

	rle = malloc(sizeof(struct rle_node));
	if (rle == NULL){
		fprintf(stderr, "%s: malloc failed\n", __FUNCTION__);
		exit(1);
	}
	pool->rle_nodes_al++;

	return rle;
}

static void
rle_node_dealloc(struct rle_mempool *pool, struct rle_node *rle)
{
	rle->next = pool->rle_nodes;
	pool->rle_nodes = rle;
	pool->rle_nodes_nr++;
}

static struct rle_mempool *
rle_pool_min_nodes(void)
{
	struct rle_mempool *pool;
	unsigned i;

	pool = __mempools;
	for (i=1; i < nrthreads__; i++){
		if (__mempools[i].rle_nodes_nr < pool->rle_nodes_nr)
			pool = __mempools + i;
	}
	return pool;
}

static struct rle_mempool *
rle_pool_max_nodes(void)
{
	struct rle_mempool *pool;
	unsigned i;

	pool = __mempools;
	for (i=1; i < nrthreads__; i++){
		if (__mempools[i].rle_nodes_nr > pool->rle_nodes_nr)
			pool = __mempools + i;
	}
	return pool;
}

static void
rle_pool_balance_nodes(void)
{
	unsigned long total, average, i;
	unsigned cnt;

	if (nrthreads__ == 1)
		return;

	total = 0;
	for (i=0; i < nrthreads__; i++)
		total += __mempools[i].rle_nodes_nr;

	average = total / nrthreads__;
	for (cnt=0;;cnt++){
		struct rle_mempool *max, *min;
		unsigned long max_nodes, min_nodes, move_nodes;

		max = rle_pool_max_nodes();
		min = rle_pool_min_nodes();
		max_nodes = max->rle_nodes_nr;
		min_nodes = min->rle_nodes_nr;

		if (min_nodes == average)
			return;

		//printf("min_nodes:%lu average:%lu\n", min_nodes, average);
		assert(min_nodes < average);
		assert(max_nodes > average);

		move_nodes = min_ul(average - min_nodes, max_nodes - average);

		//printf("---> balancing (%d)\n", cnt);
		//printf("max_nodes:%lu min_nodes:%lu moving:%lu\n", max_nodes, min_nodes, move_nodes);
		for (i=0; i< move_nodes; i++){
			struct rle_node *tmp1, *tmp2;

			tmp1 = max->rle_nodes;
			tmp2 = min->rle_nodes;

			max->rle_nodes = tmp1->next;
			tmp1->next = tmp2;
			min->rle_nodes = tmp1;
		}
		max->rle_nodes_nr -= move_nodes;
		min->rle_nodes_nr += move_nodes;

		assert(cnt < nrthreads__*nrthreads__);

		//rle_print_pool_stats();
		//printf("<--- balancing\n\n\n");
	}
}

static void
rle_nodes_dealloc(struct rle_node *first)
{
	unsigned i;
	struct rle_node *rle, *rle_next;
	struct rle_mempool *pool;

	rle = first;
	i = 0;
	while (rle != NULL){
		rle_next = rle->next;

		pool = __mempools + (i++ % nrthreads__);

		rle->next = pool->rle_nodes;
		pool->rle_nodes = rle;
		pool->rle_nodes_nr++;

		rle = rle_next;
	}
}

void
rle_print(struct rle_head *rle_head)
{
	struct rle_node *rle = rle_head->first;
	while (rle != NULL){
		printf("(`%c': %4ld) ", rle->symbol, rle->freq);
		rle = rle->next;
	}
	printf("\n");
}

struct rle_head *
rle_mkrand(unsigned long rle_nr, unsigned long *syms_nr)
{
	unsigned long i, syms_cnt = 0;
	struct rle_head *ret;
	struct rle_node *rle=NULL, dummy_head, *prev;
	const int max_freq = 100;
	const int min_freq = 1;
	struct rle_mempool *pool = rle_mempool_get();

	dummy_head.next = NULL;
	prev = &dummy_head;
	for (i=0; i<rle_nr; i++) {
		rle = rle_alloc(pool);
		prev->next = rle;

		rle->symbol = 'a' + (i % 26);
		rle->freq = min_freq + (rand() % (max_freq - min_freq +1));
		rle->next = NULL;
		syms_cnt += rle->freq;

		prev = rle;
	}

	if (syms_nr != NULL)
		*syms_nr = syms_cnt;

	ret = rle_head_alloc(pool);
	ret->first = dummy_head.next;
	ret->last = rle;
	return ret;
}

char *
rle_decode(struct rle_head *rle_head, unsigned long syms_nr)
{
	char *ret;
	unsigned long i, syms_i;
	struct rle_node *rle;

	ret = malloc(syms_nr*sizeof(char));
	if (!ret){
		fprintf(stderr, "failed to allocate %lu bytes\n", syms_nr*sizeof(char));
		exit(1);
	}

	rle = rle_head->first;
	syms_i = 0;
	while (rle != NULL){
		for (i=0; i<rle->freq; i++)
			ret[syms_i++] = rle->symbol;
		rle = rle->next;
	}

	assert(syms_i == syms_nr);
	return ret;
}

static struct rle_node *
rle_alloc_init(struct rle_mempool *pool, char symbol, unsigned long freq, struct rle_node *next)
{
	struct rle_node *rle;

	rle = rle_alloc(pool);
	rle->symbol = symbol;
	rle->freq = freq;
	rle->next = next;

	return rle;
}

struct rle_head *
rle_encode(struct rle_mempool *pool, char *symbols, unsigned long syms_nr)
{
	unsigned long i, freq;
	char prev, curr;
	struct rle_head *ret;
	struct rle_node dummy_head, *node, *n=NULL;

	if (symbols == NULL || syms_nr == 0)
		return NULL;

	dummy_head.next = NULL;
	node = &dummy_head;

	prev = symbols[0];
	freq = 1;
	for (i=1; i<syms_nr; i++) {

		curr = symbols[i];
		if (curr == prev){
			freq++;
			continue;
		}

		n = rle_alloc_init(pool, prev, freq, NULL);
		node->next = n;
		node = n;
		prev = curr;
		freq = 1;
	}

	n = rle_alloc_init(pool, prev, freq, NULL);
	node->next = n;

	assert(dummy_head.next != NULL);

	ret = rle_head_alloc(pool);
	ret->first = dummy_head.next;
	ret->last = n;
	return ret;
}

static struct rle_head *
rle_merge(struct rle_mempool *pool, struct rle_head *rle1, struct rle_head *rle2)
{
	struct rle_node *rle1_last, *rle2_first;

	rle1_last = rle1->last;
	rle2_first = rle2->first;
	if (rle1_last->symbol == rle2_first->symbol){
		rle1_last->freq += rle2_first->freq;
		rle1_last->next = rle2_first->next;
		if (rle2->last != rle2_first)
			rle1->last = rle2->last;
		//free(rle2_first);
		rle_node_dealloc(pool, rle2_first);
	} else {
		rle1_last->next = rle2_first;
		rle1->last = rle2->last;
	}

	//free(rle2);
	//rle2->first = rle2->last = NULL;
	rle_head_dealloc(pool, rle2);
	return rle1;
}

struct rle_head *
rle_encode_rec(char *symbols, unsigned long syms_nr)
{
	unsigned long syms_nr1, syms_nr2;
	struct rle_head *rle1, *rle2;
	struct rle_head *ret;

	if (syms_nr == 0 || symbols == NULL)
		return NULL;

	/* unitary solution */
	#if 0
	if (syms_nr == 1){
		ret = malloc(sizeof(struct rle_head));
		ret->first = ret->last = rle_alloc_init(symbols[0], 1, NULL);
		return ret;
	}
	#endif
	if (syms_nr <= rle_rec_limit){
		return rle_encode(rle_mempool_get(), symbols, syms_nr);
	}

	/* binary splitting */
	syms_nr1 = syms_nr / 2;
	syms_nr2 = syms_nr - syms_nr1;
	rle1 = cilk_spawn rle_encode_rec(symbols, syms_nr1);
	rle2 = cilk_spawn rle_encode_rec(symbols + syms_nr1, syms_nr2);
	cilk_sync;

	/* combining solutions */
	//assert(rle1 != NULL && rle2 != NULL);
	ret = rle_merge(rle_mempool_get(), rle1, rle2);
	return ret;
}

int
rle_cmp(struct rle_head *rle1, struct rle_head *rle2)
{
	struct rle_node *r1, *r2;

	r1 = rle1->first;
	r2 = rle2->first;

	while (1) {
		if (r1 == rle1->last && r2 == rle2->last)
			return 1;

		if (r1 == rle1->last || r2 == rle2->last)
			return 0;

		if (r1->symbol != r2->symbol)
			return 0;

		if (r1->freq != r2->freq)
			return 0;

		r1 = r1->next;
		r2 = r2->next;
	}

	assert(0 && "Stupid compiler");
	return 0;
}

#include "tsc.h"

int
main(int argc, const char *argv[])
{
	struct rle_head *rle, *rle_new=NULL, *rle_rec;
	unsigned long syms_nr, rles_nr;
	char *symbols, *rle_rec_limit_str;
	tsc_t t;
	/*
	#ifdef YES_CILK
	Cilk_time tm_begin, tm_elapsed;
	Cilk_time wk_begin, wk_elapsed;
	Cilk_time cp_begin, cp_elapsed;
	#endif
	*/
	#ifdef YES_CILK
	nrthreads__ = __cilkrts_get_nworkers();
	#endif

	if ( (rle_rec_limit_str = getenv("RLE_REC_LIMIT")) != NULL){
		rle_rec_limit = atol(rle_rec_limit_str);
		if (rle_rec_limit == 0)
			rle_rec_limit = 64;
	}

	rles_nr = 0;
	if (argc > 1)
		rles_nr = atol(argv[1]);
	if (rles_nr == 0)
		rles_nr = 100000;

	srand(time(NULL));
	rle_init_pools();

	rle = cilk_spawn rle_mkrand(rles_nr, &syms_nr);
	cilk_sync;
	//rle_print(rle);


	printf("number of rles:%lu\n", rles_nr);
	printf("number of symbols:%lu\n", syms_nr);
	printf("rle_rec_limit:%u\n", rle_rec_limit);
	#ifdef YES_CILK
	printf("Number of processors:%u\n", __cilkrts_get_nworkers());
	#endif
	symbols = rle_decode(rle, syms_nr);
	cilk_sync;

	do_rle_pool_prealloc_nodes(__mempools + 0, syms_nr);
	//rle_print_pool_stats();

	/* rle encode */
	tsc_init(&t); tsc_start(&t);
	rle_new = rle_encode(__mempools + 0, symbols, syms_nr);
	tsc_pause(&t); tsc_report("rle_encode", &t);
	assert(rle_cmp(rle, rle_new) == 1);

	rle_nodes_dealloc(rle_new->first);
	rle_head_dealloc(__mempools + 0, rle_new);
	//rle_print_pool_stats();
	rle_pool_balance_nodes();

	/* rle recursive encode */
	tsc_init(&t); tsc_start(&t);
	rle_rec = cilk_spawn rle_encode_rec(symbols, syms_nr);
	cilk_sync;
	tsc_pause(&t); tsc_report("rle_encode_rec", &t);

	if (rle_cmp(rle,rle_rec) != 1){
		rle_print(rle);
		rle_print(rle_rec);
		assert(0);
	}

	rle_nodes_dealloc(rle_rec->first);
	rle_nodes_dealloc(rle->first);
	rle_head_dealloc(__mempools + 0, rle_rec);
	rle_head_dealloc(__mempools + 0, rle);
	//rle_print_pool_stats();
	rle_pools_free();
	free(symbols);
	free(__mempools);

	return 0;
}
