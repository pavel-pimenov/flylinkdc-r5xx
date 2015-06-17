#ifndef CFlyUserRatioInfo_H
#define CFlyUserRatioInfo_H

#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include "CFlyThread.h"

#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
template <class T> class CFlyUploadDownloadPair
{
	private:
		bool m_is_dirty;
		T  m_upload;
		T  m_download;
	public:
		CFlyUploadDownloadPair() : m_upload(0), m_download(0), m_is_dirty(false)
		{
		}
		~CFlyUploadDownloadPair()
		{
		}
		bool is_dirty() const
		{
			return m_is_dirty;
		}
		void set_dirty(bool p_value)
		{
			m_is_dirty = p_value;
		}
		T get_upload() const
		{
			return m_upload;
		}
		T get_download() const
		{
			return m_download;
		}
		void add_upload(T p_size)
		{
			m_upload += p_size;
			if (p_size)
				m_is_dirty = true;
		}
		void add_download(T p_size)
		{
			m_download += p_size;
			if (p_size)
				m_is_dirty = true;
		}
		void set_upload(T p_size)
		{
			if (m_upload != p_size)
				m_is_dirty = true;
			m_upload = p_size;
		}
		void set_download(T p_size)
		{
			if (m_download != p_size)
				m_is_dirty = true;
			m_download = p_size;
		}
		void reset_dirty()
		{
			m_is_dirty = false;
		}
		T get_ratio() const
		{
			if (m_download > 0)
				return m_upload / m_download;
			else
				return 0;
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
		
		explicit CFlyUserRatioInfo(User* p_user);
		~CFlyUserRatioInfo();
		
		bool try_load_ratio(const boost::asio::ip::address_v4& p_last_ip_from_sql);
		void addUpload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size);
		void addDownload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size);
		void incMessagesCount();
		void flushRatioL();
	private:
		CFlyUploadDownloadMap* m_ip_map_ptr;
		User*  m_user;
};
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO


#endif

