//-----------------------------------------------------------------------------
//(c) 2013 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#include "stdinc.h"

#include "CFlyUserRatioInfo.h"
#include "CFlylinkDBManager.h"

#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
FastCriticalSection CFlyUserRatioInfo::g_cs;

CFlyUserRatioInfo::CFlyUserRatioInfo(User* p_user):
	m_ip_map_ptr(nullptr),
	m_user(p_user),
	m_is_dirty(false)
{
}

CFlyUserRatioInfo::~CFlyUserRatioInfo()
{
	flushRatio();
	delete m_ip_map_ptr;
}
void CFlyUserRatioInfo::addUpload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size)
{
	dcassert(p_size);
	FastLock l(g_cs);
	m_upload += p_size;
	find_ip_map(p_ip).m_upload += p_size;
	setDirty(true);
}
void CFlyUserRatioInfo::addDownload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size)
{
	dcassert(p_size);
	FastLock l(g_cs);
	m_download += p_size;
	find_ip_map(p_ip).m_download += p_size;
	setDirty(true);
}

void CFlyUserRatioInfo::flushRatio()
{
	if (m_is_dirty && m_user->getHubID() && !m_user->m_nick.empty()
	        && CFlylinkDBManager::isValidInstance()) // fix https://www.crash-server.com/DumpGroup.aspx?ClientID=ppa&Login=Guest&DumpGroupID=86337
	{
		FastLock l(g_cs); // Для защиты m_upload_download_map
		CFlylinkDBManager::getInstance()->store_all_ratio_and_last_ip(m_user->getHubID(), m_user->m_nick, m_ip_map_ptr, m_user->m_last_ip); // TODO зачем передавать туда m_user->m_last_ip?
		setDirty(false);
	}
}
bool CFlyUserRatioInfo::try_load_ratio(const boost::asio::ip::address_v4& p_last_ip_from_sql)
{
	dcassert(!p_last_ip_from_sql.is_unspecified());
	if (m_user->getHubID() && !m_user->m_nick.empty()) // Не грузили данные по рейтингу?
	{
		const CFlyRatioItem& l_item = CFlylinkDBManager::getInstance()->load_ratio(
		                                  m_user->getHubID(),
		                                  m_user->m_nick,
		                                  *this,
		                                  p_last_ip_from_sql);
		m_upload   = l_item.m_upload;
		m_download = l_item.m_download;
		return m_download || m_upload;
	}
	else
	{
		dcassert(m_upload == 0 && m_download == 0);
#ifdef _DEBUG
		m_upload = 0;
		m_download = 0;
#endif
	}
	return false;
}
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO

