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

#ifdef PPA_INCLUDE_IPGUARD

#include "IpGuard.h"
#include "Socket.h"
#include "ResourceManager.h"
#include "../windows/resource.h"
#include "../XMLParser/xmlParser.h"
#include "LogManager.h"


#ifdef _DEBUG
boost::atomic_int g_count(0);
#endif

void IpGuard::load()
{
	if (BOOLSETTING(ENABLE_IPGUARD))
	{
		string l_sIPGuard;
		const string l_IPGuard_xml = Util::getConfigPath() + "IPGuard.xml";
		CFlyLog l_IPGuard_log("[IPGuard]");
		if (File::isExist(l_IPGuard_xml))
		{
		
			XMLParser::XMLResults xRes;
			XMLParser::XMLNode xRootNode = XMLParser::XMLNode::parseFile(Text::toT(l_IPGuard_xml).c_str(), 0, &xRes);
			
			if (xRes.error == XMLParser::eXMLErrorNone)
			{
				l_IPGuard_log.step("parse IPGuard.xml");
				XMLParser::XMLNode xStringsNode = xRootNode.getChildNode("IpRanges");
				if (!xStringsNode.isEmpty())
				{
					int i = 0;
					XMLParser::XMLNode xStringNode = xStringsNode.getChildNode("Range", &i);
					while (!xStringNode.isEmpty())
					{
						string IPLine = Socket::convertIP4(Util::toUInt32(xStringNode.getAttributeOrDefault("Start"))) + "-" + Socket::convertIP4(Util::toUInt32(xStringNode.getAttributeOrDefault("End")));
						
						l_sIPGuard += IPLine + "\r\n";
						
						xStringNode = xStringsNode.getChildNode("Range", &i);
					}
				}
				l_IPGuard_log.step("parse IPGuard.xml done");
				try
				{
					l_IPGuard_log.step("write new IPGuard.ini");
					File fout(Util::getConfigPath() + "IPGuard.ini", File::WRITE, File::CREATE | File::TRUNCATE);
					fout.write(l_sIPGuard);
					fout.close();
					l_IPGuard_log.step("delete old IPGuard.xml.");
					File::deleteFile(l_IPGuard_xml);
					l_IPGuard_log.step("convert done.");
				}
				catch (const FileException& e)
				{
					l_IPGuard_log.step("Can't rewrite old IPGuard file: " + e.getError());
				}
			}
		}
		else if (File::isExist(getIPGuardFileName()))
		{
			try
			{
				l_IPGuard_log.step("read IPGuard.ini");
				l_sIPGuard = File(getIPGuardFileName(), File::READ, File::OPEN).read();
			}
			catch (const Exception& e)
			{
				l_IPGuard_log.step("error read IPGuard.ini: " + e.getError());
				dcdebug("IpGuard::load: %s\n", e.getError().c_str());
			}
		}
		else
		{
			Util::WriteTextResourceToFile(IDR_IPGUARD_EXAMPLE, Text::toT(getIPGuardFileName()));
			l_IPGuard_log.step("IPTrust.ini restored from internal resources");
		}
		
		l_IPGuard_log.step("parse IPGuard.ini");
		m_ipGuardList.clear();
		if (!l_sIPGuard.empty())
		{
			m_ipGuardList.addData(l_sIPGuard, l_IPGuard_log);
		}
		l_IPGuard_log.step("parse IPGuard.ini done");
	}
	else
	{
		m_ipGuardList.clear();
	}
}

bool IpGuard::check(const string& aIP, string& reason)
{
	if (aIP.empty())
		return false;
		
#ifdef _DEBUG
	dcdebug("IPGuard::check  count = %d\n", int(++g_count));
#endif
	
	if (m_ipGuardList.checkIp(aIP))
		return !BOOLSETTING(IP_GUARD_IS_DENY_ALL);
		
	reason = STRING(IPGUARD_DEFAULT_POLICY);
	return BOOLSETTING(IP_GUARD_IS_DENY_ALL);
}

void IpGuard::check(uint32_t aIP, Socket* socket /*= nullptr*/)
{
	if (aIP == 0)
		return;
		
#ifdef _DEBUG
	dcdebug("IPGuard::check  count = %d\n", int(++g_count));
#endif
	
	
	if (m_ipGuardList.checkIp(ntohl(aIP)))
	{
		if (!BOOLSETTING(IP_GUARD_IS_DENY_ALL))
		{
			if (socket != nullptr)
				socket->disconnect();
				
			throw SocketException(STRING(IPGUARD) + ": (" + inet_ntoa(*(in_addr*)&aIP) + ")");
		}
		
		return;
	}
	
	if (BOOLSETTING(IP_GUARD_IS_DENY_ALL))
	{
		if (socket != nullptr)
			socket->disconnect();
			
		throw SocketException(STRING(IPGUARD) + ": " + STRING(IPGUARD_DEFAULT_POLICY) + " (" + inet_ntoa(*(in_addr*)&aIP) + ")");
	}
}

#endif // PPA_INCLUDE_IPGUARD
