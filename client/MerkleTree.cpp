/*
 * Copyright (C) 2015 ecl1pse
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

#ifdef FLYLINKDC_USE_GPU_TTH

#include "MerkleTree.h"
#include "SettingsManager.h"

template <class Hasher>
void MerkleTreeGPU<Hasher>::update(const void* data, size_t len)
{
	uint8_t *buf;
	uint64_t lvbsz, last_bs;
	uint64_t bc, rbc;
	
	uint64_t lvlfc;
	uint64_t hbc;
	
	bool b_res = true;
	
	bool b_use_cpu = !BOOLSETTING(USE_GPU_IN_TTH_COMPUTING) ||
		GPGPUTTHManager::getInstance()->get()->get_status() != GPGPUPlatform::GPGPU_OK;
	
	if (b_use_cpu)
	{
		MerkleTree::update(data, len);
		return;
	}
	
	// Skip empty data sets if we already added at least one of them...
	if (len == 0 && !(leaves.empty() && blocks.empty()))
		return;
		
	buf = (uint8_t *)data;
	bc = (len ? (len - 1 + BASE_BLOCK_SIZE) / BASE_BLOCK_SIZE : 1);
	
	last_bs = (len ? (BASE_BLOCK_SIZE - 1 + len % BASE_BLOCK_SIZE) % BASE_BLOCK_SIZE + 1 : 0);
	
	rbc = bc;
	
	do
	{
		if (blocks.empty())
		{
			lvbsz = blockSize;
			
			hbc = rbc;
		}
		else
		{
			auto lb = blocks.back();
			lvbsz = lb.second;
			
			lvlfc = calcFullLeafCnt(lvbsz);
			hbc = (rbc < lvlfc ? rbc : lvlfc);
		}
		
		b_res = hash_blocks(buf, hbc, lvbsz, last_bs);
		
		// If error, dispatch update on CPU
		if (!b_res)
		{
			MerkleTree::update(data, len);
			return;
		}
		
		rbc -= hbc;
		buf += hbc * BASE_BLOCK_SIZE;
	}
	while (rbc);
	
	fileSize += (int64_t)len;
}

template void MerkleTreeGPU<TigerHash>::update(const void*, size_t);

#endif // FLYLINKDC_USE_GPU_TTH

