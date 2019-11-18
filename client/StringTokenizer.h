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


#ifndef DCPLUSPLUS_CLIENT_STRING_TOKENIZER_H
#define DCPLUSPLUS_CLIENT_STRING_TOKENIZER_H

template<class T, class T2 = StringList>
class StringTokenizer
{
	private:
		T2 m_tokens;
		
		template<class T3>
		void slice(const T& str, const T3& tok, const size_t tokLen, int p_count_hint = 0)
		{
			if (p_count_hint)
				m_tokens.reserve(p_count_hint);
			T::size_type next = 0;
			while (true)
			{
				const T::size_type cur = str.find(tok, next);
				if (cur != T::npos)
				{
					const T l_value = str.substr(next, cur - next);
					//dcassert(!l_value.empty());
					//if (!l_value.empty())
					{
						m_tokens.push_back(l_value);
					}
					next = cur + tokLen;
				}
				else
				{
					if (next < str.size())
					{
						const T l_value = str.substr(next, str.size() - next);
						//dcassert(!l_value.empty());
						//if (!l_value.empty())
						{
							m_tokens.push_back(l_value);
						}
					}
					break;
				}
			}
		}
	public:
		StringTokenizer()
		{
		}
		explicit StringTokenizer(const T& str, const typename T::value_type tok, int p_count_hint)
		{
			slice(str, tok, 1, p_count_hint);
		}
		explicit StringTokenizer(const T& str, const typename T::value_type tok)
		{
			slice(str, tok, 1);
		}
		explicit StringTokenizer(const T& str, const typename T::value_type* tok, int p_count_hint = 0)
		{
			const T tmp(tok);
			slice(str, tmp, tmp.size(), p_count_hint);
		}
		const T2& getTokens() const
		{
			return m_tokens;
		}
		T2& getTokensForWrite()
		{
			return m_tokens;
		}
		bool is_contains(const T& p_str) const
		{
			for (auto i = m_tokens.cbegin(); i != m_tokens.cend(); ++i)
			{
				if (p_str == *i)
					return true;
			}
			return false;
		}
		bool is_find2(const T& p_str_1, const T& p_str_2) const
		{
			for (auto i = m_tokens.cbegin(); i != m_tokens.cend(); ++i)
			{
				if (p_str_1.find(*i) != T::npos || p_str_2.find(*i) != T::npos)
					return true;
			}
			return false;
		}
};

#endif // !DCPLUSPLUS_CLIENT_STRING_TOKENIZER_H

/**
 * @file
 * $Id: StringTokenizer.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
