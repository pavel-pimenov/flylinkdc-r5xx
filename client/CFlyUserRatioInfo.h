#ifndef CFlyUserRatioInfo_H
#define CFlyUserRatioInfo_H

#pragma once

#include <unordered_map>
#include <boost/asio/ip/address_v4.hpp>
#include "CFlyThread.h"

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
template <class T> class CFlyUploadDownloadPair
{
	private:
		T  m_upload;
		T  m_download;
		bool m_is_dirty;
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
		const T& get_upload() const
		{
			return m_upload;
		}
		const T& get_download() const
		{
			return m_download;
		}
		void add_upload(const T& p_size)
		{
			m_upload += p_size;
			if (p_size)
				m_is_dirty = true;
		}
		void add_download(const T& p_size)
		{
			m_download += p_size;
			if (p_size)
				m_is_dirty = true;
		}
		void set_upload(const T& p_size)
		{
			if (m_upload != p_size)
				m_is_dirty = true;
			m_upload = p_size;
		}
		void set_download(const T& p_size)
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
template <class T> class CFlyDirtyValue
{
	private:
		bool m_is_dirty;
		T m_value;
	public:
		CFlyDirtyValue(const T& p_value = T()) : m_value(p_value), m_is_dirty(false)
		{
		}
		const T& get() const
		{
			return m_value;
		}
		bool set(const T& p_value)
		{
			if (p_value != m_value)
			{
				m_value = p_value;
				m_is_dirty = true;
				return true;
			}
			return false;
		}
		bool is_dirty() const
		{
			return m_is_dirty;
		}
		void reset_dirty()
		{
			m_is_dirty = false;
		}
};
typedef CFlyUploadDownloadPair<double> CFlyGlobalRatioItem;
typedef std::unordered_map<unsigned long, CFlyUploadDownloadPair<uint64_t> > CFlyUploadDownloadMap; // TODO кей boost::asio::ip::address_v4
class CFlyRatioItem : public CFlyUploadDownloadPair<uint64_t>
{
	public:
		CFlyRatioItem()
		{
		}
		~CFlyRatioItem()
		{
		}
};
class User;
struct CFlyUserRatioInfo : public CFlyRatioItem
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
		
		bool tryLoadRatio(const boost::asio::ip::address_v4& p_last_ip_from_sql);
		void addUpload(const boost::asio::ip::address_v4& p_ip, const uint64_t& p_size)
		{
			if (p_size)
			{
				add_upload(p_size);
				if (!m_ip.is_unspecified())
				{
					if (m_ip != p_ip)
					{
						find_ip_map(p_ip).add_upload(p_size);
					}
				}
				else
				{
					m_ip = p_ip;
				}
			}
		}
		void addDownload(const boost::asio::ip::address_v4& p_ip, const uint64_t& p_size)
		{
			if (p_size)
			{
				add_download(p_size);
				if (!m_ip.is_unspecified())
				{
					if (m_ip != p_ip)
					{
						find_ip_map(p_ip).add_download(p_size);
					}
				}
				else
				{
					m_ip = p_ip;
				}
			}
		}
		void resetDirty()
		{
			reset_dirty();
			if (m_ip_map_ptr)
			{
				for (auto i = m_ip_map_ptr->begin(); i != m_ip_map_ptr->end(); ++i)
				{
					i->second.reset_dirty();
				}
			}
		}
		bool flushRatioL();
		CFlyUploadDownloadMap* getUploadDownloadMap() const
		{
			return m_ip_map_ptr;
		}
		boost::asio::ip::address_v4 m_ip;
	private:
		CFlyUploadDownloadMap* m_ip_map_ptr;
		User*  m_user;
};
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO


#endif

