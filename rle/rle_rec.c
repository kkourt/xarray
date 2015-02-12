#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#ifdef NO_CILK
#define cilk_spawn
#define cilk_sync
#else
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#define YES_CILK
#endif

#include "tsc.h"
#include "misc.h"

#include "rle_rec_stats.h"
DECLARE_RLE_STATS

unsigned
rle_getmyid(void)
{
	unsigned ret = 0;
	#if defined(YES_CILK)
	ret = __cilkrts_get_worker_number();
	#endif
	return ret;
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
rle_head_alloc(void)
{
	struct rle_head *ret;

	ret = malloc(sizeof(struct rle_head));
	if (ret == NULL) {
		fprintf(stderr, "%s: malloc failed\n", __FUNCTION__);
		exit(1);
	}

	return ret;
}

struct rle_node *
rle_alloc(void)
{
	struct rle_node *rle;

	rle = malloc(sizeof(struct rle_node));
	if (rle == NULL){
		fprintf(stderr, "%s: malloc failed\n", __FUNCTION__);
		exit(1);
	}

	return rle;
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

	dummy_head.next = NULL;
	prev = &dummy_head;
	for (i=0; i<rle_nr; i++) {
		rle = rle_alloc();
		prev->next = rle;

		rle->symbol = 'a' + (i % 26);
		rle->freq = min_freq + (rand() % (max_freq - min_freq +1));
		rle->next = NULL;
		syms_cnt += rle->freq;

		prev = rle;
	}

	if (syms_nr != NULL)
		*syms_nr = syms_cnt;

	ret = rle_head_alloc();
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

struct rle_node *
rle_alloc_init(char symbol, unsigned long freq, struct rle_node *next)
{
	struct rle_node *rle;

	rle = rle_alloc();
	rle->symbol = symbol;
	rle->freq = freq;
	rle->next = next;

	return rle;
}

struct rle_head *
rle_encode(char *symbols, unsigned long syms_nr)
{
	unsigned long i, freq;
	char prev, curr;
	struct rle_head *ret;
	struct rle_node dummy_head, *node, *n;

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

		RLE_TIMER_START(rle_alloc, rle_getmyid());
		n = rle_alloc_init(prev, freq, NULL);
		RLE_TIMER_PAUSE(rle_alloc, rle_getmyid());

		node->next = n;
		node = n;
		prev = curr;
		freq = 1;
	}

	n = rle_alloc_init(prev, freq, NULL);
	node->next = n;

	assert(dummy_head.next != NULL);

	ret = rle_head_alloc();
	ret->first = dummy_head.next;
	ret->last = n;
	return ret;
}

struct rle_head *
rle_merge(struct rle_head *rle1, struct rle_head *rle2)
{
	struct rle_node *rle1_last, *rle2_first;

	rle1_last = rle1->last;
	rle2_first = rle2->first;
	if (rle1_last->symbol == rle2_first->symbol){
		rle1_last->freq += rle2_first->freq;
		rle1_last->next = rle2_first->next;
		if (rle2->last != rle2_first)
			rle1->last = rle2->last;
		free(rle2_first);
	} else {
		rle1_last->next = rle2_first;
		rle1->last = rle2->last;
	}

	free(rle2);
	return rle1;
}

struct rle_head *
rle_encode_rec(char *symbols, size_t syms_nr)
{
	struct rle_head *rle1, *rle2;

	// NB: If these are moved above the spawns, gcc complains about
	// potentially uninitialized variables
	size_t syms_nr1 = syms_nr / 2;
	size_t syms_nr2 = syms_nr - syms_nr1;

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

	if (syms_nr <= rle_rec_limit)
		return rle_encode(symbols, syms_nr);

	/* binary splitting */
	rle1 = cilk_spawn rle_encode_rec(symbols, syms_nr1);
	rle2 = cilk_spawn rle_encode_rec(symbols + syms_nr1, syms_nr2);
	cilk_sync;

	/* combining solutions */
	assert(rle1 != NULL && rle2 != NULL);
	return rle_merge(rle1, rle2);
}

int
rle_cmp(struct rle_head *rle1, struct rle_head *rle2)
{
	struct rle_node *r1, *r2;

	r1 = rle1->first;
	r2 = rle2->first;

	while (1) {
		if (r1 == NULL && r2 == NULL)
			return 1;

		if (r1 == NULL || r2 == NULL)
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

static void
set_params(void)
{
	long x;
	char *x_str;

	#define set_paraml(env_str, param) \
	do { \
		if ( (x_str = getenv(env_str)) != NULL) { \
			x = atol(x_str); \
			if (x != 0) \
				param = x; \
		} \
	} while (0)

	set_paraml("RLE_REC_LIMIT",   rle_rec_limit);

	#undef set_paraml
}

int
main(int argc, const char *argv[])
{
	struct rle_head *rle, *rle_new, *rle_rec;
	unsigned long syms_nr, rles_nr;
	char *symbols;
	tsc_t t;

	/*
	#ifdef YES_CILK
	Cilk_time tm_begin, tm_elapsed;
	Cilk_time wk_begin, wk_elapsed;
	Cilk_time cp_begin, cp_elapsed;
	#endif
	*/

	set_params();

	unsigned nthreads;
	#ifdef NO_CILK
	nthreads = 1;
	#else
	nthreads = __cilkrts_get_nworkers();
	#endif
	rle_stats_create(nthreads);

	rles_nr = 0;
	if (argc > 1)
		rles_nr = atol(argv[1]);
	if (rles_nr == 0)
		rles_nr = 100000;

	srand(time(NULL));
	rle = rle_mkrand(rles_nr, &syms_nr);
	//rle_print(rle);

	printf("xarray impl: %s\n",        "LISTARR");
	printf("number of rles:%lu\n", rles_nr);
	printf("number of symbols:%lu\n", syms_nr);
	printf("rle_rec_limit:%u\n", rle_rec_limit);
	#ifndef NO_CILK
	printf("Number of processors:%u\n", nthreads);
	#endif
	symbols = rle_decode(rle, syms_nr);

	tsc_init(&t); tsc_start(&t);
	rle_new = rle_encode(symbols, syms_nr);
	//rle_print(rle_new);
	tsc_pause(&t);
	tsc_report("rle_encode", &t);
	if (rle_cmp(rle, rle_new) != 1) {
		fprintf(stderr, "RLEs do not match\n");
		exit(1);
	}
	cilk_sync;

	/*
	#ifdef YES_CILK
	cp_begin = Cilk_user_critical_path;
	wk_begin = Cilk_user_work;
	tm_begin = Cilk_get_wall_time();
	#endif
	*/

	rle_stats_init(nthreads);
	tsc_init(&t); tsc_start(&t);
	rle_rec = cilk_spawn rle_encode_rec(symbols, syms_nr);
	cilk_sync;
	tsc_pause(&t);
	tsc_report("rle_encode_rec", &t);
	rle_stats_report(nthreads, tsc_getticks(&t));
	rle_stats_destroy();

	/*
	#ifdef YES_CILK
	tm_elapsed = Cilk_get_wall_time() - tm_begin;
	wk_elapsed = Cilk_user_work - wk_begin;
	cp_elapsed = Cilk_user_critical_path - cp_begin;
	#endif
	*/

	//rle_print(rle_rec);
	if (rle_cmp(rle, rle_rec) != 1) {
		fprintf(stderr, "RLEs do not match\n");
		exit(1);
	}

	/*
	#ifdef YES_CILK
	printf("Running time = %4f s\n", Cilk_wall_time_to_sec(tm_elapsed));
	printf("Work          = %4f s\n", Cilk_time_to_sec(wk_elapsed));
	printf("Span          = %4f s\n\n", Cilk_time_to_sec(cp_elapsed));
	#endif
	*/


	return 0;
}
