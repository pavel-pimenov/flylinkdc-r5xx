/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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


#ifndef DCPLUSPLUS_DCPP_UTIL_FLYLINKDC_H
#define DCPLUSPLUS_DCPP_UTIL_FLYLINKDC_H

#include "compiler.h"

#ifdef _DEBUG

#include <sstream>

template <class T> class CFlyTestStatistic
{
	public:
		unsigned __int64 m_max_count;
		unsigned __int64 m_count;
		unsigned __int64 m_count_contruct;
		unsigned __int64 m_size_alloc;
		unsigned __int64 m_size_alloc_array;
		unsigned __int64 m_count_alloc;
		unsigned __int64 m_count_dealloc;
		unsigned __int64 m_count_alloc_array;
		unsigned __int64 m_count_dealloc_array;
	private:
		void print()
		{
			std::stringstream l_ss;
			l_ss << "[" << m_count_contruct << "]\t| alloc/dealloc = " << m_count_alloc << "/" << m_count_dealloc
			     << "\t| T = " << typeid(T).name()
			     << "\t| size_alloc / array = " << m_size_alloc / 1024 << " Kb / " << m_size_alloc_array / 1024 << " Kb"
			     << "\t| sizeof(T) = " << sizeof(T)
			     << "\t| max_count = " << m_max_count
			     << "\t| alloc/dealloc array = " << m_count_alloc_array << "/" << m_count_dealloc_array
			     << "\t| max_count * sizeof(T) kb = " << (m_max_count * sizeof(T)) / 1024 << std::endl;
#ifdef _CONSOLE
			std::cout << l_ss;
#else
			std::FILE* l_F = std::fopen("CFlyTestStatistic-trace.txt", "a+t");
			if (l_F)
			{
				std::fwrite(l_ss.str().c_str(), l_ss.str().length(), 1, l_F);
				std::fclose(l_F);
			}
#endif
		}
	public:
		CFlyTestStatistic() :
			m_max_count(0),
			m_count(0),
			m_count_contruct(0),
			m_size_alloc(0),
			m_size_alloc_array(0),
			m_count_alloc(0),
			m_count_dealloc(0),
			m_count_dealloc_array(0),
			m_count_alloc_array(0)
		{
		}
		~CFlyTestStatistic()
		{
			print();
		}
		CFlyTestStatistic& operator++()
		{
			++m_count_contruct;
			if (++m_count > m_max_count)
				m_max_count = m_count;
			return *this;
		}
		CFlyTestStatistic& operator--()
		{
			m_count--;
			return *this;
		}
		void addAlloc(unsigned __int64 p_alloc)
		{
			++m_count_alloc;
			m_size_alloc += p_alloc;
		}
};
#endif // _DEBUG

class CFlyBusyBool
{
		bool& m_flag;
	public:
		explicit CFlyBusyBool(bool& p_flag) : m_flag(p_flag)
		{
			m_flag = true;
		}
		~CFlyBusyBool()
		{
			m_flag = false;
		}
};

class CFlyBusy
{
		int& m_count;
	public:
		explicit CFlyBusy(int& p_count) : m_count(p_count)
		{
			//dcassert(m_count >= 0);
			++m_count;
		}
		~CFlyBusy()
		{
			--m_count;
		}
};
template <class T> inline void clear_and_reset_capacity(T& p)
{
	T l_tmp;
	p.swap(l_tmp);
}

template <class T> inline void safe_delete(T* & p)
{
	if (p != nullptr)
	{
		delete p;
		p = nullptr;
	}
}
template <class T> inline void safe_delete_array(T* & p)
{
	if (p != nullptr)
	{
		delete [] p;
		p = nullptr;
	}
}
template <class T> inline void safe_release(T* & p)
{
	if (p != nullptr)
	{
		p->Release();
		p = nullptr;
	}
}
template <class T> inline void DestroyAndDetachWindow(T & p)
{
	if (p != nullptr)
	{
		p.DestroyWindow();
		p.Detach();
	}
}
struct DeleteFunction
{
    template<typename T>
    void operator()(const T& p) const
    {
        delete p;
    }
};

#endif // DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H
