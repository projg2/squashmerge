/**
 * SquashFS delta merge tool
 * (c) 2014 Michał Górny
 * Released under the terms of the 2-clause BSD license
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"

struct mmap_file mmap_open(const char* path)
{
	struct mmap_file out;
	off_t length;

	out.fd = open(path, O_RDONLY);
	if (out.fd == -1)
	{
		fprintf(stderr, "Unable to open file.\n"
				"\tpath: %s\n"
				"\terrno: %s\n", path, strerror(errno));
		return out;
	}

	length = lseek(out.fd, 0, SEEK_END);
	if (length == -1)
	{
		fprintf(stderr, "Unable to seek file (is it a regular file?).\n"
				"\tpath: %s\n"
				"\terrno: %s\n", path, strerror(errno));
		close(out.fd);
		out.fd = -1;
		return out;
	}

	out.length = length;
	out.data = mmap(0, out.length, PROT_READ, MAP_SHARED, out.fd, 0);
	if (out.data == MAP_FAILED)
	{
		fprintf(stderr, "Unable to mmap() file.\n"
				"\tpath: %s\n"
				"\terrno: %s\n", path, strerror(errno));
		close(out.fd);
		out.fd = -1;
		return out;
	}

	return out;
}

struct mmap_file mmap_create_temp(char* path_buf, size_t size)
{
	struct mmap_file out;

	out.fd = mkstemp(path_buf);
	if (out.fd == -1)
	{
		fprintf(stderr, "Unable to create a temporary file.\n"
				"\terrno: %s\n", strerror(errno));
		return out;
	}

	if (ftruncate(out.fd, size) == -1)
	{
		fprintf(stderr, "Unable to expand the file to requested size.\n"
				"\tpath: %s\n"
				"\terrno: %s\n", path_buf, strerror(errno));
		close(out.fd);
		out.fd = -1;
		return out;
	}

	out.length = size;
	out.data = mmap(0, out.length, PROT_WRITE, MAP_SHARED, out.fd, 0);
	if (out.data == MAP_FAILED)
	{
		fprintf(stderr, "Unable to mmap() file.\n"
				"\tpath: %s\n"
				"\terrno: %s\n", path_buf, strerror(errno));
		close(out.fd);
		out.fd = -1;
		return out;
	}

	return out;
}

struct mmap_file mmap_create_without_mapping(const char* path)
{
	struct mmap_file out;

	out.fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (out.fd == -1)
	{
		fprintf(stderr, "Unable to open file.\n"
				"\tpath: %s\n"
				"\terrno: %s\n", path, strerror(errno));
		return out;
	}
	out.data = 0;
	out.length = 0;

	return out;
}

void mmap_close(struct mmap_file* f)
{
	if (f->data)
	{
		if (msync(f->data, f->length, MS_SYNC) == -1)
		{
			fprintf(stderr, "File update failed.\n"
					"\terrno: %s\n", strerror(errno));
		}

		if (munmap(f->data, f->length) == -1)
		{
			fprintf(stderr, "Unable unmap file.\n"
					"\terrno: %s\n", strerror(errno));
		}
	}

	if (close(f->fd) == -1)
	{
		fprintf(stderr, "Unable to close file.\n"
				"\terrno: %s\n", strerror(errno));
	}
}

void* mmap_read(const struct mmap_file* f, size_t offset, size_t length)
{
	char* pos = f->data;

	if (offset + length > f->length)
	{
		fprintf(stderr, "Trying to read past file.\n");
		return 0;
	}

	return pos + offset;
}

size_t mmap_write(const struct mmap_file* f, void* data, size_t offset, size_t length)
{
	char* pos = f->data;

	if (offset + length > f->length)
	{
		fprintf(stderr, "Trying to write past file.\n");
		return 0;
	}

	memcpy(pos + offset, data, length);
	return length;
}
