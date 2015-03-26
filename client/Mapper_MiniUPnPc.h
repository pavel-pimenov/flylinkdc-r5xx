/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_DCPP_MAPPER_MINIUPNPC_H
#define DCPLUSPLUS_DCPP_MAPPER_MINIUPNPC_H

#include "Mapper.h"

class Mapper_MiniUPnPc : public Mapper
{
	public:
		Mapper_MiniUPnPc() : Mapper(), m_initialized(false) { }
		
		static const string g_name;
		
	private:
		bool init();
		void uninit();
		
		bool add(const unsigned short port, const Protocol protocol, const string& description);
		bool remove(const unsigned short port, const Protocol protocol);
		static void log_error(const int p_error_code, const string& p_name);
		
		uint32_t renewal() const
		{
			return 0;
		}
		
		string getDeviceName() const
		{
			return m_device;
		}
		string getModelDescription() const
		{
			return m_modelDescription;
		}
		string getExternalIP();
		
		const string& getMapperName() const
		{
			return g_name;
		}
		
		bool m_initialized;
		
		string m_url;
		string m_service;
		string m_device;
		string m_modelDescription;
};

#endif
