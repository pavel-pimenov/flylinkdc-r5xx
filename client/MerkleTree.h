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


#ifndef DCPLUSPLUS_DCPP_MERKLE_TREE_H
#define DCPLUSPLUS_DCPP_MERKLE_TREE_H

#include "typedefs.h"
#include "HashValue.h"
#include "GPGPUManager.h"

/**
 * A class that represents a Merkle Tree hash. Storing
 * only the leaves of the tree, it is rather memory efficient,
 * but can still take a significant amount of memory during / after
 * hash generation.
 * The root hash produced can be used like any
 * other hash to verify the integrity of a whole file, while
 * the leaves provide checking of smaller parts of the file.
 */

const uint64_t MIN_BLOCK_SIZE = 65536;

template < class Hasher, const size_t baseBlockSize = 1024 >
class MerkleTree
#ifdef _DEBUG
//	, private boost::noncopyable TODO
#endif
{
	public:
		static const size_t BITS = Hasher::BITS;
		static const size_t BYTES = Hasher::BYTES;
		static const size_t BASE_BLOCK_SIZE = baseBlockSize;
		
		typedef HashValue<Hasher> MerkleValue;
		typedef std::vector<MerkleValue> MerkleList;
		
		MerkleTree() : fileSize(0), blockSize(BASE_BLOCK_SIZE) { }
		explicit MerkleTree(int64_t aBlockSize) : fileSize(0), blockSize(aBlockSize) { }
		
		/**
		 * Loads a set of leaf hashes, calculating the root
		 * @param data Pointer to (aFileSize + aBlockSize - 1) / aBlockSize) hash values,
		 *             stored consecutively left to right
		 */
		MerkleTree(int64_t p_FileSize, int64_t p_BlockSize, uint8_t* p_Data, size_t p_DataSize) :
			fileSize(p_FileSize), blockSize(p_BlockSize)
		{
			dcassert(p_BlockSize > 0);
			dcassert(p_FileSize >= p_BlockSize);
			const size_t l_n = calcBlocks(p_FileSize, p_BlockSize);
			dcassert(l_n >= 1);
			if (p_DataSize != l_n * Hasher::BYTES) // [!] IRainman fix: strongly check!
			{
				dcassert(0); // TODO: please refactoring MerkleTree to calculate three for this data.
				LogManager::message("MerkleTree create error with p_FileSize=" + Util::toString(p_FileSize) + ", p_BlockSize=" + Util::toString(p_BlockSize) + ", p_DataSize=" + Util::toString(p_DataSize));
				return; // http://www.flylinkdc.ru/2012/05/strongdc-sqlite-r9957.html
			}
			leaves.reserve(l_n);
			for (size_t i = 0; i < l_n; i++, p_Data += Hasher::BYTES)
			{
				leaves.push_back(MerkleValue(p_Data));
			}
			calcRoot();
		}
		
		/** Initialise a single root tree */
		MerkleTree(int64_t aFileSize, int64_t aBlockSize, const MerkleValue& aRoot) : root(aRoot), fileSize(aFileSize), blockSize(aBlockSize)
		{
			leaves.push_back(root);
		}
		
		~MerkleTree()
		{
		}
		
		static uint64_t calcBlockSize(const uint64_t aFileSize, const unsigned int maxLevels)
		{
			uint64_t tmp = BASE_BLOCK_SIZE;
			const uint64_t maxHashes = ((uint64_t)1) << (maxLevels - 1);
			while ((maxHashes * tmp) < aFileSize)
			{
				tmp *= 2;
			}
			return tmp;
		}
		static size_t calcBlocks(int64_t aFileSize, int64_t aBlockSize)
		{
			dcassert(aBlockSize > 0);
			dcassert(aFileSize >= aBlockSize);
			return std::max((size_t)((aFileSize + aBlockSize - 1) / aBlockSize), (size_t)1);
		}
		static uint16_t calcBlocks(int64_t aFileSize)
		{
			return (uint16_t)calcBlocks(aFileSize, calcBlockSize(aFileSize, 10));
		}
		//[+]PPA
		static uint64_t getMaxBlockSize(int64_t p_file_size)
		{
			const uint64_t l_result = std::max(calcBlockSize(p_file_size, 10), MIN_BLOCK_SIZE);
			dcassert(l_result >= MIN_BLOCK_SIZE); // [+] IRainman fix: check the negative values.
			return l_result;
		}
		
		uint64_t calcFullLeafCnt(uint64_t ttrBlockSize)
		{
			return ttrBlockSize / BASE_BLOCK_SIZE;
		}
		
		/**
		 * Update the merkle tree.
		 * @param len Length of data, must be a multiple of BASE_BLOCK_SIZE, unless it's
		 *            the last block.
		 */
		virtual void update(const void* data, size_t len)
		{
			uint8_t* buf = (uint8_t*)data;
			uint8_t zero = 0;
			size_t i = 0;
			
			// Skip empty data sets if we already added at least one of them...
			if (len == 0 && !(leaves.empty() && blocks.empty()))
				return;
				
			do
			{
				size_t n = std::min(size_t(BASE_BLOCK_SIZE), len - i);
				Hasher h;
				h.update(&zero, 1);
				h.update(buf + i, n); // n=1024 [4] https://www.box.net/shared/248a073eee69128a3c7b
				if ((int64_t)BASE_BLOCK_SIZE < blockSize)
				{
					blocks.push_back(MerkleBlock(MerkleValue(h.finalize()), BASE_BLOCK_SIZE));
					reduceBlocks();
				}
				else
				{
					leaves.push_back(MerkleValue(h.finalize()));
				}
				i += n;
			}
			while (i < len);
			fileSize += len;
		}
		
		uint8_t* finalize()
		{
			// No updates yet, make sure we have at least one leaf for 0-length files...
			if (leaves.empty() && blocks.empty())
			{
				update(0, 0);
			}
			// [!] IRainman opt.
#ifdef _DEBUG
//# define TEST_NEW_FINALIZE_MERKLE_THREE
#endif
#ifdef TEST_NEW_FINALIZE_MERKLE_THREE
			// old algorithm, only for test.
			auto blocks_test = blocks;
			while (blocks_test.size() > 1)
			{
				MerkleBlock& a = blocks_test[blocks_test.size() - 2];
				MerkleBlock& b = blocks_test[blocks_test.size() - 1];
				a.first = combine(a.first, b.first);
				blocks_test.pop_back();
			}
#endif // TEST_NEW_FINALIZE_MERKLE_THREE
			// new algorithm
			if (blocks.size() > 1)
			{
				auto size = blocks.size();
				while (true)
				{
					MerkleBlock& a = blocks[size - 2];
					MerkleBlock& b = blocks[size - 1];
					a.first = combine(a.first, b.first);
					if (size == 2)
					{
						blocks.resize(1);
						break;
					}
					--size;
				}
			}
#ifdef TEST_NEW_FINALIZE_MERKLE_THREE
			dcassert(blocks.size() == blocks_test.size() && memcmp(blocks.data(), blocks_test.data(), blocks.size()) == 0);
#endif
			// [~] IRainman opt.
			
			dcassert(blocks.empty() || blocks.size() == 1);
			if (!blocks.empty())
			{
				leaves.push_back(blocks[0].first);
			}
			calcRoot();
			return root.data;
		}
		
		MerkleValue& getRoot()
		{
			return root;
		}
		const MerkleValue& getRoot() const
		{
			return root;
		}
		MerkleList& getLeaves()
		{
			return leaves;
		}
		const MerkleList& getLeaves() const
		{
			return leaves;
		}
		
		int64_t getBlockSize() const
		{
			return blockSize;
		}
		void setBlockSize(int64_t aSize)
		{
			blockSize = aSize;
		}
		
		int64_t getFileSize() const
		{
			return fileSize;
		}
		void setFileSize(int64_t aSize)
		{
			fileSize = aSize;
		}
		
		bool verifyRoot(const uint8_t* aRoot)
		{
			return memcmp(aRoot, getRoot().data(), BYTES) == 0;
		}
		
		void calcRoot()
		{
			root = getHash(0, fileSize);
		}
		
		void getLeafData(ByteVector& buf)
		{
			const auto l_size = getLeaves().size();
			if (l_size > 0)
			{
				buf.resize(l_size * BYTES);
				auto p = &buf[0];
				for (size_t i = 0; i < l_size; ++i, p += BYTES)
				{
					memcpy(p, &getLeaves()[i], BYTES);
				}
				dcassert(static_cast<ptrdiff_t>(buf.size()) == p - &buf[0]);
				return;
			}
			buf.clear();
		}
		
	protected:
		typedef std::pair<MerkleValue, int64_t> MerkleBlock;
		typedef std::vector<MerkleBlock> MBList;
		
		MBList blocks;
		
		MerkleList leaves;
		
		/** Total size of hashed data */
		int64_t fileSize;
		/** Final block size */
		int64_t blockSize;
		
	private:
		MerkleValue root;
		
		MerkleValue getHash(int64_t start, int64_t length)
		{
			dcassert(start + length <= fileSize);
			dcassert((start % blockSize) == 0);
			if (length <= blockSize)
			{
				start /= blockSize;
				dcassert(start < (int64_t)leaves.size());
				if (start < static_cast<int64_t>(leaves.size())) //[+]PPA
					return leaves[static_cast<size_t>(start)];
				else
					return MerkleValue(); // TODO - raise exception
			}
			else
			{
				int64_t l = blockSize;
				while (true)
				{
					const int64_t l2 = l * 2;
					if (l2 < length)
						l = l2;
					else
						break;
				}
				return combine(getHash(start, l), getHash(start + l, length - l));
			}
		}
		
		MerkleValue combine(const MerkleValue& a, const MerkleValue& b)
		{
			uint8_t one = 1;
			Hasher h;
			h.update(&one, 1);
			h.update(a.data, MerkleValue::BYTES);
			h.update(b.data, MerkleValue::BYTES);
			return MerkleValue(h.finalize());
		}
		
	protected:
		void reduceBlocks()
		{
			// [!] IRainman opt.
#ifdef _DEBUG
//# define TEST_NEW_REDUCE_BLOCKS_MERKLE_THREE
#endif
#ifdef TEST_NEW_REDUCE_BLOCKS_MERKLE_THREE
			auto blocks_test = blocks;
			auto leaves_test = leaves;
			while (blocks_test.size() > 1)
			{
				MerkleBlock& a = blocks_test[blocks_test.size() - 2];
				MerkleBlock& b = blocks_test[blocks_test.size() - 1];
				if (a.second == b.second)
				{
					if (a.second * 2 == blockSize)
					{
						leaves_test.push_back(combine(a.first, b.first));
						blocks_test.pop_back();
						blocks_test.pop_back();
					}
					else
					{
						a.second *= 2;
						a.first = combine(a.first, b.first);
						blocks_test.pop_back();
					}
				}
				else
				{
					break;
				}
			}
#endif // TEST_NEW_REDUCE_BLOCKS_MERKLE_THREE
			if (blocks.size() > 1)
			{
				auto size = blocks.size();
				while (true)
				{
					MerkleBlock& a = blocks[size - 2];
					MerkleBlock& b = blocks[size - 1];
					if (a.second == b.second)
					{
						if (a.second * 2 == blockSize)
						{
							leaves.push_back(combine(a.first, b.first));
							size -= 2;
						}
						else
						{
							a.second *= 2;
							a.first = combine(a.first, b.first);
							--size;
						}
					}
					else
					{
						blocks.resize(size);
						break;
					}
					if (size < 2)
					{
						blocks.resize(size);
						break;
					}
				}
			}
#ifdef TEST_NEW_REDUCE_BLOCKS_MERKLE_THREE
			dcassert(blocks.size() == blocks_test.size() && memcmp(blocks.data(), blocks_test.data(), blocks.size()) == 0);
			dcassert(leaves.size() == leaves_test.size() && memcmp(leaves.data(), leaves_test.data(), leaves.size()) == 0);
#endif
			// [~] IRainman opt.
		}
};
#ifdef FLYLINKDC_USE_GPU_TTH

