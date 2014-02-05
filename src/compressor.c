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
