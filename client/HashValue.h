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

#pragma once


#ifndef DCPLUSPLUS_DCPP_HASH_VALUE_H
#define DCPLUSPLUS_DCPP_HASH_VALUE_H

#include "TigerHash.h"
#include "Encoder.h"

extern const string g_tth;

inline bool isTTHBase64(const string& p_str)
{
	return p_str.size() == 43 && p_str.compare(0, 4, g_tth) == 0;
}

template<class Hasher>
struct HashValue
{
	static const size_t BITS = Hasher::BITS;
	static const size_t BYTES = Hasher::BYTES;
	
	HashValue()
	{
		memzero(&data, sizeof(data));
	}
	explicit HashValue(const uint8_t* aData)
	{
		memcpy(data, aData, BYTES);
	}
	explicit HashValue(const char* p_base32, unsigned p_len)
	{
		dcassert(p_len == 39);
		Encoder::fromBase32(p_base32, data, BYTES);
	}
	explicit HashValue(const std::string& base32)
	{
		//dcassert(base32.length() == 39);
		Encoder::fromBase32(base32.c_str(), data, BYTES);
	}
	bool operator!=(const HashValue& rhs) const
	{
		return !(*this == rhs);
	}
	bool operator==(const HashValue& rhs) const
	{
		return memcmp(data, rhs.data, BYTES) == 0;
	}
	bool operator<(const HashValue& rhs) const
	{
		return memcmp(data, rhs.data, BYTES) < 0;
	}
	std::string toBase32() const
	{
		return Encoder::toBase32(data, BYTES);
	}
	std::string& toBase32(std::string& tmp) const
	{
		return Encoder::toBase32(data, BYTES, tmp);
	}
	size_t toHash() const
	{
		// RVO should handle this as efficiently as reinterpret_cast version
		size_t hvHash;
		memcpy(&hvHash, data, sizeof(hvHash)); //-V512
		return hvHash;
	}
	
	uint8_t data[BYTES];
};

class TigerHash;
template<class Hasher> struct HashValue;
typedef HashValue<TigerHash> TTHValue;

/*namespace boost
{
template<typename T>
struct hash<HashValue<T>>
{
    size_t operator()(const HashValue<T>& rhs) const
    {
        return rhs.toHash();
    }
};
}
*/
namespace std
{
template<typename T>
struct hash<HashValue<T> >
{
	size_t operator()(const HashValue<T>& rhs) const
	{
		return rhs.toHash();
	}
};

template<typename T>
struct hash<HashValue<T>* >
{
	size_t operator()(const HashValue<T>* rhs) const
	{
		return *(size_t*)rhs;
	}
};

template<typename T>
struct equal_to<HashValue<T>*>
{
	bool operator()(const HashValue<T>* lhs, const HashValue<T>* rhs) const
	{
		return (*lhs) == (*rhs);
	}
};

}

#endif // !defined(HASH_VALUE_H)

/**
* @file
* $Id: HashValue.h 568 2011-07-24 18:28:43Z bigmuscle $
*/
