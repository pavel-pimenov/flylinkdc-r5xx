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


#ifndef DCPLUSPLUS_DCPP_POINTER_H
#define DCPLUSPLUS_DCPP_POINTER_H

#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/detail/atomic_count.hpp>

template<typename T>
class intrusive_ptr_base
{
	public:
		void inc() noexcept
		{
			intrusive_ptr_add_ref(this);
		}
		
		void dec() noexcept
		{
			intrusive_ptr_release(this);
		}
		
		bool unique(int p_val = 1) const noexcept
		{
			return m_ref <= p_val;
		}
		
		int getRefs() const noexcept
		{
			return m_ref;
		}
		
	protected:
		intrusive_ptr_base() noexcept :
			m_ref(0) { }
		virtual ~intrusive_ptr_base() { }
		
	private:
		friend void intrusive_ptr_add_ref(intrusive_ptr_base* p)
		{
			++p->m_ref;
		}
		friend void intrusive_ptr_release(intrusive_ptr_base* p)
		{
			if (--p->m_ref == 0)
			{
				delete static_cast<T*>(p);
			}
		}
		
		boost::detail::atomic_count m_ref;
};

#endif // !defined(POINTER_H)

/**
 * @file
 * $Id: Pointer.h 568 2011-07-24 18:28:43Z bigmuscle $
 */