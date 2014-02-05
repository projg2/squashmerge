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

#include "compressor.h"

int compressor_init(enum compressor_id c)
{
	switch (c)
	{
		case COMP_LZO:
#ifdef ENABLE_LZO
			if (lzo_init() != LZO_E_OK)
			{
				fprintf(stderr, "lzo_init() failed");
				return 0;
			}
#else
			fprintf(stderr, "LZO support disabled at build time");
			return 0;
#endif
			break;
	}

	return 1;
}

size_t compressor_decompress(enum compressor_id c,
		void* dest, const void* src, size_t length, size_t out_size)
{
	switch (c)
	{
		case COMP_LZO:
#ifdef ENABLE_LZO
		{
			lzo_uint out_bytes = out_size;

			if (lzo1x_decompress_safe(src, length, dest, &out_bytes, 0) != LZO_E_OK)
			{
				fprintf(stderr, "LZO decompression failed (corrupted data?)");
				return 0;
			}

			return out_bytes;
		}
#endif
	}
}
