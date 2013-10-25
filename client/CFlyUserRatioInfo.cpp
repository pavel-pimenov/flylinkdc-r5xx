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
	m_is_sql_record_exists(false),
	m_is_dirty(false)
{
}

CFlyUserRatioInfo::~CFlyUserRatioInfo()
{
	flushRatio();
	delete m_ip_map_ptr;
}
void CFlyUserRatioInfo::addUpload(const string& p_ip, uint64_t p_size)
{
	m_upload += p_size;
	FastLock l(g_cs);
	find_ip_map(p_ip).m_upload += p_size;
	setDirty(true);
}
void CFlyUserRatioInfo::addDownload(const string& p_ip, uint64_t p_size)
{
	m_download += p_size;
	FastLock l(g_cs);
	find_ip_map(p_ip).m_download += p_size;
	setDirty(true);
}

void CFlyUserRatioInfo::flushRatio()
{
	if (m_is_dirty && m_user->getHubID() && !m_last_ip_sql.is_unspecified() && !m_user->m_nick.empty()
	        && CFlylinkDBManager::isValidInstance()) // fix https://www.crash-server.com/DumpGroup.aspx?ClientID=ppa&Login=Guest&DumpGroupID=86337
	{
		dcassert(!m_last_ip_sql.is_unspecified());
		FastLock l(g_cs); // Для защиты m_upload_download_map
		CFlylinkDBManager::getInstance()->store_all_ratio_and_last_ip(m_user->getHubID(), m_user->m_nick, m_ip_map_ptr, m_last_ip_sql); // m_user->m_last_ip ??
		setDirty(false);
	}
}
bool CFlyUserRatioInfo::try_load_ratio(bool p_is_create, const boost::asio::ip::address_v4& p_last_ip)
{
	if (m_is_sql_record_exists)
		return true;
	dcassert(!p_last_ip.is_unspecified());
	if (m_user->getHubID() && !m_user->m_nick.empty()) // Не грузили данные по рейтингу?
	{
		if (!p_last_ip.is_unspecified())
		{
			m_last_ip_sql = p_last_ip;
		}
		auto l_dbm = CFlylinkDBManager::getInstance();
		const CFlyRatioItem& l_item = l_dbm->load_ratio(
		                                  m_user->getHubID(),
		                                  m_user->m_nick,
		                                  *this,
		                                  p_last_ip);
		m_upload   = l_item.m_upload;
		m_download = l_item.m_download;
		m_is_sql_record_exists = !p_last_ip.is_unspecified() || m_download || m_upload;
	}
	else
	{
		dcassert(m_upload == 0 && m_download == 0);
#ifdef _DEBUG
		m_upload = 0;
		m_download = 0;
#endif
	}
	return m_is_sql_record_exists || p_is_create;
}
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO

