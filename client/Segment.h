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


#ifndef DCPLUSPLUS_DCPP_SEGMENT_H_
#define DCPLUSPLUS_DCPP_SEGMENT_H_

// minimum file size to be PFS : 20M
#define PARTIAL_SHARE_MIN_SIZE 20971520

#include "Util.h"

class Segment
#ifdef _DEBUG
	// : boost::noncopyable
#endif
{
	public:
		Segment() : start(0), size(-1), overlapped(false) { }
		Segment(int64_t start_, int64_t size_, bool overlapped_ = false) : start(start_), size(size_), overlapped(overlapped_)
		{
			//dcassert(start_ >= 0);
		}
		
		int64_t getStart() const
		{
			// dcassert(start >= 0);
			return start;
		}
		int64_t getSize() const
		{
			return size;
		}
		int64_t getEnd() const
		{
			//dcassert(start >= 0);
			return start + size;
		}
		
		void setSize(int64_t size_)
		{
			size = size_;
		}
		
		bool overlaps(const Segment& rhs) const
		{
			return getStart() < rhs.getEnd() && rhs.getStart() < getEnd();
		}
#if 0
		void trim_segment(const Segment& rhs)
		{
			if (!overlaps(rhs))
			{
				return;
			}
			
			if (rhs.getStart() < start)
			{
				int64_t rend = rhs.getEnd();
				if (rend > getEnd())
				{
					start = size = 0;
				}
				else
				{
					size -= rend - start;
					start = rend;
				}
				dcassert(start >= 0);
				return;
			}
			dcassert(start >= 0);
			size = rhs.getStart() - start;
		}
		
#endif
		bool operator<(const Segment& rhs) const
		{
			return (getStart() < rhs.getStart()) || (getStart() == rhs.getStart() && getSize() < rhs.getSize());
		}
		//bool operator==(const Segment& rhs) const
		//{
		//  return getStart() == rhs.getStart() && getSize() == rhs.getSize();
		//}
		bool isSizeEqu(const int64_t& p_size) const
		{
			return getSize() == p_size && getStart() == 0;
		}
		bool contains(const Segment& rhs) const
		{
			return getStart() <= rhs.getStart() && getEnd() == rhs.getEnd();
		}
	private:
		int64_t start;
		int64_t size;
		
		GETSET(bool, overlapped, Overlapped);
};

#endif /*SEGMENT_H_*/
