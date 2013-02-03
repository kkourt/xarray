#ifndef SUM_STATS_H
#define SUM_STATS_H

#include <strings.h> //bzero

#include "misc.h"
#include "tsc.h"
#include "xcnt.h"

// ugh
#if !defined(NO_SUM_STATS)
#define SUM_STATS // enable stats
#endif

struct sum_stats {
	tsc_t sum_seq;
	tsc_t sum_split;
} __attribute__((aligned(64)));
typedef struct sum_stats sum_stats_t;

#define DECLARE_SUM_STATS                                              \
  sum_stats_t *SumStats __attribute__((unused));                    \
  sum_stats_t __thread *mySumStats  __attribute__((unused)) = NULL; \

extern sum_stats_t *SumStats;
extern __thread sum_stats_t *mySumStats;

static inline void
mysum_stats_set(int myid)
{
#if defined(SUM_STATS)
	if (mySumStats == NULL) {
		mySumStats = SumStats + myid;
	} else {
		assert(mySumStats == SumStats + myid);
	}
#endif
}



static inline void
sum_stats_init(unsigned nthreads)
{
	size_t s = sizeof(sum_stats_t)*nthreads;
	bzero(SumStats, s);
}

static inline void
sum_stats_create(unsigned nthreads)
{
	size_t s = sizeof(sum_stats_t)*nthreads;
	SumStats = xmalloc(s);
	sum_stats_init(nthreads);
}


static inline void
sum_stats_destroy(void)
{
	free(SumStats);
	SumStats = NULL;
}

static inline void
sum_stats_do_report(const char *prefix, sum_stats_t *st, uint64_t total_ticks)
{
#if defined(SUM_STATS)
	#define pr_ticks(x) tsc_report_perc("" #x, &st->x, total_ticks, 0)
	#define pr_xcnt(x__) do { \
		if (st->x__.cnt > 0) \
			xcnt_report("" #x__, &st->x__); \
		} while (0)

	pr_ticks(sum_seq);
	pr_ticks(sum_split);

	#undef  pr_ticks
	#undef  pr_cnt
#endif
}

static inline void
sum_stats_report(unsigned nthreads, uint64_t total_ticks)
{
#if defined(SUM_STATS)
	for (unsigned i=0; i<nthreads; i++) {
		printf("thread %3u:\n", i);
		sum_stats_do_report(" ", SumStats + i, total_ticks);
	}
#endif
}

#if defined(SUM_STATS)
	#define SUM_TIMER_START(x)  do {tsc_start(&mySumStats->x); } while (0)
	#define SUM_TIMER_PAUSE(x)  do {tsc_pause(&mySumStats->x);  } while (0)
	#define SUM_XCNT_INC(x)   \
		do { xcnt_inc(&mySumStats->x);     } while (0)
	#define SUM_XCNT_ADD(x,v) \
		do { xcnt_add(&mySumStats->x, v);  } while (0)
#else // !defined(SUM_STATS)
	#define SUM_TIMER_START(x)   do {;} while (0)
	#define SUM_TIMER_PAUSE(x)   do {;} while (0)
	#define SUM_XCNT_INC(x)   do {;} while (0)
	#define SUM_XCNT_ADD(x,v) do {;} while (0)
#endif // SUM_STATS


#endif /* SUM_STATS_H */
