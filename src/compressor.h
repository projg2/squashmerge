/**
 * SquashFS delta merge tool
 * (c) 2014 Michał Górny
 * Released under the terms of the 2-clause BSD license
 */

#pragma once

#ifndef SDT_COMPRESSOR_H
#define SDT_COMPRESSOR_H 1

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

int compressor_init(uint32_t c);
size_t compressor_compress(uint32_t c,
		void* dest, void* src, size_t length, size_t out_size);
size_t compressor_decompress(uint32_t c,
		void* dest, const void* src, size_t length, size_t out_size);

#endif /*SDT_COMPRESSOR_H*/
