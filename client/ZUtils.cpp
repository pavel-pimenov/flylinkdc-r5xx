/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "ZUtils.h"
#include "ResourceManager.h"
#include "ShareManager.h"

bool ZFilter::g_is_disable_compression = false;

ZFilter::ZFilter() : totalIn(0), totalOut(0), compressing(true)
{
	memzero(&zs, sizeof(zs));
	const auto l_result = deflateInit(&zs, SETTING(MAX_COMPRESSION));
	if (l_result != Z_OK)
	{
		if (l_result == Z_MEM_ERROR)
		{
			g_is_disable_compression = true;
			ShareManager::tryFixBadAlloc();
		}
		throw Exception(STRING(COMPRESSION_ERROR) + " Error code deflateInit = " + Util::toString(l_result));
	}
}

ZFilter::~ZFilter()
{
#ifdef ZLIB_DEBUG
	dcdebug("ZFilter end, %ld/%ld = %.04f\n", zs.total_out, zs.total_in, (float)zs.total_out / max((float)zs.total_in, (float)1));
#endif
	deflateEnd(&zs);
}

bool ZFilter::operator()(const void* in, size_t& insize, void* out, size_t& outsize)
{
	if (outsize == 0)
		return false;
		
	zs.next_in = (Bytef*)in;
	zs.next_out = (Bytef*)out;
	
#ifdef ZLIB_DEBUG
	dcdebug("ZFilter: totalOut = %lld, totalIn = %lld, outsize = %d\n", totalOut, totalIn, outsize);
#endif
	
	// Check if there's any use compressing; if not, save some cpu...
	if (compressing && insize > 0 && outsize > 16 && (totalIn > (64 * 1024)) && ((static_cast<double>(totalOut) / totalIn) > 0.95))
	{
		zs.avail_in = 0;
		zs.avail_out = outsize;
		
		// Starting with zlib 1.2.9, the deflateParams API has changed.
		auto err = ::deflateParams(&zs, 0, Z_DEFAULT_STRATEGY);
#if ZLIB_VERNUM >= 0x1290
		if (err == Z_STREAM_ERROR)
		{
#else
		if (err != Z_OK)
		{
#endif
			throw Exception(STRING(COMPRESSION_ERROR));
		}
		
		zs.avail_in = insize;
		compressing = false;
		dcdebug("ZFilter: Dynamically disabled compression\n");
		
		// Check if we ate all space already...
#if ZLIB_VERNUM >= 0x1290
		if (err == Z_BUF_ERROR)
		{
#else
		if (zs.avail_out == 0)
		{
#endif
			outsize = outsize - zs.avail_out;
			insize = insize - zs.avail_in;
			totalOut += outsize;
			totalIn += insize;
			return true;
		}
	}
	else
	{
		zs.avail_in = insize;
		zs.avail_out = outsize;
	}
	
	if (insize == 0)
	{
		int err = ::deflate(&zs, Z_FINISH);
		if (err != Z_OK && err != Z_STREAM_END)
			throw Exception(STRING(COMPRESSION_ERROR));
			
		outsize = outsize - zs.avail_out;
		insize = insize - zs.avail_in;
		totalOut += outsize;
		totalIn += insize;
		return err == Z_OK;
	}
	else
	{
		int err = ::deflate(&zs, Z_NO_FLUSH);
		if (err != Z_OK)
			throw Exception(STRING(COMPRESSION_ERROR));
			
		outsize = outsize - zs.avail_out;
		insize = insize - zs.avail_in;
		totalOut += outsize;
		totalIn += insize;
		return true;
	}
}

UnZFilter::UnZFilter()
{
	memzero(&zs, sizeof(zs));
	if (inflateInit(&zs) != Z_OK)
		throw Exception(STRING(DECOMPRESSION_ERROR));
}

UnZFilter::~UnZFilter()
{
#ifdef ZLIB_DEBUG
	dcdebug("UnZFilter end, %ld/%ld = %.04f\n", zs.total_out, zs.total_in, (float)zs.total_out / max((float)zs.total_in, (float)1));
#endif
	inflateEnd(&zs);
}

bool UnZFilter::operator()(const void* in, size_t& insize, void* out, size_t& outsize)
{
	if (outsize == 0)
		return 0;
		
	zs.avail_in = insize;
	zs.next_in = (Bytef*)in;
	zs.avail_out = outsize;
	zs.next_out = (Bytef*)out;
	
	int err = ::inflate(&zs, Z_NO_FLUSH);
	
	// see zlib/contrib/minizip/unzip.c, Z_BUF_ERROR means we should have padded
	// with a dummy byte if at end of stream - since we don't do this it's not a real
	// error
	if (!(err == Z_OK || err == Z_STREAM_END || (err == Z_BUF_ERROR && in == NULL)))
		throw Exception(STRING(DECOMPRESSION_ERROR));
		
	outsize = outsize - zs.avail_out;
	insize = insize - zs.avail_in;
	return err == Z_OK;
}

/**
 * @file
 * $Id: ZUtils.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
