#ifndef __TSC__
#define __TSC__

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <inttypes.h>

struct tsc {
	uint64_t	ticks;
	uint64_t	last;
	uint64_t        cnt;
};
typedef struct tsc tsc_t;

#if defined(__i386__) || defined(__x86_64__)
static inline uint64_t get_ticks(void)
{
	uint32_t hi,low;
	uint64_t ret;

	__asm__ __volatile__ ("rdtsc" : "=a"(low), "=d"(hi));

	ret = hi;
	ret <<= 32;
	ret |= low;

	return ret;
}
#elif defined(__ia64__)
#include <asm/intrinsics.h>
static inline uint64_t get_ticks(void)
{
	uint64_t ret = ia64_getreg(_IA64_REG_AR_ITC);
	ia64_barrier();

	return ret;
}
#elif defined(__sparc__)
// linux-2.6.28/arch/sparc64/kernel/time.c
static inline uint64_t get_ticks(void)
{
	uint64_t t;
	__asm__ __volatile__ (
		"rd     %%tick, %0\n\t"
		"mov    %0,     %0"
		: "=r"(t)
	);
	return t & (~(1UL << 63));
}
#else
#error "dont know how to count ticks"
#endif

static inline void tsc_init(tsc_t *tsc)
{
	tsc->ticks = 0;
	tsc->last  = 0;
	tsc->cnt   = 0;
}

static inline void tsc_shut(tsc_t *tsc)
{
}

static inline void tsc_start(tsc_t *tsc)
{
	tsc->last = get_ticks();
	assert(tsc->cnt % 2 == 0);
	tsc->cnt++;
}

static inline void tsc_pause(tsc_t *tsc)
{
	uint64_t t = get_ticks();
	assert(tsc->last < t);
	assert(tsc->cnt % 2 == 1);
	tsc->ticks += (t - tsc->last);
	tsc->cnt++;
}

static double __getMhz(void)
{
	double mhz = 0;
#ifdef CPU_MHZ_SH
	FILE *script;
	char buff[512], *endptr;
	int ret;

	script = popen(CPU_MHZ_SH, "r");
	assert(script != NULL);
	ret = fread(buff, 1, sizeof(buff), script);
	if (!ret){
		perror("fread");
		exit(1);
	}

	mhz = strtod(buff, &endptr);
	if (endptr == buff){
		perror("strtod");
		exit(1);
	}
#endif
	return mhz;
}

static inline double getMhz(void)
{
	static double mhz = 0.0;
	if (mhz == 0.0){
		mhz = __getMhz();
	}
	return mhz;
}

static inline double __tsc_getsecs(uint64_t ticks)
{
	return (ticks/(1000000.0*getMhz()));
}
static inline double tsc_getsecs(tsc_t *tsc)
{
	return __tsc_getsecs(tsc->ticks);
}

static uint64_t tsc_getticks(tsc_t *tsc)
{
	return tsc->ticks;
}

static inline void tsc_spinticks(uint64_t ticks)
{
	uint64_t t0;
	//uint64_t spins = 0;
	t0 = get_ticks();
	for (;;){
		if (get_ticks() - t0 >= ticks)
			break;
		//spins++;
	}
	//printf("spins=%lu\n", spins);
}

static inline void tsc_report(tsc_t *tsc)
{
	uint64_t ticks = tsc_getticks(tsc);

	printf("ticks : %llu\n", (long long unsigned)ticks);
	printf("time  : %lf (sec)\n", __tsc_getsecs(ticks));
}

#define TSC_MEASURE_TICKS(_ticks, _code)       \
uint64_t _ticks = ({                           \
        tsc_t xtsc_;                           \
        tsc_init(&xtsc_); tsc_start(&xtsc_);   \
        do { _code } while (0);                \
        tsc_pause(&xtsc_);                     \
        tsc_getticks(&xtsc_);                  \
});

#define TSC_SET_TICKS(_ticks, _code)           \
_ticks = ({                                   \
        tsc_t xtsc_;                           \
        tsc_init(&xtsc_); tsc_start(&xtsc_);   \
        do { _code } while (0);                \
        tsc_pause(&xtsc_);                     \
        tsc_getticks(&xtsc_);                  \
});

#define TSC_ADD_TICKS(_ticks, _code)           \
_ticks += ({                                   \
        tsc_t xtsc_;                           \
        tsc_init(&xtsc_); tsc_start(&xtsc_);   \
        do { _code } while (0);                \
        tsc_pause(&xtsc_);                     \
        tsc_getticks(&xtsc_);                  \
});

#endif
