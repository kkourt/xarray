#ifndef RLE_REC_STATS_H
#define RLE_REC_STATS_H

#include <strings.h> //bzero

#include "misc.h"
#include "tsc.h"

// ugh
#if !defined(NO_RLE_STATS)
#define RLE_STATS // enable stats
#endif

#define DECLARE_RLE_STATS rle_stats_t *RleStats __attribute__((unused));

struct rle_stats {
	tsc_t rle_merge;
	tsc_t rle_split;
	tsc_t rle_encode;
	tsc_t rle_encode_loop;
	tsc_t append_rle;
	tsc_t rle_alloc;
	tsc_t sla_append_prepare;
	tsc_t sla_node_alloc;
	tsc_t sla_chunk_alloc;
} __attribute__((aligned(64)));
typedef struct rle_stats rle_stats_t;

extern rle_stats_t *RleStats;

static inline void
rle_stats_init(unsigned nthreads)
{
	size_t s = sizeof(rle_stats_t)*nthreads;
	bzero(RleStats, s);
}

static inline void
rle_stats_create(unsigned nthreads)
{
	size_t s = sizeof(rle_stats_t)*nthreads;
	RleStats = xmalloc(s);
	rle_stats_init(nthreads);
}


static inline void
rle_stats_destroy(void)
{
	free(RleStats);
	RleStats = NULL;
}

static inline void
rle_stats_do_report(rle_stats_t *st, uint64_t total_ticks)
{
#if defined(RLE_STATS)
	#define pr_ticks(x) do {         \
		tsc_report_perc("" #x, &st->x, total_ticks, 0); \
	} while (0)

	pr_ticks(rle_merge);
	pr_ticks(rle_split);
	pr_ticks(rle_encode);
	pr_ticks(rle_alloc);
	pr_ticks(rle_encode_loop);
	pr_ticks(append_rle);
	pr_ticks(sla_append_prepare);
	pr_ticks(sla_node_alloc);
	pr_ticks(sla_chunk_alloc);

	#undef pr_ticks
#endif
}

static inline void
rle_stats_report(unsigned nthreads, uint64_t total_ticks)
{
#if defined(RLE_STATS)
	printf("RLE_STATS START\n");
	for (unsigned i=0; i<nthreads; i++) {
		printf("thread %3u:\n", i);
		rle_stats_do_report(RleStats + i, total_ticks);
	}
	printf("RLE_STATS END\n");
#endif
}

#if defined(RLE_STATS)
	#define RLE_TIMER_START(x,i)  do {tsc_start(&RleStats[i].x); } while (0)
	#define RLE_TIMER_PAUSE(x,i)  do {tsc_pause(&RleStats[i].x);  } while (0)
#else // !defined(RLE_STATS)
	#define RLE_TIMER_START(x,i)  do {;} while (0)
	#define RLE_TIMER_PAUSE(x,i)  do {;} while (0)
#endif // RLE_STATS

#endif
