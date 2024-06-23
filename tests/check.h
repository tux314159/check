#ifndef INCLUDE_CHECK_H
#define INCLUDE_CHECK_H

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* ANSI escapes */
#define T_DIM "\x1b[2m"
#define T_BOLD "\x1b[1m"
#define T_ITAL "\x1b[3m"
#define T_LINE "\x1b[4m"
#define T_RED "\x1b[38;5;1m"
#define T_GREEN "\x1b[38;5;2m"
#define T_YELLOW "\x1b[38;5;3m"
#define T_BLUE "\x1b[38;5;4m"
#define T_NORM "\x1b[0m"

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
	{T_BLUE, T_YELLOW, T_RED}; // not a macro so we don't forget to update it

#define test_output(type, msg, ...)                            \
	do {                                                       \
		printf(                                                \
			" > " T_DIM "%s:%d: " T_NORM "%s" msg T_NORM "\n", \
			__FILE__,                                          \
			__LINE__,                                          \
			_msgtstr[type],                                    \
			__VA_ARGS__                                        \
		);                                                     \
		fflush(stdout);                                        \
	} while (0)

/* All the asserts */

extern jmp_buf _assert_trampoline;

#define _ASSERT_CMP(x, y, op, fmt)                                   \
	do {                                                             \
		if (!(x op y)) {                                             \
			test_output(                                             \
				msgt_err,                                            \
				"assert(%s " #op " %s) FAILED, " fmt " " #op " " fmt \
				" is false",                                         \
				#x,                                                  \
				#y,                                                  \
				x,                                                   \
				y                                                    \
			);                                                       \
			longjmp(_assert_trampoline, 1);                          \
		}                                                            \
	} while (0)

#define assert_null(x) _ASSERT_CMP(x, NULL, ==, "NULL")
#define assert_not_null(x) _ASSERT_CMP(x, NULL, !=, "NULL")
#define assert_ptr(x, op, y) _ASSERT_CMP(x, y, op, "%p")
#define assert_ptr_eq(x, y) assert_ptr(x, ==, y)
#define assert_ptr_neq(x, y) assert_ptr(x, !=, y)
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

#define assert_quiet(expr)                  \
	do {                                    \
		if (!(expr))                        \
			longjmp(_assert_trampoline, 1); \
	} while (0);

/* Random generators */

void
random_string(char *buf, size_t width);
unsigned int
random_uint(void);
unsigned long
random_ulong(void);

#endif
