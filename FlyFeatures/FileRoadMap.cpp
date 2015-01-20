/*
 * Copyright (C) 2011-2015 FlylinkDC++ Team http://flylinkdc.com/
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
#include "FileRoadMap.h"
#include "../client/LogManager.h"

#ifdef SSA_VIDEO_PREVIEW_FEATURE

void FileRoadMap::AddSegment(int64_t pos, int64_t size)
{
// LogManager::message("[FileRoadMap::AddSegment] pos = " + Util::toString(pos) + " size = " + Util::toString(size));
	if (size == 0)
		return;
	// SSA calculate new pos and size
	int64_t newPos = pos;
	int64_t newSize = size;
	const auto l_is_insertable = GetInsertableSizeAndPos(newPos, newSize);
//	LogManager::message("[FileRoadMap::AddSegment] pos = " + Util::toString(pos) + " size = " + Util::toString(size) +
//										" newPos = " + Util::toString(newPos) + " newSize = " + Util::toString(newSize));
	if(l_is_insertable)
	{
			return;
	}
	const FileRoadMapItem item(newPos, newSize);
	FastLock l(cs);
	const auto l_res = m_map.insert(item);
	dcassert(l_res.second == true);
	//LogManager::message("[FileRoadMap::AddSegment] newPos = " + Util::toString(newPos) + " newSize = "  + 
	//	                                Util::toString(newSize) + " l_res.second = " + Util::toString(l_res.second));
}

bool FileRoadMap::IsAvaiable(int64_t pos, int64_t size)
{
	if (size == 0)
		return true;
	int64_t newPos = pos;
	int64_t newSize = size;
	const auto l_res =  GetInsertableSizeAndPos(newPos, newSize);
//	LogManager::message("[FileRoadMap::IsAvaiable] pos = " + Util::toString(pos) + " size = " + Util::toString(size) +
//										" newPos = " + Util::toString(newPos) + " newSize = " + Util::toString(newSize));
	return l_res;
}

bool FileRoadMap::GetInsertableSizeAndPos(int64_t& pos, int64_t& size)
{

	//LogManager::message("[FileRoadMap::GetInsertableSizeAndPos] pos = " + Util::toString(pos) + " size = " + Util::toString(size));

	if (pos + size > m_totalSize)
		size = m_totalSize - pos;
		
	FastLock l(cs);
	for (auto item = m_map.cbegin(); item != m_map.cend(); ++item)
	{
		const auto& itemR = (*item);
		if (itemR.getPosition() <= pos && itemR.getLastPosition() > pos)
		{
			size -= itemR.getLastPosition() - pos;
			pos   = itemR.getLastPosition();
			//LogManager::message("[FileRoadMap::GetInsertableSizeAndPos][1] pos = " + Util::toString(pos) + " size = " + Util::toString(size));
			if (size <= 0)
				break;
		}
		else if (itemR.getPosition() > pos && itemR.getLastPosition() <= pos + size)
		{
			size = itemR.getLastPosition() - pos;
			//LogManager::message("[FileRoadMap::GetInsertableSizeAndPos][2] pos = " + Util::toString(pos) + " size = " + Util::toString(size));
			if (size <= 0)
				break;
		}
	}
	//LogManager::message("[FileRoadMap::GetInsertableSizeAndPos][exit] pos = " + Util::toString(pos) + " size = " + Util::toString(size));
	return size <= 0;
}

#endif // SSA_VIDEO_PREVIEW_FEATURE