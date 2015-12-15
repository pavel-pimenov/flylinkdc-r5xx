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
 *
 * IPList
 * Idea and C# code by Bo Norgaard
 * C++ code by SSA
 *
 */

#include "stdinc.h"

#ifdef PPA_INCLUDE_IPFILTER
#include "socket.h"
#include <boost/algorithm/string.hpp>
#include "iplist.h"
#include "Util.h"
#include "ResourceManager.h"
#include "LogManager.h"
#include "ClientManager.h"

IPList::IPList()
{
	// Initialize IP mask list and create IPArrayList into the ipRangeList
	uint32_t l_mask = 0x00000000;
	for (uint32_t level = 1; level < 33; ++level)
	{
		l_mask = (l_mask >> 1) | 0x80000000;
		m_maskList[ l_mask ] =  level ;
		m_ipRangeList.push_back(IPList::IPArrayList(l_mask));
	}
}

IPList::~IPList()
{
}


uint32_t IPList::add(const std::string& IPNumber)
{
	const uint32_t ip = Socket::convertIP4(IPNumber);
	if (ip == INADDR_NONE || ip == 0)
		return IP_ERROR;
	add(ip);
	return NO_IP_ERROR;
}

void IPList::add(uint32_t ip)
{
	if (ip == INADDR_NONE || ip == 0)
		return;
	addRangeListAndSort(ip, 31);
}

uint32_t IPList::add(const std::string& IPNumber, const std::string& Mask)
{
	const uint32_t ip = Socket::convertIP4(IPNumber);
	const uint32_t umask = Socket::convertIP4(Mask);
	if (ip == INADDR_NONE || ip == 0)
		return IP_ERROR;
	if (umask == INADDR_NONE || umask == 0)
		return MASK_ERROR;
	add(ip, umask);
	return NO_IP_ERROR;
}

void IPList::add(uint32_t ip, uint32_t umask)
{
	if (ip == INADDR_NONE && umask == INADDR_NONE)
		return;
		
	const auto it = m_maskList.find(umask);
	if (it != m_maskList.end())
	{
		const  uint32_t Level = it->second;
		if (Level)
		{
			ip = ip & umask;
			addRangeListAndSort(ip, Level - 1);
		}
	}
}

uint32_t IPList::add(const std::string& IPNumber, uint32_t maskLevel)
{
	const uint32_t ip = Socket::convertIP4(IPNumber);
	const uint32_t umask = getMaskByLevel(maskLevel);
	
	if (ip == INADDR_NONE || ip == 0)
		return IP_ERROR;
	if (umask == INADDR_NONE || umask == 0)
		return MASK_ERROR;
	add(ip, umask);
	return NO_IP_ERROR;
}

uint32_t IPList::getMaskByLevel(uint32_t maskLevel)
{
	uint32_t umask = INADDR_NONE;
//	CFlyFastLock(m_cs);  лок не нужен (контейнер не меняется)
	for (auto it = m_maskList.begin(); it != m_maskList.end(); ++it)
	{
		if (it->second == maskLevel)
		{
			umask = it->first;
			break;
		}
	}
	return umask;
}

uint32_t IPList::addRange(const std::string& fromIP, const std::string& toIP)
{
	const uint32_t ufromIP = Socket::convertIP4(fromIP);
	const uint32_t utoIP = Socket::convertIP4(toIP);
	
	if (ufromIP == INADDR_NONE || ufromIP == 0)
		return START_IP_ERROR;
	if (utoIP == INADDR_NONE || utoIP == 0)
		return END_IP_ERROR;
		
	return addRange(ufromIP, utoIP);
}

uint32_t IPList::addRange(uint32_t fromIP, uint32_t toIP)
{
	if (fromIP > toIP)
	{
		return START_GREATE_THEN_END_RANGE_ERROR;
	}
	if (fromIP == toIP)
	{
		add(fromIP);
		return START_AND_END_EQUAL;
	}
	else
	{
		uint32_t diff = toIP - fromIP;
		int diffLevel = 1;
		uint32_t range = 0x80000000;
		if (diff < 256)
		{
			diffLevel = 24;
			range = 0x00000100;
		}
		while (range > diff)
		{
			range = range >> 1;
			diffLevel++;
		}
		
		uint32_t mask = getMaskByLevel(diffLevel);
		
		uint32_t minIP = fromIP & mask;
		if (minIP < fromIP) minIP += range;
		if (minIP > fromIP)
		{
			addRange(fromIP, minIP - 1);
			fromIP = minIP;
		}
		if (fromIP == toIP)
		{
			add(fromIP);
		}
		else
		{
			if ((minIP + (range - 1)) <= toIP)
			{
				add(minIP, mask);
				fromIP = minIP + range;
			}
			if (fromIP == toIP)
			{
				add(toIP);
			}
			else
			{
				if (fromIP < toIP) addRange(fromIP, toIP);
			}
		}
	}
	return NO_IP_ERROR;
}

