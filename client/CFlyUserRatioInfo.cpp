//-----------------------------------------------------------------------------
//(c) 2013 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#include "stdinc.h"

#include "CFlyUserRatioInfo.h"
#include "CFlylinkDBManager.h"

#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
FastCriticalSection CFlyUserRatioInfo::g_cs;

CFlyUserRatioInfo::CFlyUserRatioInfo(User* p_user): 
     m_user(p_user), 
	 m_is_sql_record_exists(false), 
	 m_is_ditry(false)
{
}

CFlyUserRatioInfo::~CFlyUserRatioInfo()
{
	flushRatio();
}

void CFlyUserRatioInfo::setDitry(bool p_value)
{
	m_is_ditry = p_value;
}
void CFlyUserRatioInfo::addUpload(const string& p_ip, uint64_t p_size)
{
	m_upload += p_size;
	FastLock l(g_cs);
	auto& l_u_d_map = m_upload_download_map[p_ip]; 
	l_u_d_map.m_upload += p_size;
	setDitry(true);
}
void CFlyUserRatioInfo::addDownload(const string& p_ip, uint64_t p_size)
{
	m_download += p_size;
	FastLock l(g_cs);
	auto& l_u_d_map = m_upload_download_map[p_ip]; 
	l_u_d_map.m_download += p_size;
	setDitry(true);
}

void CFlyUserRatioInfo::flushRatio()
{
	//  dcdebug(" [!!!!!!] CFlyUserRatioInfo::flush() m_last_ip = %s nick = %s, m_is_ditry = %d m_nick_id = %d m_hub_id = %d m_is_sql_record_exists = %d this = %p\r\n",
	//   m_last_ip.c_str(), m_nick.c_str(), int(m_is_ditry), int(m_hub_id), int(m_is_sql_record_exists), this);
	// dcassert(!m_nick.empty());
	//dcassert(m_hub_id && (m_nick_id || !m_nick.empty()));
	// dcassert(m_nick_id && !m_nick.empty()); Такое возможно когда ник - это бот хаба
	if (m_is_ditry && m_user->getHubID() && !m_user->m_nick.empty() && !m_last_ip_sql.empty() // !m_user->m_last_ip.empty()
	        && CFlylinkDBManager::isValidInstance()) // fix https://www.crash-server.com/DumpGroup.aspx?ClientID=ppa&Login=Guest&DumpGroupID=86337
	{
		dcassert(!m_last_ip_sql.empty());
		FastLock l(g_cs); // Для защиты m_upload_download_map
		CFlylinkDBManager::getInstance()->store_all_ratio_and_last_ip(m_user->getHubID(), m_user->m_nick, m_upload_download_map, m_last_ip_sql); // m_user->m_last_ip ??
			setDitry(false);
		}
	}
bool CFlyUserRatioInfo::try_load_ratio(bool p_is_create, const string& p_last_ip)
{
	if(m_is_sql_record_exists)
		return true;
	dcassert(!p_last_ip.empty());
	if (m_user->getHubID() && !m_user->m_nick.empty()) // Не грузили данные по рейтингу?
	{
		   if(!p_last_ip.empty())
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
			m_is_sql_record_exists = !p_last_ip.empty() || m_download || m_upload;
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

