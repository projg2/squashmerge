/**
 * SquashFS delta merge tool
 * (c) 2014 Michał Górny
 * Released under the terms of the 2-clause BSD license
 */

#pragma once

#ifndef SDT_COMPRESSOR_H
#define SDT_COMPRESSOR_H 1

enum compressor_id
{
	COMP_LZO = 3
};

int compressor_init(enum compressor_id c);

#endif /*SDT_COMPRESSOR_H*/
