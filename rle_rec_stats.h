#ifndef RLE_REC_STATS_H
#define RLE_REC_STATS_H

#include <strings.h> //bzero

#include "misc.h"
#include "tsc.h"

#define RLE_STATS // enable stats



#define DECLARE_RLE_STATS rle_stats_t *RleStats;

struct rle_stats {
	tsc_t rle_merge;
	tsc_t rle_split;
	tsc_t rle_encode;
	tsc_t rle_encode_loop;
	tsc_t rle_alloc;
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
rle_stats_do_report(const char *prefix, rle_stats_t *st, uint64_t total_ticks)
{
	#define pr_ticks(x__) do { \
		uint64_t t__ = tsc_getticks(&st->x__);  \
		uint64_t c__ = st->x__.cnt;             \
		double p__ = t__ / (double)total_ticks; \
		if (p__ < -0.01) \
			break; \
		printf("%s" "%24s" ": %6.1lfM (%4.1lf%%) cnt:%9lu (avg:%7.2lfK)\n", \
		        prefix, "" #x__, t__/(1000*1000.0), p__*100, c__, t__/(1000.0*c__)); \
	} while (0)

	pr_ticks(rle_merge);
	pr_ticks(rle_split);
	pr_ticks(rle_encode);
	pr_ticks(rle_alloc);
	pr_ticks(rle_encode_loop);

	#undef pr_ticks
}

static inline void
rle_stats_report(unsigned nthreads, uint64_t total_ticks)
{
	for (unsigned i=0; i<nthreads; i++) {
		printf("thread %3u:\n", i);
		rle_stats_do_report(" ", RleStats + i, total_ticks);
	}
}

#if defined(RLE_STATS)
	#define RLE_TIMER_START(x,i)  do {tsc_start(&RleStats[i].x); } while (0)
	#define RLE_TIMER_PAUSE(x,i)  do {tsc_pause(&RleStats[i].x);  } while (0)
#else // !defined(RLE_STATS)
	#define DECLARE_RLE_STATS()
	#define RLE_TIMER_START(x,i)  do {;} while (0)
	#define RLE_TIMER_PAUSE(x,i)  do {;} while (0)
#endif // RLE_STATS

#endif
