/**
 * SquashFS delta merge tool
 * (c) 2014 Michał Górny
 * Released under the terms of the 2-clause BSD license
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <pthread.h>

#include <stdlib.h>
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
	uint32_t compression;
	uint32_t block_count;
};
#pragma pack(pop)

const uint32_t sqdelta_magic = 0x5371ceb4UL;

struct sqdelta_header read_sqdelta_header(const struct mmap_file* f,
		size_t offset)
{
	struct sqdelta_header* h;
	struct sqdelta_header out;

	out.magic = 0;

	h = mmap_read(f, offset, sizeof(*h));
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

	out.compression = ntohl(h->compression);
	out.block_count = ntohl(h->block_count);
	out.magic = sqdelta_magic;

	return out;
}

struct compress_data_shared
{
	struct sqdelta_header* dh;
	struct compressed_block* block_list;
	struct mmap_file* input_f;
	struct mmap_file* output_f;
	size_t* prev_offset;
	int thread_count;
};

struct compress_data_private
{
	struct compress_data_shared* shared;
	int thread_no;
	pthread_t thread_id;
};

int run_multithreaded(void* (*func) (void*), struct compress_data_shared* d)
{
	struct compress_data_private* pd;
	int i, spawned, ret;

	int num_cpus = 1;

#ifdef _SC_NPROCESSORS_ONLN
	num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (num_cpus < 1)
	{
		fprintf(stderr, "Warning: unable to get number of CPUs.\n");
		num_cpus = 1;
	}
#endif

	d->thread_count = num_cpus;

	pd = calloc(num_cpus, sizeof(*pd));
	if (!pd)
	{
		fprintf(stderr, "Unable to allocate memory for threading.\n"
				"\terrno: %s\n", strerror(errno));
		return 0;
	}

	for (i = 0; i < num_cpus; ++i)
	{
		pd[i].shared = d;
		pd[i].thread_no = i;

		ret = pthread_create(&pd[i].thread_id, 0, func, &pd[i]);
		if (ret != 0)
		{
			fprintf(stderr, "Unable to create thread %d.\n"
					"\terror: %s\n", i, strerror(ret));
			break;
		}
	}
	spawned = i;

	if (ret != 0)
	{
		int cret;

		for (i = 0; i < spawned; ++i)
		{
			cret = pthread_cancel(pd[i].thread_id);
			if (cret != 0)
				fprintf(stderr, "Warning: unable to cancel thread %d.\n"
						"\terror: %s\n", i, strerror(cret));
		}
	}

	for (i = 0; i < spawned; ++i)
	{
		int jret;
		void* res;

		jret = pthread_join(pd[i].thread_id, &res);
		if (jret != 0)
		{
			fprintf(stderr, "Warning: unable to join thread %d.\n"
					"\terror: %s\n", i, strerror(jret));
		}

		if (!res)
			ret = 1;
	}

	free(pd);

	return !ret;
}

void* decompress_blocks(void* data)
{
	struct compress_data_private* pd = data;

	struct sqdelta_header* dh = pd->shared->dh;
	struct compressed_block* source_blocks = pd->shared->block_list;
	struct mmap_file* source_f = pd->shared->input_f;
	struct mmap_file* temp_source_f = pd->shared->output_f;
	size_t prev_offset = *pd->shared->prev_offset;
	int id = pd->thread_no;
	int no_threads = pd->shared->thread_count;

	size_t i;

	for (i = 0; i < dh->block_count; ++i)
	{
		size_t unc_length = ntohl(source_blocks[i].uncompressed_length);

		if (i % no_threads == id)
		{
			size_t offset = ntohl(source_blocks[i].offset);
			size_t length = ntohl(source_blocks[i].length);
			size_t ret;

			void* in_pos = mmap_read(source_f, offset, length);
			void* out_pos = mmap_read(temp_source_f,
					prev_offset, unc_length);

			if (!in_pos || !out_pos)
				return 0;

			ret = compressor_decompress(dh->compression,
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
		}

		prev_offset += unc_length;
	}

	if (id == 0)
		*pd->shared->prev_offset = prev_offset;

	return pd;
}

int expand_input(struct sqdelta_header* dh,
		struct compressed_block* source_blocks,
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

	{
		struct compress_data_shared d;

		d.dh = dh;
		d.block_list = source_blocks;
		d.input_f = source_f;
		d.output_f = temp_source_f;
		d.prev_offset = &prev_offset;

		if (!run_multithreaded(decompress_blocks, &d))
			return 0;
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

void* compress_blocks(void* data)
{
	struct compress_data_private* pd = data;

	struct sqdelta_header* dh = pd->shared->dh;
	struct compressed_block* target_blocks = pd->shared->block_list;
	struct mmap_file* target_f = pd->shared->output_f;
	size_t prev_offset = *pd->shared->prev_offset;
	int id = pd->thread_no;
	int no_threads = pd->shared->thread_count;

	size_t i;

	for (i = dh->block_count; i > 0; --i)
	{
		size_t unc_length = ntohl(target_blocks[i - 1].uncompressed_length);

		prev_offset -= unc_length;

		if (i % no_threads == id)
		{
			size_t offset = ntohl(target_blocks[i - 1].offset);
			size_t length = ntohl(target_blocks[i - 1].length);
			size_t ret;

			void* in_pos;
			void* out_pos = mmap_read(target_f, offset, length);

			in_pos = mmap_read(target_f, prev_offset, unc_length);
			if (!in_pos || !out_pos)
				return 0;

			ret = compressor_compress(dh->compression,
					out_pos, in_pos, unc_length, length);

			if (ret != length)
			{
				if (ret != 0)
					fprintf(stderr, "Block re-compression resulted in different size.\n"
							"\toffset: 0x%08lx\n"
							"\tinput length: %lu\n"
							"\texpected packed length: %lu\n"
							"\treal packed length: %lu\n",
							offset, unc_length, length, ret);

				return 0;
			}
		}
	}

	if (id == 0)
		*pd->shared->prev_offset = prev_offset;

	return pd;
}

int squash_target_file(struct mmap_file* target_f)
{
	struct sqdelta_header dh;
	size_t block_list_size, block_list_offset;
	struct compressed_block* target_blocks;
	size_t prev_offset;

	dh = read_sqdelta_header(target_f, target_f->length - sizeof(dh));

	if (dh.magic == 0)
		return 0;
	block_list_size = sizeof(*target_blocks) * dh.block_count;
	block_list_offset = target_f->length - sizeof(dh) - block_list_size;

	target_blocks = mmap_read(target_f, block_list_offset,
			block_list_size);
	if (!target_blocks)
		return 0;

	prev_offset = block_list_offset;

	{
		struct compress_data_shared d;

		d.dh = &dh;
		d.block_list = target_blocks;
		d.output_f = target_f;
		d.prev_offset = &prev_offset;

		if (!run_multithreaded(compress_blocks, &d))
			return 0;
	}

	/* truncate the resulting file */
	if (ftruncate(target_f->fd, prev_offset) == -1)
	{
		fprintf(stderr, "Unable to truncate output file.\n"
				"\terrno: %s\n", strerror(errno));
		return 0;
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
		patch_f = mmap_open(patch_file);
		if (patch_f.fd == -1)
			break;

		do
		{
			struct compressed_block* source_blocks;
			char tmp_name_buf[] = "tmp.XXXXXX";
			size_t tmp_length = 0;
			size_t block_list_size;

			struct sqdelta_header dh = read_sqdelta_header(&patch_f, 0);
			if (dh.magic == 0)
				break;

			block_list_size = sizeof(*source_blocks) * dh.block_count;
			source_blocks = mmap_read(&patch_f, sizeof(dh),
					block_list_size);
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
					if (!expand_input(&dh, source_blocks,
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

					if (!mmap_map_created_file(&target_f))
						break;

					squash_target_file(&target_f);
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
