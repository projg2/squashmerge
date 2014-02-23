/**
 * SquashFS delta merge tool
 * (c) 2014 Michał Górny
 * Released under the terms of the 2-clause BSD license
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <stdio.h>

#ifdef ENABLE_LZO
#	include <lzo/lzo1x.h>
#endif
#ifdef ENABLE_LZ4
#	include <lz4.h>
#	include <lz4hc.h>
#endif

#include "compressor.h"

enum compressor_id
{
	COMP_ID_LZO = 0x01 << 24,
	COMP_ID_LZ4 = 0x02 << 24,
	COMP_ID_MASK = 0xff << 24
};

#ifdef ENABLE_LZO
enum lzo_options
{
	COMP_LZO_ALGO_LZO1X_999_MIN = 0x01,
	COMP_LZO_ALGO_LZO1X_999_MAX = 0x09,
	COMP_LZO_ALGO_MASK = 0x0f,
	COMP_LZO_OPTIMIZED = 0x10,
	COMP_LZO_KNOWN_FLAG_MASK = COMP_LZO_OPTIMIZED,
	COMP_LZO_FLAG_MASK = 0xfffff0
};
#endif

#ifdef ENABLE_LZ4
enum lz4_options
{
	COMP_LZ4_HC = 0x01,

	COMP_LZ4_KNOWN_FLAG_MASK = COMP_LZ4_HC,
	COMP_LZ4_FLAG_MASK = 0xffffff
};
#endif

int compressor_init(uint32_t c)
{
	switch (c & COMP_ID_MASK)
	{
		case COMP_ID_LZO:
#ifdef ENABLE_LZO
			if ((c & COMP_LZO_ALGO_MASK) < COMP_LZO_ALGO_LZO1X_999_MIN
					|| (c & COMP_LZO_ALGO_MASK) > COMP_LZO_ALGO_LZO1X_999_MAX)
			{
				fprintf(stderr, "Unsupported LZO variant %02x\n",
						c & COMP_LZO_ALGO_MASK);
				return 0;
			}

			if (((c & COMP_LZO_FLAG_MASK) & ~COMP_LZO_KNOWN_FLAG_MASK) != 0)
			{
				fprintf(stderr, "Unknown LZO flags enabled: %06x\n",
						c & COMP_LZO_FLAG_MASK & ~COMP_LZO_KNOWN_FLAG_MASK);
				return 0;
			}

			if (lzo_init() != LZO_E_OK)
			{
				fprintf(stderr, "lzo_init() failed\n");
				return 0;
			}
#else
			fprintf(stderr, "LZO support disabled at build time\n");
			return 0;
#endif
			break;

		case COMP_ID_LZ4:
#ifdef ENABLE_LZ4
			if (((c & COMP_LZ4_FLAG_MASK) & ~COMP_LZ4_KNOWN_FLAG_MASK) != 0)
			{
				fprintf(stderr, "Unknown LZ4 flags enabled: %08x\n",
						c & COMP_LZ4_FLAG_MASK);
				return 0;
			}
#else
			fprintf(stderr, "LZ4 support disabled at build time\n");
			return 0;
#endif
			break;
		default:
			fprintf(stderr, "Unknown compressor %02x requested\n",
					c & COMP_ID_MASK);
	}

	return 1;
}

size_t compressor_compress(uint32_t c,
		void* dest, void* src, size_t length, size_t out_size)
{
	switch (c & COMP_ID_MASK)
	{
		case COMP_ID_LZO:
#ifdef ENABLE_LZO
		{
			char workspace[LZO1X_999_MEM_COMPRESS];

			lzo_uint out_bytes = out_size;
			lzo_uint orig_size = length;

			if (lzo1x_999_compress_level(src, length, dest, &out_bytes,
						workspace, 0, 0, 0, c & COMP_LZO_ALGO_MASK) != LZO_E_OK)
			{
				fprintf(stderr, "LZO compression failed\n");
				return 0;
			}

			if (c & COMP_LZO_OPTIMIZED)
			{
				if (lzo1x_optimize(dest, out_bytes, src, &orig_size, 0)
						!= LZO_E_OK)
				{
					fprintf(stderr, "LZO optimization failed\n");
					return 0;
				}
			}

			if (orig_size != length)
			{
				fprintf(stderr, "LZO optimization resulted in different input length\n");
				return 0;
			}

			return out_bytes;
		}
#endif
			break;

		case COMP_ID_LZ4:
#ifdef ENABLE_LZ4
		{
			int out_bytes;

			if (c & COMP_LZ4_HC)
				out_bytes = LZ4_compressHC_limitedOutput(src, dest, length, out_size);
			else
				out_bytes = LZ4_compress_limitedOutput(src, dest, length, out_size);

			if (out_bytes < 0)
			{
				fprintf(stderr, "LZ4 compression failed\n");
				return 0;
			}

			return out_bytes;
		}
#endif
			break;
	}

	return 0;
}
size_t compressor_decompress(uint32_t c,
		void* dest, const void* src, size_t length, size_t out_size)
{
	switch (c & COMP_ID_MASK)
	{
		case COMP_ID_LZO:
#ifdef ENABLE_LZO
		{
			lzo_uint out_bytes = out_size;

			if (lzo1x_decompress_safe(src, length, dest, &out_bytes, 0) != LZO_E_OK)
			{
				fprintf(stderr, "LZO decompression failed (corrupted data?)\n");
				return 0;
			}

			return out_bytes;
		}
#endif
			break;

		case COMP_ID_LZ4:
#ifdef ENABLE_LZ4
		{
			int out_bytes;

			out_bytes = LZ4_decompress_safe(src, dest, length, out_size);

			if (out_bytes < 0)
			{
				fprintf(stderr, "LZ4 decompression failed (corrupted data?)\n");
				return 0;
			}

			return out_bytes;
		}
#endif
			break;
	}

	return 0;
}
