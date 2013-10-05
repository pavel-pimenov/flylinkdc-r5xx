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

#include "stdinc.h"
#include "FileRoadMap.h"

#ifdef SSA_VIDEO_PREVIEW_FEATURE

void FileRoadMap::AddSegment(int64_t pos, int64_t size)
{
	if (size == 0)
		return;
	// SSA calculate new pos and size
	int64_t newPos = pos;
	int64_t newSize = size;
	if (GetInsertableSizeAndPos(newPos, newSize))
		return;
	FileRoadMapItem item(newPos, newSize);
	FastLock l(cs);
	m_map.insert(item);
}

bool FileRoadMap::IsAvaiable(int64_t pos, int64_t size)
{
	if (size == 0)
		return true;
	int64_t newPos = pos;
	int64_t newSize = size;
	return GetInsertableSizeAndPos(newPos, newSize);
}

bool FileRoadMap::GetInsertableSizeAndPos(int64_t& pos, int64_t& size)
{

	if (pos + size > m_totalSize)
		size = m_totalSize - pos;
		
	FastLock l(cs);
	for (auto item = m_map.cbegin(); item != m_map.cend(); ++item)
	{
		FileRoadMapItem itemR = (*item);
		if ((itemR.getPosition() <= pos) &&
		        (itemR.getLastPosition() > pos))
		{
			size -= (itemR.getLastPosition() - pos);
			pos = itemR.getLastPosition();
			if (size <= 0)
				break;
		}
		else if ((itemR.getPosition() > pos) &&
		         (itemR.getLastPosition() <= pos + size))
		{
			size = itemR.getLastPosition() - pos;
			if (size <= 0)
				break;
		}
	}
	
	return size <= 0;
}

#endif // SSA_VIDEO_PREVIEW_FEATURE