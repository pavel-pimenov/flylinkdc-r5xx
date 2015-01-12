/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_CLIENT_STRING_TOKENIZER_H
#define DCPLUSPLUS_CLIENT_STRING_TOKENIZER_H

template<class T, class T2 = StringList>
class StringTokenizer
{
	private:
		T2 m_tokens;
		
		template<class T3>
		void slice(const T& str, const T3& tok, const size_t tokLen) // [+] IRainman copy-past fix.
		{
			T::size_type next = 0;
			while (true)
			{
				const T::size_type cur = str.find(tok, next);
				if (cur != T::npos)
				{
					m_tokens.push_back(str.substr(next, cur - next));
					next = cur + tokLen;
				}
				else
				{
					if (next < str.size())
					{
						m_tokens.push_back(str.substr(next, str.size() - next)); // 2012-05-03_22-05-14_YNJS7AEGAWCUMRBY2HTUTLYENU4OS2PKNJXT6ZY_F4B220A1_crash-stack-r502-beta24-x64-build-9900.dmp
					}
					break;
				}
			}
		}
	public:
		StringTokenizer()
		{
		}
		
		explicit StringTokenizer(const T& str, const typename T::value_type tok) // [!] IRainman fix: no needs link to T::value_type.
		{
			slice(str, tok, 1);
		}
		
		explicit StringTokenizer(const T& str, const typename T::value_type* tok)
		{
			const T tmp(tok);
			slice(str, tmp, tmp.size());
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
