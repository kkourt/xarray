#ifndef FLORPLAN_STATS_H
#define FLORPLAN_STATS_H

#include <strings.h> //bzero

#include "misc.h"
#include "tsc.h"
#include "xcnt.h"

// ugh
#if !defined(NO_FLOORPLAN_STATS)
#define FLOORPLAN_STATS // enable stats
#endif

struct floorplan_stats {
	tsc_t xvarray_branch;
	tsc_t lay_down;
	tsc_t lay_down_write;
	tsc_t lay_down_read;
	tsc_t verpmap_get;
	tsc_t verpmap_set;
	tsc_t verpmap_remove;
	tsc_t verpmap_update;
	tsc_t verpmap_reset;
	tsc_t tmp_tsc;
	xcnt_t lay_down_ok;
	xcnt_t lay_down_fail;
	xcnt_t branch;
	xcnt_t commit;
	xcnt_t chunks;
} __attribute__((aligned(64)));
typedef struct floorplan_stats floorplan_stats_t;

#define DECLARE_FLOORPLAN_STATS                                              \
  floorplan_stats_t *FloorPStats __attribute__((unused));                    \
  floorplan_stats_t __thread *myFLoorPstats  __attribute__((unused)) = NULL; \

extern floorplan_stats_t *FloorPStats;
extern __thread floorplan_stats_t *myFLoorPstats;

static inline void
myfloorplan_stats_set(int myid)
{
#if defined(FLOORPLAN_STATS)
	if (myFLoorPstats == NULL) {
		myFLoorPstats = FloorPStats + myid;
	} else {
		//assert(myFLoorPstats == FloorPStats + myid);
	}
#endif
}



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

	pr_ticks(xvarray_branch);
	pr_ticks(lay_down);
	pr_ticks(lay_down_write);
	pr_ticks(lay_down_read);
	pr_ticks(tmp_tsc);
	pr_ticks(verpmap_get);
	pr_ticks(verpmap_set);
	pr_ticks(verpmap_remove);
	pr_ticks(verpmap_update);
	pr_ticks(verpmap_reset);
	pr_xcnt(lay_down_ok);
	pr_xcnt(lay_down_fail);
	pr_xcnt(branch);
	pr_xcnt(commit);
	pr_xcnt(chunks);

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
	#define FLOORPLAN_TIMER_START(x)  do {tsc_start(&myFLoorPstats->x); } while (0)
	#define FLOORPLAN_TIMER_PAUSE(x)  do {tsc_pause(&myFLoorPstats->x);  } while (0)
	#define FLOORPLAN_XCNT_INC(x)   \
		do { xcnt_inc(&myFLoorPstats->x);     } while (0)
	#define FLOORPLAN_XCNT_ADD(x,v) \
		do { xcnt_add(&myFLoorPstats->x, v);  } while (0)
#else // !defined(FLOORPLAN_STATS)
	#define FLOORPLAN_TIMER_START(x)   do {;} while (0)
	#define FLOORPLAN_TIMER_PAUSE(x)   do {;} while (0)
	#define FLOORPLAN_XCNT_INC(x)   do {;} while (0)
	#define FLOORPLAN_XCNT_ADD(x,v) do {;} while (0)
#endif // FLOORPLAN_STATS


#endif /* FLORPLAN_STATS_H */
