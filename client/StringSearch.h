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


#ifndef DCPLUSPLUS_DCPP_STRING_SEARCH_H
#define DCPLUSPLUS_DCPP_STRING_SEARCH_H

#include "Text.h"

/**
 * A class that implements a fast substring search algo suited for matching
 * one pattern against many strings (currently Quick Search, a variant of
 * Boyer-Moore. Code based on "A very fast substring search algorithm" by
 * D. Sunday).
 * @todo Perhaps find an algo suitable for matching multiple substrings.
 */
class StringSearch
#ifdef _DEBUG
	: boost::noncopyable
#endif
	
{
	public:
		typedef vector<StringSearch> List;
		
		explicit StringSearch(const string& aPattern) noexcept :
			pattern(Text::toLower(aPattern))
		{
			initDelta1();
		}
		StringSearch(const StringSearch& rhs) noexcept :
			pattern(rhs.pattern)
		{
			memcpy(delta1, rhs.delta1, sizeof(delta1));
		}
		const StringSearch& operator=(const StringSearch& rhs)
		{
			memcpy(delta1, rhs.delta1, sizeof(delta1));
			pattern = rhs.pattern;
			return *this;
		}
		bool operator==(const StringSearch& rhs) const
		{
			return pattern.compare(rhs.pattern) == 0;
		}
		
		const string& getPattern() const
		{
			return pattern;
		}
		
		/** Match a text against the pattern */
		bool match(const string& aText) const noexcept
		{
			// Lower-case representation of UTF-8 string, since we no longer have that 1 char = 1 byte...
			string lower;
			Text::toLower(aText, lower);
			return matchLower(lower);
		}
		/** Match a text against the pattern */
		bool matchLower(const string& aText) const noexcept
		{
			dcassert(Text::toLower(aText) == aText);
			const string::size_type plen = pattern.length();
			const string::size_type tlen = aText.length();
			if (tlen < plen)// fix UTF-8 support
			{
				//          static int l_cnt = 0;
				//          dcdebug("aText.length() < plen %d, [name: %s  pattern: %s] [%d %d]\n",
				//               ++l_cnt,aText.c_str(),pattern.c_str(),aText.length(), pattern.length());
				return false;
			}
			// uint8_t to avoid problems with signed char pointer arithmetic
			uint8_t *tx = (uint8_t*)aText.c_str();
			uint8_t *px = (uint8_t*)pattern.c_str();
			uint8_t *end = tx + tlen - plen + 1;
			while (tx < end)
			{
				size_t i = 0;
				for (; px[i] && (px[i] == tx[i]); ++i)
					;       // Empty!
					
				if (px[i] == 0)
					return true;
					
				tx += delta1[tx[plen]];
			}
			
			return false;
		}
	private:
		static const uint16_t m_ASIZE = 256;
		/**
		 * Delta1 shift, uint16_t because we expect all patterns to be shorter than 2^16
		 * chars.
		 */
		uint16_t delta1[m_ASIZE];
		string pattern;
		
		void initDelta1()
		{
			register uint16_t x = (uint16_t)(pattern.length() + 1);
			register uint16_t i;
			for (i = 0; i < m_ASIZE; ++i)
			{
				delta1[i] = x;
			}
			x--;// x = pattern.length();// [!] Flylink
			register uint8_t* p = (uint8_t*)pattern.data();
			for (i = 0; i < x; ++i)
			{
				delta1[p[i]] = (uint16_t)(x - i);
			}
		}
};

#endif // DCPLUSPLUS_DCPP_STRING_SEARCH_H

/**
 * @file
 * $Id: StringSearch.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
