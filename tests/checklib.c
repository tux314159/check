#include <limits.h>
#include <stdlib.h>

#include "check.h"

void
random_string(char *buf, size_t width)
{
	assert_ulong(width, >, 0L);
	for (size_t i = 0; i < width - 1; i++) {
		buf[i] = random() % (127 - 33) + 33;
	}
	buf[width - 1] = '\0';
}

unsigned int
random_uint(void)
{
	return (unsigned long)random() % (unsigned long)INT_MAX;
}

unsigned long
random_ulong(void)
{
	return (unsigned long)random() % (unsigned long)LONG_MAX;
}
