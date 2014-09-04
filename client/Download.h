#ifndef DCPLUSPLUS_DCPP_DOWNLOAD_H_
#define DCPLUSPLUS_DCPP_DOWNLOAD_H_

#include "noexcept.h"
#include "Transfer.h"
#include "Streams.h"
#include "QueueItem.h"

/**
 * Comes as an argument in the DownloadManagerListener functions.
 * Use it to retrieve information about the ongoing transfer.
 */
class Download : public Transfer, public Flags
#ifdef _DEBUG
	, virtual NonDerivable<Download> // [+] IRainman fix.
#endif
{
	public:
	
		enum
		{
			FLAG_ZDOWNLOAD          = 0x01,
			FLAG_CHUNKED            = 0x02,
			FLAG_TTH_CHECK          = 0x04, //-V112
			FLAG_SLOWUSER           = 0x08,
			FLAG_XML_BZ_LIST        = 0x10,
			FLAG_PARTIAL            = 0x20, //-V112
			FLAG_OVERLAP        = 0x40,
			FLAG_USER_CHECK     = 0x80,
			FLAG_USER_GET_IP    = 0x200     // [+] SSA
		};
		
		explicit Download(UserConnection* p_conn, QueueItem* item, const string& p_ip, const string& p_chiper_name) noexcept; // [!] IRainman fix.
		
		void getParams(const UserConnection* aSource, StringMap& params);
		
		~Download();
		
		/** @return Target filename without path. */
		string getTargetFileName() const
		{
			return Util::getFileName(getPath());
		}
		
		/** @internal */
		const string& getDownloadTarget() const
		{
			if (getTempTarget().empty())
				return getPath();
			else
				return getTempTarget();
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
		const string& getPFS() const
		{
			return m_pfs;
		}
		// [+] IRainman fix.
		/*const QueueItem* getQueueItem() const
		{
		    return qi;
		}*/
		// [+] IRainman fix.
		/** @internal */
		AdcCommand getCommand(bool zlib) const;
		
		// [!] IRainman fix.
		const string& getTempTarget() const // [+]
		{
			return qi->getTempTarget();
		}
		// [-] GETSET(string, tempTarget, TempTarget);
		// [~] IRainman fix.
#ifdef PPA_INCLUDE_DROP_SLOW
		GETSET(uint64_t, lastNormalSpeed, LastNormalSpeed);
#endif
		GETSET(OutputStream*, m_download_file, DownloadFile);
		GETSET(bool, treeValid, TreeValid);
	private:
		QueueItem* qi; // [+] IRainman fix.
		TigerTree  m_tiger_tree;
		string     m_pfs;
};

#endif /*DOWNLOAD_H_*/
