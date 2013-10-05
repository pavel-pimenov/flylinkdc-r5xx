#ifndef CFlyUserRatioInfo_H
#define CFlyUserRatioInfo_H

#include "Thread.h"

#pragma once

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
typedef unordered_map<string, CFlyUploadDownloadPair<uint64_t> > CFlyUploadDownloadMap;
struct CFlyRatioItem : public CFlyUploadDownloadPair<uint64_t>
{
	string m_last_ip_sql;
	CFlyRatioItem() 
	{
	}
	~CFlyRatioItem()
	{
	}
};
struct CFlyUserRatioInfo : public CFlyRatioItem
#ifdef _DEBUG
		, boost::noncopyable , virtual NonDerivable<CFlyUserRatioInfo>
#endif
{
	private:
		static FastCriticalSection g_cs;
	public:
		uint32_t	   m_hub_id;
		const string&  m_nick_ref;
		string&		   m_last_ip_ref;

		CFlyUploadDownloadMap m_upload_download_map;
		
		CFlyUserRatioInfo(const string& p_nick, string& p_last_ip, uint32_t p_hub_id);
		~CFlyUserRatioInfo();
		
		bool try_load_ratio(bool p_is_create, const string& p_last_ip);
		void addUpload(const string& p_ip, uint64_t p_size);
		void addDownload(const string& p_ip, uint64_t p_size);
		void flushRatio();
		void setDitry(bool p_value);
	private:
		bool      m_is_sql_record_exists; // Флаг наличия записи в базе данных
		bool      m_is_ditry;
		void      update_last_ip(const string& p_ip);
};
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO


#endif

