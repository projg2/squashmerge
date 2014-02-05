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
#pragma pack(pop)

const uint32_t sqdelta_magic = 0x5371ceb4;

size_t read_sqdelta_header(FILE* f)
{
	struct sqdelta_header h;

	if (fread(&h, sizeof(h), 1, f) < 1)
	{
		if (feof(f))
			fprintf(stderr, "EOF reading patch file.\n");
		else
			fprintf(stderr, "Failure reading patch file.\n"
					"\terror: %s\n", strerror(errno));
		return 0;
	}

	if (ntohl(h.magic) != sqdelta_magic)
	{
		fprintf(stderr, "Incorrect magic in patch file.\n"
				"\tmagic: %08x, expected: %08x\n",
				ntohl(h.magic), sqdelta_magic);
		return 0;
	}

	if (ntohl(h.flags))
	{
		fprintf(stderr, "Unknown flag enabled in patch file.\n"
				"\tflags: %08x\n", ntohl(h.flags));
		return 0;
	}

	return ntohl(h.block_count);
}

int main(int argc, char* argv[])
{
	const char* source_file;
	const char* patch_file;
	const char* target_file;

	FILE* source_f;
	FILE* patch_f;
	FILE* target_f;

	int ret = 1;

	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s <source> <patch> <target>\n", argv[0]);
		return 1;
	}

	source_file = argv[1];
	patch_file = argv[2];
	target_file = argv[3];

	source_f = fopen(source_file, "rb");
	if (!source_f)
	{
		fprintf(stderr, "Unable to open source file.\n"
				"\tpath: %s\n"
				"\terror: %s\n", source_file, strerror(errno));
		return 1;
	}

	do
	{
		patch_f = fopen(patch_file, "rb");
		if (!patch_f)
		{
			fprintf(stderr, "Unable to open patch file.\n"
					"\tpath: %s\n"
					"\terror: %s\n", patch_file, strerror(errno));
			break;
		}

		do
		{
			size_t block_count = read_sqdelta_header(patch_f);
			if (block_count == 0)
				break;

			unlink(target_file);
			target_f = fopen(target_file, "wb");
			if (!target_f)
			{
				fprintf(stderr, "Unable to open target file.\n"
						"\tpath: %s\n"
						"\terror: %s\n", target_file, strerror(errno));
				break;
			}

			do
			{

			} while (0);
			fclose(target_f);
		} while (0);
		fclose(patch_f);
	} while (0);
	fclose(source_f);

	return ret;
}
