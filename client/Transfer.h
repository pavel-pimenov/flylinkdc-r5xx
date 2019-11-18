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

#ifndef DCPLUSPLUS_DCPP_TRANSFER_H_
#define DCPLUSPLUS_DCPP_TRANSFER_H_

#include "Segment.h"
#include "TransferData.h"
#include <deque>

class UserConnection;

class Transfer
#ifdef _DEBUG
	: private boost::noncopyable
#endif
{
	public:
		enum Type
		{
			TYPE_FILE,
			TYPE_FULL_LIST,
			TYPE_PARTIAL_LIST,
			TYPE_TREE,
			TYPE_LAST
		};
		
		static const string g_type_names[TYPE_LAST];
		static const string g_user_list_name;
		static const string g_user_list_name_bz;
		
		explicit Transfer(UserConnection* p_conn, const string& p_path, const TTHValue& p_tth, const string& p_ip, const string& p_chiper_name);
		virtual ~Transfer() { }
		int64_t getPos() const
		{
			return m_pos; // [3] https://www.box.net/shared/60b6618fc263e636bb4e
		}
		int64_t getStartPos() const
		{
			return getSegment().getStart();
		}
		void resetPos()
		{
			m_pos = 0;    //http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2153
			m_actual = 0;
		}
		void addPos(int64_t aBytes, int64_t aActual)
		{
			m_pos += aBytes;
			m_actual += aActual;
		}
		/** Record a sample for average calculation */
		void tick(uint64_t p_CurrentTick);
		int64_t getRunningAverage() const
		{
			return m_runningAverage;
		}
		int64_t getActual() const
		{
			return m_actual;
		}
		int64_t getSize() const
		{
			return getSegment().getSize();
		}
		void setSize(int64_t size)
		{
			m_segment.setSize(size);
		}
		bool getOverlapped() const
		{
			return getSegment().getOverlapped();
		}
		void setOverlapped(bool overlap)
		{
			m_segment.setOverlapped(overlap);
		}
		void setStartPos(int64_t aPos)
		{
			m_startPos = aPos;
			m_pos = aPos;
		}
		
		int64_t getSecondsLeft(const bool wholeFile = false) const;
		
	protected:
		void getParams(const UserConnection* aSource, StringMap& params) const;
	public:
		UserPtr& getUser()
		{
			return m_hinted_user.user;
		}
		const UserPtr& getUser() const
		{
			return m_hinted_user.user;
		}
		HintedUser getHintedUser() const
		{
			return m_hinted_user;
		}
		const string& getPath() const
		{
			return m_path;
		}
		const TTHValue& getTTH() const
		{
			return m_tth;
		}
		string getConnectionQueueToken() const;
		const UserConnection* getUserConnection() const
		{
			return m_userConnection;
		}
		UserConnection* getUserConnection()
		{
			return m_userConnection;
		}
		const string& getChiperName() const
		{
			return m_chiper_name;
		}
		const string& getIP() const
		{
			return m_ip;
		}
		
		GETSET(Segment, m_segment, Segment);
		GETSET(int64_t, m_fileSize, FileSize);
		GETSET(Type, m_type, Type);
		uint64_t getStart() const
		{
			return m_start;
		}
		void setStart(uint64_t tick);
		const uint64_t getLastActivity();
		GETSET(uint64_t, m_lastTick, LastTick);
		const bool m_isSecure;
		const bool m_isTrusted;
	private:
		uint64_t m_start;
		
		typedef std::pair<uint64_t, const int64_t> Sample;
		typedef std::deque<Sample> SampleList;
		
		SampleList m_samples;
		FastCriticalSection m_cs;
		
		/** The file being transferred */
		const string m_path;
		/** TTH of the file being transferred */
		const TTHValue m_tth;
		/** Bytes transferred over socket */
		int64_t m_actual;
		/** Bytes transferred to/from file */
		int64_t m_pos;
		/** Starting position */
		int64_t m_startPos;
		/** Actual speed */
		int64_t m_runningAverage;
		UserConnection* m_userConnection;
		HintedUser m_hinted_user;
		const string m_chiper_name;
		const string m_ip;
};

#endif /*TRANSFER_H_*/