template < class Hasher, const size_t baseBlockSize = 1024 >
class MerkleTreeGPU : public MerkleTree<Hasher, baseBlockSize> { };

/*
 * Realized only for Hasher == TigerHash.
 * If you need more, please realize ;)
 */
template <class Hasher>
class MerkleTreeGPU<Hasher> : public MerkleTree<Hasher>
{
	public:
		MerkleTreeGPU() : MerkleTree() {}
		MerkleTreeGPU(int64_t aBlockSize) : MerkleTree(aBlockSize) { }
		MerkleTreeGPU(int64_t p_FileSize, int64_t p_BlockSize, uint8_t* p_Data, size_t p_DataSize)
			: MerkleTree(p_FileSize, p_BlockSize, p_Data, p_DataSize) { }
		MerkleTreeGPU(int64_t aFileSize, int64_t aBlockSize, const MerkleValue& aRoot)
			: MerkleTree(aFileSize, aBlockSize, aRoot) { }
			
		void update(const void* data, size_t len);
		
	private:
		bool hash_blocks(uint8_t *buf, uint64_t bc, uint64_t lvbsz, uint64_t last_bs)
		{
			uint64_t lvlfc;
			uint8_t res[Hasher::BYTES];
			
			bool b_res = true;
			
			if (!bc) return b_res;
			
			lvlfc = calcFullLeafCnt(lvbsz);
			
			if (bc < lvlfc)
			{
				return hash_blocks(buf, bc, lvbsz >> 1, last_bs);
			}
			
			b_res = GPGPUTTHManager::getInstance()->get()->krn_ttr(buf, lvlfc, (bc == lvlfc ? last_bs : BASE_BLOCK_SIZE), (uint64_t *)res);
			
			// Data doesn't been processed on GPU
			if (!b_res) return b_res;
			
			if (lvbsz < (uint64_t)blockSize)
			{
				blocks.push_back(MerkleBlock(MerkleValue(res), lvbsz));
				reduceBlocks();
			}
			else
			{
				leaves.push_back(MerkleValue(res));
			}
			
			return hash_blocks(buf + lvlfc * BASE_BLOCK_SIZE, bc - lvlfc, lvbsz, last_bs);
		}
};

typedef MerkleTreeGPU<TigerHash> TigerTree;
#else
typedef MerkleTree<TigerHash> TigerTree;
#endif // FLYLINKDC_USE_GPU_TTH

typedef TigerTree::MerkleValue TTHValue;

struct TTFilter
{
		TTFilter() : tt(1024 * 1024 * 1024) { }
		void operator()(const void* data, size_t len)
		{
			tt.update(data, len);
		}
		TigerTree& getTree()
		{
			return tt;
		}
	private:
		TigerTree tt;
};

#endif // DCPLUSPLUS_DCPP_MERKLE_TREE_H

/**
 * @file
 * $Id: MerkleTree.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
