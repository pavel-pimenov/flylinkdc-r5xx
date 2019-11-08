/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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

#include "IpGuard.h"
#include "Socket.h"
#include "ResourceManager.h"
#include "../windows/resource.h"
#include "../XMLParser/xmlParser.h"
#include "SimpleXML.h"
#include "LogManager.h"
#include "../FlyFeatures/flyServer.h"

IPList IpGuard::g_ipGuardList;
int IpGuard::g_ipGuardListLoad = 0;

void IpGuard::load()
{
	CFlyBusy l(g_ipGuardListLoad);
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
		g_ipGuardList.clear();
		if (!l_sIPGuard.empty())
		{
			g_ipGuardList.addData(l_sIPGuard, l_IPGuard_log);
		}
		l_IPGuard_log.step("parse IPGuard.ini done");
	}
	else
	{
		g_ipGuardList.clear();
	}
}
bool IpGuard::is_block_ip(const string& aIP, uint32_t& p_ip4)
{
	p_ip4 = Socket::convertIP4(aIP);
	return CFlyServerConfig::isBlockIP(aIP);
}

bool IpGuard::check_ip_str(const string& aIP, string& reason)
{
	if (g_ipGuardListLoad > 0) // fix https://drdump.com/Problem.aspx?ProblemID=263631
		return false;
	if (aIP.empty())
		return false;
		
	uint32_t l_ip4;
	if (IpGuard::is_block_ip(aIP, l_ip4))
	{
		reason = "Block IP: " + aIP;
		return true;
	}
	if (BOOLSETTING(ENABLE_IPGUARD))
	{
		if (g_ipGuardList.checkIp(l_ip4))
		{
			return !BOOLSETTING(IP_GUARD_IS_DENY_ALL);
		}
		
		reason = STRING(IPGUARD_DEFAULT_POLICY);
		return BOOLSETTING(IP_GUARD_IS_DENY_ALL);
	}
	else
		return false;
}

void IpGuard::check_ip_str(const string& aIP, Socket* socket /*= nullptr*/)
{
	if (aIP.empty())
		return;
		
	uint32_t l_ip4;
	if (IpGuard::is_block_ip(aIP, l_ip4))
	{
		if (socket)
		{
			socket->disconnect();
		}
		throw SocketException(STRING(IPGUARD_BLOCK_LIST) + ": (" + aIP + ")");
	}
	if (BOOLSETTING(ENABLE_IPGUARD))
	{
		if (g_ipGuardList.checkIp(l_ip4))
		{
			if (!BOOLSETTING(IP_GUARD_IS_DENY_ALL))
			{
				if (socket)
				{
					socket->disconnect();
				}
				throw SocketException(STRING(IPGUARD) + ": (" + aIP + ")");
			}
			return;
		}
		if (BOOLSETTING(IP_GUARD_IS_DENY_ALL))
		{
			if (socket)
			{
				socket->disconnect();
			}
			
			throw SocketException(STRING(IPGUARD) + ": " + STRING(IPGUARD_DEFAULT_POLICY) + " (" + aIP + ")");
		}
	}
}
