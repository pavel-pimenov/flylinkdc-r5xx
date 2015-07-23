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

#include "IpGrant.h"
#include "File.h"
#include "LogManager.h"
#include "../windows/resource.h"

#ifdef SSA_IPGRANT_FEATURE

void IpGrant::load()
{
	if (BOOLSETTING(EXTRA_SLOT_BY_IP))
	{
		CFlyLog l_IPGrant_log("[IPGrant]");
		string l_data;
		const string l_IPGrant_ini = getConfigFileName();
		
		if (File::isExist(l_IPGrant_ini))
		{
			try
			{
				l_IPGrant_log.step("read IPGrant.ini");
				l_data = File(l_IPGrant_ini, File::READ, File::OPEN).read();
			}
			catch (const FileException& e)
			{
				l_IPGrant_log.step("Can't read IPGrant.ini file: " + e.getError());
				dcdebug("IPGrant::load: %s\n", e.getError().c_str());
			}
		}
		else
		{
			Util::WriteTextResourceToFile(IDR_IPGRANT_EXAMPLE, Text::toT(l_IPGrant_ini));
			l_IPGrant_log.step("IPGrant.ini restored from internal resources");
		}
		l_IPGrant_log.step("parse IPGrant.ini");
		{
			FastLock l(m_cs);
			m_ipList.clear();
			if (!l_data.empty())
			{
				m_ipList.addData(l_data, l_IPGrant_log);
			}
		}
		l_IPGrant_log.step("parse IPGrant.ini done");
	}
}

bool IpGrant::check(uint32_t p_ip4)
{
	if (p_ip4 == INADDR_NONE)
		return false;
	FastLock l(m_cs);
	return m_ipList.checkIp(p_ip4);
}

#endif // SSA_IPGRANT_FEATURE