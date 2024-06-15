#ifndef INCLUDE_HASH_H
#define INCLUDE_HASH_H

#include <stdint.h>

/* Use these */
#define HASH_STR_32 djb2

uint32_t
djb2(const unsigned char *str);

int
hash_file(char *filename, uint64_t *hash);

#endif
