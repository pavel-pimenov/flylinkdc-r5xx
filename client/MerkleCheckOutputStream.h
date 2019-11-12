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


#ifndef DCPLUSPLUS_DCPP_MERKLE_CHECK_OUTPUT_STREAM_H
#define DCPLUSPLUS_DCPP_MERKLE_CHECK_OUTPUT_STREAM_H

#include "Streams.h"

template<class TreeType, bool managed>
class MerkleCheckOutputStream : public OutputStream
{
	public:
		MerkleCheckOutputStream(const TreeType& aTree, OutputStream* aStream, int64_t start) : s(aStream), real(aTree), cur(aTree.getBlockSize()), verified(0), bufPos(0)
		{
			//dcdebug("[==========================] MerkleCheckOutputStream() start = %d s = %d this = %d\r\n\r\n", start, s, this);
			// Only start at block boundaries
			const auto l_blocksize = aTree.getBlockSize();
			dcassert(start % l_blocksize == 0);
			cur.setFileSize(start);
			
			const size_t nBlocks = static_cast<size_t>(start / l_blocksize);
			if (nBlocks > aTree.getLeaves().size())
			{
				dcdebug("Invalid tree / parameters");
				return;
			}
			cur.getLeaves().insert(cur.getLeaves().begin(), aTree.getLeaves().begin(), aTree.getLeaves().begin() + nBlocks);
		}
		
		~MerkleCheckOutputStream()
		{
			//dcdebug("[==========================] ~MerkleCheckOutputStream() bufPos = %d s = %d this = %d\r\n\r\n", bufPos, s,  this);
			if (managed)
			{
				delete s;
			}
		}
		
		size_t flushBuffers(bool aForce) override
		{
			if (bufPos != 0)
				cur.update(buf, bufPos);
			bufPos = 0;
			
			cur.finalize();
			if (cur.getLeaves().size() == real.getLeaves().size())
			{
				if (cur.getRoot() != real.getRoot())
					throw FileException(STRING(TTH_INCONSISTENCY));
			}
			else
			{
				checkTrees();
			}
			return s->flushBuffers(aForce);
		}
		
		void commitBytes(const void* b, size_t len)
		{
			uint8_t* xb = (uint8_t*)b;
			size_t pos = 0;
			
			if (bufPos != 0)
			{
				size_t bytes = std::min(TreeType::BASE_BLOCK_SIZE - bufPos, len);
				memcpy(buf + bufPos, xb, bytes);
				pos = bytes;
				bufPos += bytes;
				
				if (bufPos == TreeType::BASE_BLOCK_SIZE)
				{
					cur.update(buf, TreeType::BASE_BLOCK_SIZE);
					bufPos = 0;
				}
			}
			
			if (pos < len)
			{
				dcassert(bufPos == 0);
				size_t left = len - pos;
				size_t part = left - (left %  TreeType::BASE_BLOCK_SIZE);
				if (part > 0)
				{
					cur.update(xb + pos, part);
					pos += part;
				}
				left = len - pos;
				memcpy(buf, xb + pos, left);
				bufPos = left;
			}
		}
		
		size_t write(const void* b, size_t len)
		{
			commitBytes(b, len);
			checkTrees();
			return s->write(b, len);
		}
		
		int64_t verifiedBytes() const
		{
			return min(real.getFileSize(), (int64_t)(cur.getBlockSize() * cur.getLeaves().size()));
		}
	private:
		OutputStream* s;
		TreeType real;
		TreeType cur;
		size_t verified;
		
		uint8_t buf[TreeType::BASE_BLOCK_SIZE];
		size_t bufPos;
		
		void checkTrees()
		{
			while (cur.getLeaves().size() > verified)
			{
				if (cur.getLeaves().size() > real.getLeaves().size() ||
				        !(cur.getLeaves()[verified] == real.getLeaves()[verified]))
				{
					throw FileException(STRING(TTH_INCONSISTENCY));
				}
				verified++;
			}
		}
};

#endif // !defined(MERKLE_CHECK_OUTPUT_STREAM_H)

/**
 * @file
 * $Id: MerkleCheckOutputStream.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
