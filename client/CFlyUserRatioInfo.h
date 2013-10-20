#ifndef CFlyUserRatioInfo_H
#define CFlyUserRatioInfo_H

#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include "Thread.h"

#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
template <class T> class CFlyUploadDownloadPair
{
	public:
		T  m_upload;
		T  m_download;
		CFlyUploadDownloadPair(): m_upload(0), m_download(0)
		{
		}
		~CFlyUploadDownloadPair()
		{
		}
};
typedef CFlyUploadDownloadPair<double> CFlyGlobalRatioItem;
typedef boost::unordered_map<unsigned long, CFlyUploadDownloadPair<uint64_t> > CFlyUploadDownloadMap; // TODO кей boost::asio::ip::address_v4
struct CFlyRatioItem : public CFlyUploadDownloadPair<uint64_t>
{
	boost::asio::ip::address_v4 m_last_ip_sql;
	CFlyRatioItem()
	{
	}
	~CFlyRatioItem()
	{
	}
};
class User;
struct CFlyUserRatioInfo : public CFlyRatioItem
#ifdef _DEBUG
		, boost::noncopyable , virtual NonDerivable<CFlyUserRatioInfo>
#endif
{
	private:
		static FastCriticalSection g_cs;
		User*  m_user;
	public:
		CFlyUploadDownloadMap* m_ip_map_ptr;
		CFlyUploadDownloadPair<uint64_t>& find_ip_map(const string& p_ip)
		{
			if (!m_ip_map_ptr)
			{
				m_ip_map_ptr = new CFlyUploadDownloadMap;
			}
			boost::system::error_code ec;
			const auto l_ip = boost::asio::ip::address_v4::from_string(p_ip, ec);
			dcassert(!ec);
			return (*m_ip_map_ptr)[l_ip.to_ulong()];
		}
		
		CFlyUserRatioInfo(User* p_user);
		~CFlyUserRatioInfo();
		
		bool try_load_ratio(bool p_is_create, const boost::asio::ip::address_v4& p_last_ip);
		void addUpload(const string& p_ip, uint64_t p_size);
		void addDownload(const string& p_ip, uint64_t p_size);
		void flushRatio();
		void setDirty(bool p_value)
		{
			m_is_dirty = p_value;
		}
	private:
		bool      m_is_sql_record_exists; 
		bool      m_is_dirty;
};
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO


#endif

