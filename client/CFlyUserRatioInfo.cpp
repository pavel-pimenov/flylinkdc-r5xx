//-----------------------------------------------------------------------------
//(c) 2014-2019 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#include "stdinc.h"

#include "CFlylinkDBManager.h"
#include "SettingsManager.h"

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO

CFlyUserRatioInfo::CFlyUserRatioInfo(User* p_user):
	m_ip_map_ptr(nullptr),
	m_user(p_user)
{
}

CFlyUserRatioInfo::~CFlyUserRatioInfo()
{
	flushRatioL();
	delete m_ip_map_ptr;
}

bool CFlyUserRatioInfo::flushRatioL()
{
	if ((is_dirty() ||
	        m_user->m_message_count.is_dirty() ||
	        m_user->m_last_ip_sql.is_dirty())
	        && m_user->getHubID() && !m_user->m_nick.empty()
	        && CFlylinkDBManager::isValidInstance())
	{
		bool l_is_sql_not_found = m_user->isSet(User::IS_SQL_NOT_FOUND);
		CFlylinkDBManager::getInstance()->store_all_ratio_and_last_ip(m_user->getHubID(),
		                                                              m_user->m_nick,
		                                                              *this,
		                                                              m_user->m_message_count.get(),
		                                                              m_user->getLastIPfromRAM(),
		                                                              m_user->m_last_ip_sql.is_dirty(),
		                                                              m_user->m_message_count.is_dirty(),
		                                                              l_is_sql_not_found
		                                                             );
		m_user->setFlag(User::IS_SQL_NOT_FOUND, l_is_sql_not_found);
		set_dirty(false);
		m_user->m_last_ip_sql.reset_dirty();
		m_user->m_message_count.reset_dirty();
		return true;
	}
	return false;
}
bool CFlyUserRatioInfo::tryLoadRatio(const boost::asio::ip::address_v4& p_last_ip_from_sql)
{
	//dcassert(!p_last_ip_from_sql.is_unspecified());
	if (BOOLSETTING(ENABLE_RATIO_USER_LIST) && m_user->getHubID() && !m_user->m_nick.empty()) // Не грузили данные по рейтингу?
	{
		CFlylinkDBManager::getInstance()->load_ratio(
		    m_user->getHubID(),
		    m_user->m_nick,
		    *this,
		    p_last_ip_from_sql);
		return get_download() || get_upload();
	}
	else
	{
		dcassert(get_upload() == 0 && get_download() == 0);
#ifdef _DEBUG
		set_upload(0);
		set_download(0);
#endif
	}
	return false;
}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO

