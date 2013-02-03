#ifndef SUM_OP_H_
#define SUM_OP_H_

#include <math.h>

static inline int
sum_op(int x)
{
	#if defined(SUM_SQRT)
	return (int)floor(sqrt((double)x));
	#elif defined(SUM_NOOP)
	return x;
	#else
	return x;
	#endif
}


#endif /* SUM_OP_H_ */
