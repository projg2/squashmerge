/**
 * SquashFS delta merge tool
 * (c) 2014 Michał Górny
 * Released under the terms of the 2-clause BSD license
 */

#pragma once

#ifndef SDT_UTIL_H
#define SDT_UTIL_H 1

#include <stdlib.h>

struct mmap_file
{
	int fd;
	void* data;
	size_t length;
};

struct mmap_file mmap_open(const char* path);
struct mmap_file mmap_create_temp(char* path_buf, size_t size);
struct mmap_file mmap_create_without_mapping(const char* path);
int mmap_map_created_file(struct mmap_file* f);
void mmap_close(struct mmap_file* f);

void* mmap_read(const struct mmap_file* f, size_t offset, size_t length);

#endif /*!SDT_UTIL_H*/
