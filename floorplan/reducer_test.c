#include <stdio.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
//#include <cilk/reducer_min.h>
#include <cilk/reducer_opadd.h>

#include "misc.h"

#define LOOPS 32

spinlock_t xlock;

int foo(int level)
{
	int cnt = 0;
	CILK_C_REDUCER_OPADD(cnt__, int, 0);

	int j;
	cilk_for (j=0; j<LOOPS; j++) {
		int addme = j;
		if (level > 1) {
			addme += foo(level -1);
		}

		spin_lock(&xlock);
		cnt += addme;
		spin_unlock(&xlock);
		REDUCER_VIEW(cnt__) += addme;
	}

	return REDUCER_VIEW(cnt__);
	//return cnt;
}

int main(int argc, const char *argv[])
{

	spinlock_init(&xlock);
	int ret = foo(2);

	printf("ret  =%d\n", ret);
	//printf("cnt__=%d\n", REDUCER_VIEW(cnt__));
	return 0;
}
