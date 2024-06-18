#ifndef INCLUDE_CHECK_H
#define INCLUDE_CHECK_H

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* Utility macros */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Output functions/macros */

enum MsgT {
	msgt_info = 0,
	msgt_warn,
	msgt_err,
	msgt_end,
};
static const char *_msgtstr[msgt_end] =
	{"INFO", "WARN", "ERROR"}; // not a macro so we don't forget to update it

#define test_output(type, msg, ...)                          \
	do {                                                     \
		printf("%s " msg "\n", _msgtstr[type], __VA_ARGS__); \
		fflush(stdout);                                      \
	} while (0)

/* All the asserts */

extern jmp_buf _assert_trampoline;

#define _ASSERT_CMP(x, y, op, fmt)                                          \
	do {                                                                    \
		if (!(x op y)) {                                                    \
			test_output(                                                    \
				msgt_err,                                                   \
				"%s:%d: assert(%s " #op " %s) FAILED, " fmt " " #op " " fmt \
				" is false",                                                \
				__FILE__,                                                   \
				__LINE__,                                                   \
				#x,                                                         \
				#y,                                                         \
				x,                                                          \
				y                                                           \
			);                                                              \
			longjmp(_assert_trampoline, 1);                                 \
		}                                                                   \
	} while (0)

#define assert_null(x) _ASSERT_CMP(x, NULL, ==, "NULL")
#define assert_not_null(x) _ASSERT_CMP(x, NULL, !=, "NULL")
#define assert_int(x, op, y) _ASSERT_CMP(x, y, op, "%d")
#define assert_int_eq(x, y) assert_int(x, ==, y)
#define assert_int_neq(x, y) assert_int(x, !=, y)
#define assert_uint(x, op, y) _ASSERT_CMP(x, y, op, "%u")
#define assert_uint_eq(x, y) assert_uint(x, ==, y)
#define assert_uint_neq(x, y) assert_uint(x, !=, y)
#define assert_long(x, op, y) _ASSERT_CMP(x, y, op, "%ld")
#define assert_long_eq(x, y) assert_long(x, ==, y)
#define assert_long_neq(x, y) assert_long(x, !=, y)
#define assert_ulong(x, op, y) _ASSERT_CMP(x, y, op, "%lu")
#define assert_ulong_eq(x, y) assert_ulong(x, ==, y)
#define assert_ulong_neq(x, y) assert_ulong(x, !=, y)
#define assert_llong(x, op, y) _ASSERT_CMP(x, y, op, "%lld")
#define assert_llong_eq(x, y) assert_llong(x, ==, y)
#define assert_llong_neq(x, y) assert_llong(x, !=, y)

#define assert_quiet(expr)                    \
	do {                                      \
		if (!(expr))                          \
			longjmp(_assert_trampoline, 1);   \
	} while (0);

/* Random generators */

void
random_string(char *buf, size_t width);
unsigned int
random_uint(void);
unsigned long
random_ulong(void);

#endif
