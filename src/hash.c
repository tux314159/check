#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>

#include "hash.h"

uint32_t
djb2(const unsigned char *str)
{
	uint32_t hash = 5381;
	unsigned char c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

int
hash_file(char *filename, uint64_t *hash)
{
	int fd = -1;
	struct stat sb;
	unsigned char *buf = NULL;

	fd = open(filename, O_RDONLY);
	if (fd == -1)
		goto cleanup_fail;
	if (fstat(fd, &sb))
		goto cleanup_fail;

	buf = malloc((size_t)sb.st_size + 1);
	if (!buf)
		goto cleanup_fail;
	if ((read(fd, buf, (size_t)sb.st_size)) == -1)
		goto cleanup_fail;
	buf[sb.st_size] = '\0';

	/* Pack the file size into the first 32 bits */
	*hash = 0;
	*hash = djb2(buf); /* TODO what the fuck this is a shit hash */
	*hash |= ((uint64_t)sb.st_size << 32);

	free(buf);
	close(fd);
	return 0;

cleanup_fail:
	free(buf);
	close(fd);
	return -1;
}
