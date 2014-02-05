/**
 * SquashFS delta merge tool
 * (c) 2014 Michał Górny
 * Released under the terms of the 2-clause BSD license
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> /* for endian conversion */

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

#include "compressor.h"
#include "util.h"

#pragma pack(push, 1)
struct serialized_compressed_block
{
	uint32_t offset;
	uint32_t length;
	uint32_t uncompressed_length;
};

struct sqdelta_header
{
	uint32_t magic;
	uint32_t flags;
	uint32_t block_count;
};

struct squashfs_header
{
	uint32_t magic;
	uint32_t unused[4];
	uint16_t compression;
};
#pragma pack(pop)

const uint32_t sqdelta_magic = 0x5371ceb4UL;
const uint32_t squashfs_magic = 0x73717368UL;
const uint32_t squashfs_magic_be = 0x68737173UL;

struct squashfs_header read_squashfs_header(const struct mmap_file* f)
{
	struct squashfs_header* h;
	struct squashfs_header out;
	uint16_t comp;

	out.magic = 0;

	h = mmap_read(f, 0, sizeof(*h));
	if (!h)
		return out;

	if (h->magic == squashfs_magic)
		comp = h->compression;
	else if (h->magic == squashfs_magic_be)
		comp = h->compression << 8 | h->compression >> 8;
	else
	{
		fprintf(stderr, "Invalid magic in squashfs input.\n");
		return out;
	}

	switch (comp)
	{
		case COMP_LZO:
			break;
		default:
			fprintf(stderr, "Unsupported compression method in squashfs input.\n"
				"\tcompressor id: %d\n", comp);
			return out;
	}

	if (!compressor_init(comp))
		return out;

	out.magic = squashfs_magic;
	out.compression = comp;

	return out;
}

struct sqdelta_header read_sqdelta_header(const struct mmap_file* f)
{
	struct sqdelta_header* h;
	struct sqdelta_header out;

	out.magic = 0;

	h = mmap_read(f, 0, sizeof(*h));
	if (!h)
		return out;

	if (ntohl(h->magic) != sqdelta_magic)
	{
		fprintf(stderr, "Incorrect magic in patch file.\n"
				"\tmagic: %08x, expected: %08x\n",
				ntohl(h->magic), sqdelta_magic);
		return out;
	}

	out.flags = ntohl(h->flags);
	if (out.flags)
	{
		fprintf(stderr, "Unknown flag enabled in patch file.\n"
				"\tflags: %08x\n", ntohl(h->flags));
		return out;
	}

	out.block_count = ntohl(h->block_count);
	out.magic = sqdelta_magic;

	return out;
}

int main(int argc, char* argv[])
{
	const char* source_file;
	const char* patch_file;
	const char* target_file;

	struct mmap_file source_f;
	struct mmap_file patch_f;
	struct mmap_file target_f;

	int ret = 1;

	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s <source> <patch> <target>\n", argv[0]);
		return 1;
	}

	source_file = argv[1];
	patch_file = argv[2];
	target_file = argv[3];

	source_f = mmap_open(source_file);
	if (source_f.fd == -1)
		return 1;

	do
	{
		struct squashfs_header sb = read_squashfs_header(&source_f);
		if (sb.magic == 0)
			break;

		patch_f = mmap_open(patch_file);
		if (patch_f.fd == -1)
			break;

		do
		{
			struct sqdelta_header dh = read_sqdelta_header(&patch_f);
			if (dh.magic == 0)
				break;
		} while (0);

		mmap_close(&patch_f);
	} while (0);

	mmap_close(&source_f);

	return ret;
}
