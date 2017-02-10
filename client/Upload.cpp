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

#include "stdinc.h"
#include "Upload.h"
#include "Streams.h"

// [!] IRainman fix.
static const TTHValue g_empty_tth;
Upload::Upload(UserConnection* p_conn, const TTHValue& p_tth, const string& p_path, const string& p_ip, const string& p_chiper_name):
	Transfer(p_conn, p_path, p_tth, p_ip, p_chiper_name),
	m_read_stream(nullptr),
	m_delayTime(0)
// [~] IRainman fix.
{
	//!!!!!!!!!!!!!!!! p_conn->setUpload(this);
}

Upload::~Upload()
{
	//!!!!!!!!!!!!!!!! getUserConnection()->setUpload(nullptr);
	safe_delete(m_read_stream);
}

void Upload::getParams(StringMap& p_params) const
{
	Transfer::getParams(getUserConnection(), p_params);
	p_params["source"] = getPath();
}
