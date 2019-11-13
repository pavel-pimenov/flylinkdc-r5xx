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

#include "UserConnection.h"
#include "QueueItem.h"
#include "HashManager.h"

// [!] IRainman fix.
Download::Download(UserConnection* p_conn, const QueueItemPtr& p_item, const string& p_ip, const string& p_chiper_name) noexcept :
	Transfer(p_conn, p_item->getTarget(), p_item->getTTH(), p_ip, p_chiper_name),
	m_qi(p_item),
	m_download_file(nullptr),
	treeValid(false)
#ifdef FLYLINKDC_USE_DROP_SLOW
	, m_lastNormalSpeed(0)
#endif
{
	////////// p_conn->setDownload(this);
	
	// [-] QueueItem::SourceConstIter source = qi.getSource(getUser()); [-] IRainman fix.
	
	if (m_qi->isSet(QueueItem::FLAG_PARTIAL_LIST))
	{
		setType(TYPE_PARTIAL_LIST);
		//pfs = qi->getTarget(); // [+] IRainman fix. TODO: пофикшено, но не до конца :(
	}
	else if (m_qi->isSet(QueueItem::FLAG_USER_LIST))
	{
		setType(TYPE_FULL_LIST);
	}
	
#ifdef IRAINMAN_INCLUDE_USER_CHECK
	if (m_qi->isSet(QueueItem::FLAG_USER_CHECK))
		setFlag(FLAG_USER_CHECK);
#endif
	// [+] SSA
	if (m_qi->isSet(QueueItem::FLAG_USER_GET_IP))
		setFlag(FLAG_USER_GET_IP);
		
	// TODO: убрать флажки после рефакторинга, ибо они всё равно бесполезны
	const bool l_is_type_file = getType() == TYPE_FILE && m_qi->getSize() != -1;
	
	bool l_check_tth_sql = false;
	if (getTTH() != TTHValue()) // не зовем проверку на TTH = 0000000000000000000000000000000000 (файл-лист)
	{
		__int64 l_block_size = 0;
		l_check_tth_sql = CFlylinkDBManager::getInstance()->get_tree(getTTH(), m_tiger_tree, l_block_size);
		if (l_check_tth_sql && l_block_size)
		{
			m_qi->setBlockSize(l_block_size);
		}
	}
	{
		RLock(*QueueItem::g_cs); // Fix https://drdump.com/DumpGroup.aspx?DumpGroupID=476822&Login=guest
		const bool l_is_check_tth = l_is_type_file && l_check_tth_sql;
		const auto& l_source_it = m_qi->findSourceL(getUser());
		if (l_source_it != m_qi->getSourcesL().end())
		{
			const auto& l_src = l_source_it->second;
			if (l_src.isSet(QueueItem::Source::FLAG_PARTIAL))
			{
				setFlag(FLAG_DOWNLOAD_PARTIAL);
			}
			
			if (l_is_type_file)
			{
				if (l_is_check_tth)
				{
					setTreeValid(true);
					setSegment(m_qi->getNextSegmentL(getTigerTree().getBlockSize(), p_conn->getChunkSize(), p_conn->getSpeed(), l_src.getPartialSource()));
				}
				else if (p_conn->isSet(UserConnection::FLAG_SUPPORTS_TTHL) && !l_src.isSet(QueueItem::Source::FLAG_NO_TREE) && m_qi->getSize() > MIN_BLOCK_SIZE) // [!] IRainman opt.
				{
					// Get the tree unless the file is small (for small files, we'd probably only get the root anyway)
					setType(TYPE_TREE);
					getTigerTree().setFileSize(m_qi->getSize());
					setSegment(Segment(0, -1));
				}
				else
				{
					// Use the root as tree to get some sort of validation at least...
					getTigerTree() = TigerTree(m_qi->getSize(), m_qi->getSize(), getTTH());
					setTreeValid(true);
					setSegment(m_qi->getNextSegmentL(getTigerTree().getBlockSize(), 0, 0, l_src.getPartialSource()));
				}
				
				if ((getStartPos() + getSize()) != m_qi->getSize())
				{
					setFlag(FLAG_CHUNKED);
				}
				
				if (getSegment().getOverlapped())
				{
					setFlag(FLAG_OVERLAP);
					m_qi->setOverlapped(getSegment(), true);
				}
			}
		}
		else
		{
			dcassert(0);
		}
	}
}

Download::~Download()
{
	dcassert(m_download_file == nullptr);
	//////////getUserConnection()->setDownload(nullptr);
}

void Download::getCommand(AdcCommand& cmd, bool zlib) const
{
	cmd.addParam(Transfer::g_type_names[getType()]);
	
	if (getType() == TYPE_PARTIAL_LIST)
	{
		cmd.addParam(Util::toAdcFile(getTempTarget()));
	}
	else if (getType() == TYPE_FULL_LIST)
	{
		if (isSet(Download::FLAG_XML_BZ_LIST))
		{
			cmd.addParam(g_user_list_name_bz);
		}
		else
		{
			cmd.addParam(g_user_list_name);
		}
	}
	else
	{
		cmd.addParam("TTH/" + getTTH().toBase32());
	}
	//dcassert(getStartPos() >= 0);
	cmd.addParam(Util::toString(getStartPos()));
	cmd.addParam(Util::toString(getSize()));
	
	if (zlib && BOOLSETTING(COMPRESS_TRANSFERS))
	{
		cmd.addParam("ZL1");
	}
}

void Download::getParams(StringMap& params) const
{
	Transfer::getParams(getUserConnection(), params);
	params["target"] = getPath();
}

string Download::getTempTarget() const
{
	return m_qi->getTempTarget();
}
