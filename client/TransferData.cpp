//-----------------------------------------------------------------------------
//(c) 2007-2019 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#include "stdinc.h"

#include "ResourceManager.h"
#include "TransferData.h"


#ifdef FLYLINKDC_USE_TORRENT
void TransferData::init(libtorrent::torrent_status const& s)
{
	//l_td.m_hinted_user = d->getHintedUser();
	//m_token = s.info_hash.to_string(); для токена используется m_sha1
	
	m_sha1 = s.info_hash;
	m_pos = 1;
	m_speed = s.download_payload_rate;
	m_actual = s.total_done;// d->getActual();
	m_second_left = 0;// d->getSecondsLeft();
	m_start = 0; // d->getStart();
	m_size = s.total_wanted; // ti->files().file_size(i);
	m_type = 0; // d->getType();
	m_path = s.save_path + s.name; // - путь к корню торрент-файла
	m_num_seeds = s.num_seeds;
	m_num_peers = s.num_peers;
	m_is_torrent = true;
	m_is_seeding = s.state == libtorrent::torrent_status::seeding || s.state == libtorrent::torrent_status::finished;
	m_is_pause = (s.flags & libtorrent::torrent_flags::paused) == libtorrent::torrent_flags::paused;
	//calc_percent();
	m_percent = s.progress_ppm / 10000;
	///l_td.m_status_string += _T("[Torrent] Peers:") + Util::toStringT(s.num_peers) + _T(" Seeds:") + Util::toStringT(s.num_seeds) + _T(" ");
	//l_td.m_status_string += Text::tformat(TSTRING(DOWNLOADED_BYTES), Util::formatBytesW(l_td.m_pos).c_str(),
	//  l_td.m_percent, l_td.get_elapsed(aTick).c_str());
	m_status_string = _T("[Torrent] ");
	switch (s.state)
	{
		case  libtorrent::torrent_status::checking_files:
			m_status_string += Text::tformat(TSTRING(CHECKED_BYTES), "", m_percent, "");
			break;
		case  libtorrent::torrent_status::downloading_metadata:
			m_status_string += TSTRING(DOWNLOADING_METADATA);
			break;
		case  libtorrent::torrent_status::downloading:
			m_status_string += TSTRING(DOWNLOADING);
			break;
		case  libtorrent::torrent_status::finished:
			m_status_string += TSTRING(FINISHED);
			break;
		case  libtorrent::torrent_status::seeding:
			m_status_string += TSTRING(SEEDING);
			break;
		case  libtorrent::torrent_status::allocating:
			m_status_string += TSTRING(ALLOCATING);
			break;
		case  libtorrent::torrent_status::checking_resume_data:
			m_status_string += TSTRING(CHECKING_RESUME_DATA);
			break;
		default:
			dcassert(0);
			break;
	}
	
	if (m_is_pause && s.state == libtorrent::torrent_status::downloading_metadata)
	{
		m_status_string += TSTRING(PLEASE_WAIT);
	}
	if (s.state == libtorrent::torrent_status::seeding)
	{
		m_status_string +=  +_T(" (Download: ") + Text::toT(Util::formatSeconds(s.active_duration.count() - s.finished_duration.count())) + _T(")")
		                    + _T("(Seedind: ") + Text::toT(Util::formatSeconds(s.seeding_duration.count())) + _T(")");
	}
	if (s.state == libtorrent::torrent_status::downloading ||
	        s.state == libtorrent::torrent_status::finished)
	{
		const tstring l_peer_seed = _T(" Peers:") + Util::toStringT(s.num_peers) + _T(" Seeds:") + Util::toStringT(s.num_seeds) + _T(" ");
		if (s.state == libtorrent::torrent_status::seeding) // TODO - false
		{
			m_status_string = l_peer_seed + _T("  Download: ") +
			                  Util::formatBytesW(s.total_download) + _T(" Upload: ") +
			                  Util::formatBytesW(s.total_upload) + _T(" Time: ") +
			                  Text::toT(Util::formatSeconds(s.seeding_duration.count())).c_str();
		}
		else
		{
			m_status_string += l_peer_seed + Text::tformat(TSTRING(DOWNLOADED_BYTES), Util::formatBytesW(s.total_done).c_str(),
			                                               m_percent,
			                                               Text::toT(Util::formatSeconds(s.active_duration.count())).c_str());
		}
	}
}
#endif