void IPList::addLine(std::string& Line, CFlyLog& p_log)
{
	dcassert(!Line.empty() && Line[0] != '#')
	int32_t l_errorCode = 0;
	boost::replace_all(Line, " ", "");
	if (!Line.empty() && Line[0] != '#')
	{
		// Parse with ip/mask or ip-ip or ip
		const string::size_type mask_pos = Line.find('/');
		const string::size_type range_pos = Line.find('-');
		if (mask_pos != string::npos)
		{
			const string ip = Line.substr(0, mask_pos);
			const string mask = Line.substr(mask_pos + 1);
			if (mask.find('.') != string::npos)
			{
				l_errorCode = add(ip, mask);
			}
			else
			{
				l_errorCode = add(ip, Util::toUInt32(mask));
			}
		}
		else if (range_pos != string::npos)
		{
			const string fromIP = Line.substr(0, range_pos);
			const string toIP = Line.substr(range_pos + 1);
			l_errorCode = addRange(fromIP, toIP);
		}
		else
		{
			l_errorCode = add(Line);
		}
		
		if (l_errorCode)
		{
			string l_ErrorString;
			switch (l_errorCode)
			{
				case IP_ERROR:
					l_ErrorString = STRING(WRONG_IP);
					break;
				case MASK_ERROR:
					l_ErrorString = STRING(WRONG_MASK);
					break;
				case START_IP_ERROR:
					l_ErrorString = STRING(WRONG_START_IP);
					break;
				case END_IP_ERROR:
					l_ErrorString = STRING(WRONG_END_IP);
					break;
				case START_GREATE_THEN_END_RANGE_ERROR:
					l_ErrorString = STRING(START_IP_GREATER);
					break;
			}
			if (l_errorCode == START_AND_END_EQUAL)
			{
				p_log.step(STRING(WARNING_PARSE) + ": [" + Line + "] " + STRING(START_IP_EQUAL));
			}
			else
			{
				p_log.step(STRING(ERROR_PARSE) + ": [" + Line + "] " + l_ErrorString + ' ' + STRING(SKIPPED));
			}
		}
	}
	else
	{
		if (Line.empty())
		{
			p_log.step(STRING(EMPTY_LINE_SKIPED));
		}
		else if (Line[0] == '#')
		{
			p_log.step(STRING(COMMENTED_LINE) + ": [" + Line + "] " + STRING(SKIPPED));
		}
	}
}

void IPList::addData(const std::string& Data, CFlyLog& p_log)
{
	string::size_type linestart = 0;
	string::size_type lineend = 0;
	for (;;)
	{
		lineend = Data.find('\n', linestart);
		if (lineend == string::npos)
		{
			string line = Data.substr(linestart);
			addLine(line, p_log);
			break;
		}
		else
		{
			string line = Data.substr(linestart, lineend - linestart - 1);
			addLine(line, p_log);
			linestart = lineend + 1;
		}
	}
}
void IPList::addRangeListAndSort(uint32_t p_ip, uint32_t p_level)
{
	dcassert(!ClientManager::isShutdown());
	CFlyFastLock(m_cs);
	m_ipRangeList[p_level].add(p_ip);
	if (std::find(m_usedList.begin(), m_usedList.end(), p_level) == m_usedList.end())
	{
		m_usedList.push_back(p_level);
		std::sort(m_usedList.begin(), m_usedList.end());
	}
}

bool IPList::checkIp(UINT32 ip)
{
	bool found = false;
	dcassert(!ClientManager::isShutdown());
	if (ClientManager::isShutdown())
	{
		size_t i = 0;
		CFlyFastLock(m_cs);
		while (!found && i < m_usedList.size())
		{
			found = m_ipRangeList[m_usedList[i]].check(ip); // TODO: Please optimize my searching faster than brute force search!
			i++;
		}
	}
	return found;
}

void IPList::clear()
{
	CFlyFastLock(m_cs);
	for (size_t i = 0; i < m_usedList.size(); i++)
	{
		m_ipRangeList[m_usedList[i]].clear();
	}		
	m_usedList.clear();
}

#endif // PPA_INCLUDE_IPFILTER