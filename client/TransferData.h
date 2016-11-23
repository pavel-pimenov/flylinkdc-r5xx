//-----------------------------------------------------------------------------
//(c) 2007-2016 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#pragma once

#ifndef DCPLUSPLUS_DCPP_TRANSFER_DATA_H_
#define DCPLUSPLUS_DCPP_TRANSFER_DATA_H_

#include "HintedUser.h"
#ifdef _DEBUG
#include "LogManager.h"
#endif
#include "libtorrent/sha1_hash.hpp"

namespace libtorrent
{
struct torrent_status;
}
class TransferData
{
	public:
		TransferData(const string& p_token): m_actual(0),
			m_pos(0), m_start_pos(0), m_start(0),
			m_second_left(0), m_percent(0),
			m_type(0), m_size(0), m_token(p_token),
			m_speed(0), m_num_seeds(0), m_num_peers(0), m_is_torrent(false), m_is_seeding(false)
		{
		}
		void init(libtorrent::torrent_status const& s);
		int64_t m_actual;
		int64_t m_pos;
		int64_t m_start_pos;
		uint64_t m_start;
		int64_t m_running_average;
		int64_t m_second_left;
		int64_t m_size;
		uint8_t m_type;
		int64_t m_speed;
		
		int m_num_seeds;
		int m_num_peers;
		bool m_is_torrent;
		bool m_is_seeding;
		libtorrent::sha1_hash m_sha1;
		
		double m_percent;
		tstring m_status_string;
		string m_path;
		string m_token;
		HintedUser m_hinted_user;
		
		void calc_percent()
		{
			dcassert(m_size);
			if (m_size)
				m_percent = (double)m_pos * 100.0 / m_size;
			else
				m_percent = 0;
		}
		tstring get_elapsed(uint64_t aTick) const
		{
			tstring l_elapsed;
			if (m_start)
			{
				l_elapsed = Util::formatSecondsW((aTick - m_start) / 1000);
			}
			return l_elapsed;
		}
		void log_debug() const
		{
#ifdef ____DEBUG
			LogManager::message("TransferData-dump = "
			                    " m_actual = " + Util::toString(m_actual) +
			                    " m_pos = " + Util::toString(m_pos) +
			                    " m_start_pos = " + Util::toString(m_start_pos) +
			                    " m_start = " + Util::toString(m_start) +
			                    " m_speed = " + Util::toString(m_speed) +
			                    " m_second_left = " + Util::toString(m_second_left) +
			                    " m_size = " + Util::toString(m_size) +
			                    " m_type = " + Util::toString(m_type) +
			                    " m_percent = " + Util::toString(m_percent) +
			                    " user = " + m_hinted_user.user->getLastNick() +
			                    " hub = " + m_hinted_user.hint +
			                    " m_status_string = " + Text::fromT(m_status_string)
			                   );
#endif
		}
};
typedef std::vector<TransferData> UploadArray;
typedef std::vector<TransferData> DownloadArray;


#endif /*DCPLUSPLUS_DCPP_TRANSFER_DATA_H_*/
