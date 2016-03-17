
#pragma once

#ifndef DCPLUSPLUS_DCPP_DOWNLOAD_H_
#define DCPLUSPLUS_DCPP_DOWNLOAD_H_

#include "noexcept.h"
#include "Transfer.h"
#include "Streams.h"

/**
 * Comes as an argument in the DownloadManagerListener functions.
 * Use it to retrieve information about the ongoing transfer.
 */
class AdcCommand;
class Download : public Transfer, public Flags
{
	public:
	
		enum
		{
			FLAG_ZDOWNLOAD          = 0x01,
			FLAG_CHUNKED            = 0x02,
			FLAG_TTH_CHECK          = 0x04, //-V112
			FLAG_SLOWUSER           = 0x08,
			FLAG_XML_BZ_LIST        = 0x10,
			FLAG_DOWNLOAD_PARTIAL   = 0x20, //-V112
			FLAG_OVERLAP            = 0x40,
#ifdef IRAINMAN_INCLUDE_USER_CHECK
			FLAG_USER_CHECK     = 0x80,
#endif
			FLAG_USER_GET_IP    = 0x200     // [+] SSA
		};
		
		explicit Download(UserConnection* p_conn, const QueueItemPtr& p_item, const string& p_ip, const string& p_chiper_name) noexcept; // [!] IRainman fix.
		
		void getParams(StringMap& params) const;
		
		~Download();
		
		/** @return Target filename without path. */
		string getTargetFileName() const
		{
			return Util::getFileName(getPath());
		}
		
		/** @internal */
		const string getDownloadTarget() const
		{
			const auto l_tmp_target = getTempTarget();
			if (l_tmp_target.empty())
				return getPath();
			else
				return l_tmp_target;
		}
		
		/** @internal */
		TigerTree& getTigerTree()
		{
			return m_tiger_tree;
		}
		const TigerTree& getTigerTree() const
		{
			return m_tiger_tree;
		}
		string& getPFS()
		{
			return m_pfs;
		}
		/** @internal */
		void getCommand(AdcCommand& p_cmd, bool zlib) const;
		
		string getTempTarget() const;
		// [~] IRainman fix.
#ifdef PPA_INCLUDE_DROP_SLOW
		GETSET(uint64_t, m_lastNormalSpeed, LastNormalSpeed);
#endif
		void setDownloadFile(OutputStream* p_file)
		{
			m_download_file = p_file;
		}
		OutputStream* getDownloadFile()
		{
			return m_download_file;
		}
		GETSET(bool, treeValid, TreeValid);
		void reset_download_file()
		{
			safe_delete(m_download_file);
		}
		//string     m_reason;
	private:
		OutputStream* m_download_file;
		const QueueItemPtr m_qi;
		TigerTree  m_tiger_tree;
		string     m_pfs;
};

typedef std::shared_ptr<Download> DownloadPtr;
#endif /*DOWNLOAD_H_*/
