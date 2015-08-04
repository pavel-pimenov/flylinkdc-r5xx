//-----------------------------------------------------------------------------
//(c) 2014-2015 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#include "stdinc.h"

#include "CFlyUserRatioInfo.h"
#include "CFlylinkDBManager.h"

#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO

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
void CFlyUserRatioInfo::addUpload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size)
{
	dcassert(p_size);
	add_upload(p_size);
	find_ip_map(p_ip).add_upload(p_size);
}
void CFlyUserRatioInfo::incMessagesCount()
{
	++m_message_count;
	set_dirty(true);
}
void CFlyUserRatioInfo::addDownload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size)
{
	dcassert(p_size);
	add_download(p_size);
	find_ip_map(p_ip).add_download(p_size);
}

void CFlyUserRatioInfo::flushRatioL()
{
	if (is_dirty() && m_user->getHubID() && !m_user->m_nick.empty()
	        && CFlylinkDBManager::isValidInstance()) // fix https://www.crash-server.com/DumpGroup.aspx?ClientID=ppa&Login=Guest&DumpGroupID=86337
	{
		CFlylinkDBManager::getInstance()->store_all_ratio_and_last_ip(m_user->getHubID(), m_user->m_nick, m_ip_map_ptr, m_message_count, m_user->m_last_ip); // TODO зачем передавать туда m_user->m_last_ip?
		set_dirty(false);
		m_user->m_is_last_ip_dirty = false;
	}
}
bool CFlyUserRatioInfo::tryLoadRatio(const boost::asio::ip::address_v4& p_last_ip_from_sql)
{
	//dcassert(!p_last_ip_from_sql.is_unspecified());
	if (BOOLSETTING(ENABLE_RATIO_USER_LIST) && m_user->getHubID() && !m_user->m_nick.empty()) // Не грузили данные по рейтингу?
	{
		const CFlyRatioItem& l_item = CFlylinkDBManager::getInstance()->load_ratio(
		                                  m_user->getHubID(),
		                                  m_user->m_nick,
		                                  *this,
		                                  p_last_ip_from_sql);
		set_upload(l_item.get_upload());
		set_download(l_item.get_download());
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
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO

