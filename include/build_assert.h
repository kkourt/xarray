/* CC0 (Public domain) - see LICENSE file for details */
#ifndef CCAN_BUILD_ASSERT_H
#define CCAN_BUILD_ASSERT_H

/**
 * BUILD_ASSERT - assert a build-time dependency.
 * @cond: the compile-time condition which must be true.
 *
 * Your compile will fail if the condition isn't true, or can't be evaluated
 * by the compiler.  This can only be used within a function.
 *
 * Example:
 *	#include <stddef.h>
 *	...
 *	static char *foo_to_char(struct foo *foo)
 *	{
 *		// This code needs string to be at start of foo.
 *		BUILD_ASSERT(offsetof(struct foo, string) == 0);
 *		return (char *)foo;
 *	}
 */
#define BUILD_ASSERT(cond) \
	do { (void) sizeof(char [1 - 2*!(cond)]); } while(0)

/**
 * BUILD_ASSERT_OR_ZERO - assert a build-time dependency, as an expression.
 * @cond: the compile-time condition which must be true.
 *
 * Your compile will fail if the condition isn't true, or can't be evaluated
 * by the compiler.  This can be used in an expression: its value is "0".
 *
 * Example:
 *	#define foo_to_char(foo)					\
 *		 ((char *)(foo)						\
 *		  + BUILD_ASSERT_OR_ZERO(offsetof(struct foo, string) == 0))
 */
#define BUILD_ASSERT_OR_ZERO(cond) \
	(sizeof(char [1 - 2*!(cond)]) - 1)


#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
  /* This can't be used twice on the same line so ensure if using in headers
   * that the headers are not included twice (by wrapping in #ifndef...#endif)
   * Note it doesn't cause an issue when used on same line of separate modules
   * compiled with gcc -combine -fwhole-program.  */
# define STATIC_ASSERT(e) \
    enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(!!(e)) }

#endif /* CCAN_BUILD_ASSERT_H */
