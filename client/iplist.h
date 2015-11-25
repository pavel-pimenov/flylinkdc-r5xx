/*
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
 *
 * IPList
 * Idea and C# code by Bo Norgaard
 * C++ code by SSA
 *
 */

#ifndef IPLIST_H
#define IPLIST_H

#ifdef PPA_INCLUDE_IPFILTER

#include <vector>
#include <map>
#include "CFlyThread.h"

class CFlyLog;

class IPList
{
		// TODO: needs review :(
		// Please optimize my searching faster than brute force search!
	private:
		class IPArrayList
		{
			private:
				std::vector< uint32_t > m_ipNumList;
				uint32_t m_ipmask;
				
			public:
				explicit IPArrayList(uint32_t p_mask)
					: m_ipmask(p_mask)
				{
				}
				
				void add(uint32_t p_IPNum)
				{
					m_ipNumList.push_back(p_IPNum & m_ipmask);
					std::sort(m_ipNumList.begin(), m_ipNumList.end());
				}
				
				bool check(uint32_t p_IPNum)
				{
					bool found;
					if (!m_ipNumList.empty())
					{
						p_IPNum = p_IPNum & m_ipmask;
						//found = ipNumList.BinarySearch(IPNum) >= 0;
						found = std::find(m_ipNumList.begin(), m_ipNumList.end(), p_IPNum) != m_ipNumList.end(); // TODO: Please optimize my searching faster than brute force search!
					}
					else
					{
						found = false;
					}
					return found;
				}
				
				void clear()
				{
					m_ipNumList.clear();
				}
				
				uint32_t getMask()
				{
					return m_ipmask;
				}
		};
		
		enum IP_ERROR_STATE
		{
			NO_IP_ERROR = 0,
			IP_ERROR,
			MASK_ERROR,
			START_IP_ERROR,
			END_IP_ERROR,
			START_GREATE_THEN_END_RANGE_ERROR,
			START_AND_END_EQUAL,
			LAST
		};
		
		std::vector<IPArrayList> m_ipRangeList;
		std::map<uint32_t, uint32_t> m_maskList;
		std::vector< uint32_t > m_usedList;
		FastCriticalSection m_cs; // [!] IRainman opt: use spin lock here.
		
		static uint32_t parseIP(const std::string& IPNumber);
		uint32_t getMaskByLevel(uint32_t maskLevel);
		uint32_t add(const std::string& IPNumber);
		void add(uint32_t ip);
		
		uint32_t add(const std::string& IPNumber, const std::string& Mask);
		void add(uint32_t ip, uint32_t umask);
		uint32_t add(const std::string& IPNumber, uint32_t maskLevel);
		
		uint32_t addRange(uint32_t fromIP, uint32_t toIP);
		uint32_t addRange(const std::string& fromIP, const std::string& toIP);
		void addRangeListAndSort(uint32_t p_ip, uint32_t p_level)
		{
			CFlyFastLock(m_cs);
			m_ipRangeList[p_level].add(p_ip);
			if (std::find(m_usedList.begin(), m_usedList.end(), p_level) == m_usedList.end())
			{
				m_usedList.push_back(p_level);
				std::sort(m_usedList.begin(), m_usedList.end());
			}
		}
		
		
		string translateIPError(int32_t& p_errorCode);
		
	public:
		IPList();
		~IPList();
		bool empty()
		{
			CFlyFastLock(m_cs);
			return m_usedList.empty();
		}
		void addLine(std::string& Line, CFlyLog& p_log);
		void addData(const std::string& Data, CFlyLog& p_log);
		
		bool checkIp(uint32_t ip);
		
		void clear();
};

#endif // PPA_INCLUDE_IPFILTER

#endif // IPLIST_H
