/*
 * Copyright (C) 2011-2012 FlylinkDC++ Team http://flylinkdc.com/
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

#if !defined(_FILE_ROAD_MAP_H_)
#define _FILE_ROAD_MAP_H_

#pragma once

#include <set>
#include "../client/Util.h"
#ifdef SSA_VIDEO_PREVIEW_FEATURE
#include "../client/CFlyThread.h"


class FileRoadMapItem
{
	public:
		FileRoadMapItem(int64_t pos_, int64_t size_) : pos(pos_), size(size_) {}
		const int64_t getLastPosition()
		{
			return pos + size;
		}
		
		friend bool operator < (const FileRoadMapItem &left, const FileRoadMapItem &right)
		{
			return left.getPosition() < right.getPosition();
		}
		
		GETSET(int64_t, pos, Position);
		GETSET(int64_t, size, Size);
};

typedef std::set<FileRoadMapItem> MapItems;
//typedef MapItems::const_iterator MapItemsCIter;
//typedef MapItems::iterator MapItemsIter;

typedef std::vector<FileRoadMapItem> MapVItems;

class FileRoadMap
{

	public:
		FileRoadMap(int64_t totalSize) : m_totalSize(totalSize)
		{
			// [-] m_map.clear(); [-] IRainman fix.
		}
		~FileRoadMap()
		{
			// [-] FastLock l(cs); [-] IRainman fix.
			// [-] m_map.clear(); [-] IRainman fix.
		}
		void AddSegment(int64_t pos, int64_t size);
		
		bool IsAvaiable(int64_t pos, int64_t size);
		bool GetInsertableSizeAndPos(int64_t& pos, int64_t& size);
		
	private:
		FastCriticalSection cs; // [!] IRainman opt: use spinlock here.
		int64_t m_totalSize;
		MapItems m_map;
};

#endif // SSA_VIDEO_PREVIEW_FEATURE

#endif // _FILE_ROAD_MAP_H_