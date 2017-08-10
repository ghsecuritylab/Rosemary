#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//#define DEBUG

#if defined (DEBUG)
#define debug(msg...) 	{ printf(msg); }
#else
#define debug(msg...) 	do{} while (0)
#endif

int mmc_read(char *path, long long start, long long length, char *buffer)
{
	char p[100];
	int fd, ret = 0;

	debug("%s [%s], blk_s: %lld blk_l: %lld ", __func__, path, start, length);
	if (0 != access(path, F_OK)) {
		printf("cannot access file (%s).\n", path);
		return -errno;
	}

	fd = open(path, O_RDONLY);
	lseek(fd, start, 0);
	ret = read(fd, buffer, length);
	debug("len:%d\n", ret);
	close(fd);

	return ret;
}

int mmc_write(char *path, long long start, long long length, char *buffer)
{
	char p[100];
	int fd, ret = 0;

	debug("%s [%s], blk_s: %lld blk_l: %lld ", __func__, path, start, length);
	if (0 != access(path, F_OK)) {
		printf("cannot access file (%s).\n", path);
		return -errno;
	}

	fd = open(path, O_RDWR);
	lseek(fd, start, 0);
	ret = write(fd, buffer, length);
	debug("len:%d\n", ret);
	close(fd);

	return ret;
}