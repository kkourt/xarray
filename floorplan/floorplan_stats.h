#ifndef FLORPLAN_STATS_H
#define FLORPLAN_STATS_H

#include <strings.h> //bzero

#include "misc.h"
#include "tsc.h"

// ugh
#if !defined(NO_FLOORPLAN_STATS)
#define FLOORPLAN_STATS // enable stats
#endif

#define DECLARE_FLOORPLAN_STATS \
	floorplan_stats_t *FloorPStats __attribute__((unused));

struct floorplan_stats {
	tsc_t memcpy_brd;
	tsc_t memcpy_cells;
	tsc_t xvarray_branch;
	tsc_t lay_down;
	xcnt_t lay_down_ok;
	xcnt_t lay_down_fail;
} __attribute__((aligned(64)));
typedef struct floorplan_stats floorplan_stats_t;

extern floorplan_stats_t *FloorPStats;

static inline void
floorplan_stats_init(unsigned nthreads)
{
	size_t s = sizeof(floorplan_stats_t)*nthreads;
	bzero(FloorPStats, s);
}

static inline void
floorplan_stats_create(unsigned nthreads)
{
	size_t s = sizeof(floorplan_stats_t)*nthreads;
	FloorPStats = xmalloc(s);
	floorplan_stats_init(nthreads);
}


static inline void
floorplan_stats_destroy(void)
{
	free(FloorPStats);
	FloorPStats = NULL;
}

static inline void
floorplan_stats_do_report(const char *prefix, floorplan_stats_t *st, uint64_t total_ticks)
{
#if defined(FLOORPLAN_STATS)
	#define pr_ticks(x) tsc_report_perc("" #x, &st->x, total_ticks, 0)
	#define pr_xcnt(x__) do { \
		if (st->x__.cnt > 0) \
			xcnt_report("" #x__, &st->x__); \
		} while (0)

	pr_ticks(memcpy_brd);
	pr_ticks(memcpy_cells);
	pr_ticks(xvarray_branch);
	pr_ticks(lay_down);
	pr_xcnt(lay_down_ok);
	pr_xcnt(lay_down_fail);

	#undef  pr_ticks
	#undef  pr_cnt
#endif
}

static inline void
floorplan_stats_report(unsigned nthreads, uint64_t total_ticks)
{
#if defined(FLOORPLAN_STATS)
	for (unsigned i=0; i<nthreads; i++) {
		printf("thread %3u:\n", i);
		floorplan_stats_do_report(" ", FloorPStats + i, total_ticks);
	}
#endif
}

#if defined(FLOORPLAN_STATS)
	#define FLOORPLAN_TIMER_START(x,i)  do {tsc_start(&FloorPStats[i].x); } while (0)
	#define FLOORPLAN_TIMER_PAUSE(x,i)  do {tsc_pause(&FloorPStats[i].x);  } while (0)
	#define FLOORPLAN_INC_COUNTER(x,i)   \
		do { xcnt_inc(&(FloorPStats[i].x));     } while (0)
	#define FLOORPLAN_ADD_COUNTER(x,i,v) \
		do { xcnt_add(&((FloorPStats[i].x), v));  } while (0)
#else // !defined(FLOORPLAN_STATS)
	#define FLOORPLAN_TIMER_START(x,i)   do {;} while (0)
	#define FLOORPLAN_TIMER_PAUSE(x,i)   do {;} while (0)
	#define FLOORPLAN_INC_COUNTER(x,i)   do {;} while (0)
	#define FLOORPLAN_ADD_COUNTER(x,i,v) do {;} while (0)
#endif // FLOORPLAN_STATS


#endif /* FLORPLAN_STATS_H */
