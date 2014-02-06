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
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h> /* for endian conversion */

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

#include "compressor.h"
#include "util.h"

#pragma pack(push, 1)
struct compressed_block
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

int expand_input(struct sqdelta_header* dh,
		struct compressed_block* source_blocks,
		struct squashfs_header* sb,
		struct mmap_file* source_f,
		struct mmap_file* patch_f,
		struct mmap_file* temp_source_f)
{
	size_t prev_offset = 0;
	size_t i;

	for (i = 0; i < dh->block_count; ++i)
	{
		size_t offset = ntohl(source_blocks[i].offset);
		size_t length = ntohl(source_blocks[i].length);

		void* in_pos = mmap_read(source_f, prev_offset,
				offset - prev_offset);
		void* out_pos = mmap_read(temp_source_f,
				prev_offset, offset - prev_offset);

		if (!in_pos || !out_pos)
			return 0;

		memcpy(out_pos, in_pos, offset - prev_offset);
		prev_offset = offset + length;
	}

	/* the last block */
	{
		void* in_pos = mmap_read(source_f, prev_offset,
				source_f->length - prev_offset);
		void* out_pos = mmap_read(temp_source_f,
				prev_offset, source_f->length - prev_offset);

		if (!in_pos || !out_pos)
			return 0;

		memcpy(out_pos, in_pos, source_f->length - prev_offset);
	}

	prev_offset = source_f->length;
	for (i = 0; i < dh->block_count; ++i)
	{
		size_t offset = ntohl(source_blocks[i].offset);
		size_t length = ntohl(source_blocks[i].length);
		size_t unc_length = ntohl(source_blocks[i].uncompressed_length);
		size_t ret;

		void* in_pos = mmap_read(source_f, offset, length);
		void* out_pos = mmap_read(temp_source_f,
				prev_offset, unc_length);

		if (!in_pos || !out_pos)
			return 0;

		ret = compressor_decompress(sb->compression,
				out_pos, in_pos, length, unc_length);

		if (ret != unc_length)
		{
			if (ret != 0)
				fprintf(stderr, "Block decompression resulted in different size.\n"
						"\toffset: 0x%08lx\n"
						"\tlength: %lu\n"
						"\texpected unpacked length: %lu\n"
						"\treal unpacked length: %lu\n",
						offset, length, unc_length, ret);

			return 0;
		}

		prev_offset += unc_length;
	}

	/* copy the block lists and the header */
	{
		size_t block_list_size = sizeof(*source_blocks) * dh->block_count;

		void* in_pos = mmap_read(patch_f, sizeof(*dh),
				block_list_size);
		void* out_pos = mmap_read(temp_source_f,
				prev_offset, block_list_size);
		if (!in_pos || !out_pos)
			return 0;

		memcpy(out_pos, in_pos, block_list_size);

		prev_offset += block_list_size;
		in_pos = mmap_read(patch_f, 0, sizeof(*dh));
		out_pos = mmap_read(temp_source_f, prev_offset, sizeof(*dh));
		if (!in_pos || !out_pos)
			return 0;
		memcpy(out_pos, in_pos, sizeof(*dh));
	}

	return 1;
}

int run_xdelta3(struct mmap_file* patch, struct mmap_file* output,
		const char* input_path)
{
	pid_t child_pid = fork();

	if (child_pid == -1)
	{
		fprintf(stderr, "fork() failed\n"
				"\terrno: %s\n", strerror(errno));
		return 0;
	}

	if (child_pid == 0)
	{
		/* child */
		if (close(0) == -1)
		{
			fprintf(stderr, "Unable to close stdin in child\n"
					"\terrno: %s\n", strerror(errno));
			exit(1);
		}
		if (close(1) == -1)
		{
			fprintf(stderr, "Unable to close stdout in child\n"
					"\terrno: %s\n", strerror(errno));
			exit(1);
		}

		if (dup2(patch->fd, 0) == -1)
		{
			fprintf(stderr, "Unable to dup2() patch file into stdin\n"
					"\terrno: %s\n", strerror(errno));
			exit(1);
		}
		if (dup2(output->fd, 1) == -1)
		{
			fprintf(stderr, "Unable to dup2() output file into stdout\n"
					"\terrno: %s\n", strerror(errno));
			exit(1);
		}

		if (execlp("xdelta3",
					"xdelta3", "-c", "-d", "-s", input_path, 0) == -1)
		{
			fprintf(stderr, "execlp() failed\n"
					"\terrno: %s\n", strerror(errno));
			exit(1);
		}
	}
	else
	{
		int ret;
		waitpid(child_pid, &ret, 0);

		if (WEXITSTATUS(ret) != 0)
		{
			fprintf(stderr, "Child exited with non-success status\n"
					"\texit status: %d\n", WEXITSTATUS(ret));
			return 0;
		}
	}

	return 1;
}

int main(int argc, char* argv[])
{
	const char* source_file;
	const char* patch_file;
	const char* target_file;

	struct mmap_file source_f;
	struct mmap_file patch_f;
	struct mmap_file temp_source_f;
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
			struct compressed_block* source_blocks;
			char tmp_name_buf[] = "tmp.XXXXXX";
			size_t tmp_length = 0;
			size_t block_list_size;

			struct sqdelta_header dh = read_sqdelta_header(&patch_f);
			if (dh.magic == 0)
				break;

			source_blocks = mmap_read(&patch_f, sizeof(dh),
					sizeof(*source_blocks) * dh.block_count);
			if (!source_blocks)
				break;

			/* open target before chdir() */
			target_f = mmap_create_without_mapping(target_file);
			if (target_f.fd == -1)
				break;

			do
			{
				size_t i;

				{
					const char* tmpdir = getenv("TMPDIR");
#ifdef _P_tmpdir
					if (!tmpdir)
						tmpdir = P_tmpdir;
#endif
					if (!tmpdir)
						tmpdir = "/tmp";

					if (chdir(tmpdir) == -1)
					{
						fprintf(stderr, "Unable to enter temporary directory.\n"
								"\tpath: %s\n"
								"\terrno: %s\n", tmpdir, strerror(errno));
						break;
					}
				}

				block_list_size = sizeof(*source_blocks) * dh.block_count;

				tmp_length += source_f.length;
				tmp_length += sizeof(dh);
				tmp_length += block_list_size;

				for (i = 0; i < dh.block_count; ++i)
				{
					size_t unc_length = ntohl(source_blocks[i].uncompressed_length);
					tmp_length += unc_length;
				}

				temp_source_f = mmap_create_temp(tmp_name_buf, tmp_length);

				do
				{
					if (!expand_input(&dh, source_blocks, &sb,
								&source_f, &patch_f, &temp_source_f))
						break;

					mmap_close(&temp_source_f);

					if (lseek(patch_f.fd, sizeof(dh) + block_list_size,
								SEEK_SET) == -1)
					{
						fprintf(stderr, "Unable to seek patch file for applying.\n"
								"\terrno: %s\n", strerror(errno));
						break;
					}

					/* run xdelta3 to obtain the expanded target file */
					if (!run_xdelta3(&patch_f, &target_f, tmp_name_buf))
						break;
				} while (0);

				unlink(tmp_name_buf);
			} while (0);

			mmap_close(&target_f);
		} while (0);

		mmap_close(&patch_f);
	} while (0);

	mmap_close(&source_f);

	return ret;
}
