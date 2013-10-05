#ifndef PGLOADER_H
#define PGLOADER_H

#include "Singleton.h"
#include "SettingsManager.h"

#ifdef PPA_INCLUDE_IPFILTER

#include "iplist.h"

class PGLoader : public Singleton<PGLoader>, private SettingsManagerListener
{
	public:
		PGLoader()
		{
			SettingsManager::getInstance()->addListener(this);
		}
		
		~PGLoader()
		{
			SettingsManager::getInstance()->removeListener(this);
		}
		bool check(const string& p_IP);
		void addLine(string& p_Line, CFlyLog& p_log);
		void load(const string& p_data = Util::emptyString);
		static string getConfigFileName()
		{
			return Util::getConfigPath() + "IPTrust.ini";
		}
	private:
		FastCriticalSection m_cs; // [!] IRainman opt: use spin lock here.
		IPList  m_ipList;
		IPList  m_ipListBlock;
		// SettingsManagerListener
		void on(SettingsManagerListener::Load, SimpleXML& /*xml*/) noexcept
		{
			load();
		}
};
#endif // PPA_INCLUDE_IPFILTER

#endif // PGLOADER_H
