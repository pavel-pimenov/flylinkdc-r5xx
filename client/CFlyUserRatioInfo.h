#ifndef CFlyUserRatioInfo_H
#define CFlyUserRatioInfo_H

#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include "CFlyThread.h"

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
	uint32_t m_message_count;
	CFlyRatioItem(): m_message_count(0)
	{
	}
	~CFlyRatioItem()
	{
	}
};
class User;
struct CFlyUserRatioInfo : public CFlyRatioItem
#ifdef _DEBUG
		, boost::noncopyable
#endif
{
	public:
		CFlyUploadDownloadPair<uint64_t>& find_ip_map(const boost::asio::ip::address_v4& p_ip)
		{
			if (!m_ip_map_ptr)
			{
				m_ip_map_ptr = new CFlyUploadDownloadMap;
			}
			return (*m_ip_map_ptr)[p_ip.to_ulong()];
		}
		
		CFlyUserRatioInfo(User* p_user);
		~CFlyUserRatioInfo();
		
		bool try_load_ratio(const boost::asio::ip::address_v4& p_last_ip_from_sql);
		void addUpload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size);
		void addDownload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size);
		void incMessagesCount();
		void flushRatioL();
		void setDirty(bool p_value)
		{
			m_is_dirty = p_value;
		}
	private:
		CFlyUploadDownloadMap* m_ip_map_ptr;
		User*  m_user;
		bool      m_is_dirty;
};
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO


#endif

