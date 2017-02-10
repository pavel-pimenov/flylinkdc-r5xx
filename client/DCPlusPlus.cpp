/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "CryptoManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "LogManager.h"
#include "FinishedManager.h"
#include "ADLSearch.h"
#include "MappingManager.h"
#include "ConnectivityManager.h"
#include "UserManager.h"
#include "WebServerManager.h"
#include "ThrottleManager.h"
#include "GPGPUManager.h"

#include "CFlylinkDBManager.h"
#include "../FlyFeatures/flyServer.h"
#include "syslog/syslog.h"
#include "../windows/ToolbarManager.h"

#include "IpGuard.h"
#include "PGLoader.h"
#ifdef SSA_IPGRANT_FEATURE
#include "IPGrant.h"
#endif // SSA_IPGRANT_FEATURE

#ifdef STRONG_USE_DHT
#include "../dht/DHT.h"
#endif


#ifdef USE_FLYLINKDC_VLD
#include "C:\Program Files (x86)\Visual Leak Detector\include\vld.h" // VLD качать тут http://vld.codeplex.com/
#endif

#ifndef _DEBUG
#include "../doctor-dump/CrashRpt.h"
CFlyCrashReportMarker::CFlyCrashReportMarker(const TCHAR* p_value)
{
	extern crash_rpt::CrashRpt g_crashRpt;
	g_crashRpt.SetCustomInfo(p_value);
}
CFlyCrashReportMarker::~CFlyCrashReportMarker()
{
	extern crash_rpt::CrashRpt g_crashRpt;
	g_crashRpt.SetCustomInfo(_T(""));
}
#endif // _DEBUG

void startup(PROGRESSCALLBACKPROC pProgressCallbackProc, void* pProgressParam, GUIINITPROC pGuiInitProc, void *pGuiParam)
{
	CFlyCrashReportMarker l_marker(_T("StartCore"));
	
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	CFlyTickDelta l_delta(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE]);
#endif
	
	WSADATA wsaData = {0};
	uint8_t i = 0;
	do
	{
		if (WSAStartup(MAKEWORD(2, 2), &wsaData))
			i++;
		else
			break;
	}
	while (i < 6);
#ifdef FLYLINKDC_USE_SYSLOG
	syslog_loghost("syslog.fly-server.ru");
	openlog("flylinkdc", 0 , LOG_USER | LOG_INFO);
#endif
	
	CFlyLog l_StartUpLog("[StartUp]");
	
	// [+] IRainman fix.
#define LOAD_STEP(component_name, load_function)\
	{\
		pProgressCallbackProc(pProgressParam, _T(component_name));\
		const string l_componentName(component_name);\
		l_StartUpLog.loadStep(l_componentName);\
		load_function;\
		l_StartUpLog.loadStep(l_componentName, false);\
	}
	
#define LOAD_STEP_L(locale_key, load_function)\
	{\
		pProgressCallbackProc(pProgressParam, TSTRING(locale_key));\
		const auto& l_componentName = STRING(locale_key);\
		l_StartUpLog.loadStep(l_componentName);\
		load_function;\
		l_StartUpLog.loadStep(l_componentName, false);\
	}
	// [~] IRainman fix.
	
	dcassert(pProgressCallbackProc != nullptr);
	
	// Загрузку конфига нужно делать раньше
	// LOAD_STEP("Fly server", g_fly_server_config.loadConfig());
	
	LOAD_STEP("SQLite database init... Please wait!!!", CFlylinkDBManager::newInstance());
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
#ifdef _DEBUG
	CFlyServerConfig::SyncAntivirusDBSafe();
#endif
	LOAD_STEP("Antivirus DB", CFlylinkDBManager::getInstance()->load_avdb());
#endif
	
#ifdef FLYLINKDC_USE_GEO_IP
	LOAD_STEP("Geo IP", Util::loadGeoIp());
#endif
	LOAD_STEP("P2P Guard", Util::loadP2PGuard()); // Этот грузить всегда первым - выполняет зачистку базы
	LOAD_STEP("iblocklist.com", Util::loadIBlockList());
	
	LOAD_STEP("Custom Locations", Util::loadCustomlocations());
	
#ifdef FLYLINKDC_USE_GPU_TTH
	LOAD_STEP("TTH on GPU", GPGPUTTHManager::newInstance());
#endif
	HashManager::newInstance();
#ifdef USE_FLYLINKDC_VLD
	VLDDisable(); // TODO VLD показывает там лики - не понял пока как победить OpenSSL
#endif
// FLYLINKDC_CRYPTO_DISABLE
	LOAD_STEP("SSL", CryptoManager::newInstance());
#ifdef USE_FLYLINKDC_VLD
	VLDEnable(); // TODO VLD показывает там лики - не понял пока как победить OpenSSL
#endif
	SearchManager::newInstance();
	ConnectionManager::newInstance();
	DownloadManager::newInstance();
	UploadManager::newInstance();
	
#ifdef STRONG_USE_DHT
	LOAD_STEP("DHT", dht::DHT::newInstance());
#endif
	LOAD_STEP("Ensure list path", QueueManager::newInstance());
	LOAD_STEP("Create empty share", ShareManager::newInstance());
	LOAD_STEP("Ensure fav path", FavoriteManager::newInstance());
	LOAD_STEP("Ignore list", UserManager::newInstance()); // [+] IRainman core
	//HistoryManager::newInstance();//[-] FlylinkDC this functional released in DB Manager
	if (pGuiInitProc)
	{
		LOAD_STEP("Gui and FlyFeatures", pGuiInitProc(pGuiParam));
	}
	LOAD_STEP_L(SETTINGS, SettingsManager::getInstance()->loadOtherSettings());
	LOAD_STEP("IPGuard.xml", IpGuard::load());
	LOAD_STEP("IPTrus.ini", PGLoader::load());
#ifdef SSA_IPGRANT_FEATURE
	LOAD_STEP("IPGrant.ini", IpGrant::load());
#endif
	
	FinishedManager::newInstance();
	LOAD_STEP("ADLSearch", ADLSearchManager::newInstance());
	ConnectivityManager::newInstance();
	MappingManager::newInstance();
	//DebugManager::newInstance(); [-] IRainman opt.
	
	LOAD_STEP_L(FAVORITE_HUBS, FavoriteManager::load());
	
// FLYLINKDC_CRYPTO_DISABLE
	LOAD_STEP_L(CERTIFICATES, CryptoManager::getInstance()->loadCertificates());
	
	LOAD_STEP_L(WAITING_USERS, UploadManager::getInstance()->load()); // !SMT!-S
	
	WebServerManager::newInstance();
	
	LOAD_STEP_L(HASH_DATABASE, HashManager::getInstance()->startup());
	
	LOAD_STEP_L(SHARED_FILES, ShareManager::getInstance()->refresh_share(true, false));
	
	LOAD_STEP_L(DOWNLOAD_QUEUE, QueueManager::getInstance()->loadQueue());
#ifdef IRAINMAN_USE_STRING_POOL
	StringPool::newInstance(); // [+] IRainman opt.
#endif
	
#undef LOAD_STEP
#undef LOAD_STEP_L
}

void preparingCoreToShutdown() // [+] IRainamn fix.
{
	static bool g_is_first = false;
	if (!g_is_first)
	{
		g_is_first = true;
		CFlyLog l_log("[Core shutdown]");
		TimerManager::getInstance()->shutdown();
		ClientManager::shutdown();
#ifdef STRONG_USE_DHT
		dht::BootstrapManager::shutdown_http();
#endif
		UploadManager::shutdown();
		WebServerManager::getInstance()->shutdown();
		HashManager::getInstance()->shutdown();
		ClientManager::getInstance()->prepareClose();
		FavoriteManager::getInstance()->prepareClose();
		ShareManager::getInstance()->shutdown();
		QueueManager::getInstance()->shutdown();
		SearchManager::getInstance()->disconnect();
		ClientManager::getInstance()->clear();
		CFlylinkDBManager::getInstance()->flush();
	}
}

void shutdown(GUIINITPROC pGuiInitProc, void *pGuiParam)
{
	// Сохраним маркеры времени завершения
#ifndef _DEBUG
	extern crash_rpt::CrashRpt g_crashRpt;
	g_crashRpt.SetCustomInfo(_T("StopCore"));
#endif
	
	{
#ifdef FLYLINKDC_COLLECT_UNKNOWN_TAG
		string l_debugTag;
		{
			CFlyFastLock(NmdcSupports::g_debugCsUnknownNmdcTagParam);
			//dcassert(NmdcSupports::g_debugUnknownNmdcTagParam.empty());
			const auto& l_debugUnknownNmdcTagParam = NmdcSupports::g_debugUnknownNmdcTagParam;
			for (auto i = l_debugUnknownNmdcTagParam.begin(); i != l_debugUnknownNmdcTagParam.end(); ++i)
			{
				l_debugTag += i->first + "(" + Util::toString(i->second) + ")" + ',';
			}
			NmdcSupports::g_debugUnknownNmdcTagParam.clear();
		}
		if (!l_debugTag.empty())
		{
			LogManager::message("Founded unknown NMDC tag param: " + l_debugTag);
		}
#endif
		
#ifdef FLYLINKDC_COLLECT_UNKNOWN_FEATURES
		// [!] IRainman fix: supports cleanup.
		string l_debugFeatures;
		string l_debugConnections;
		{
			CFlyFastLock(AdcSupports::g_debugCsUnknownAdcFeatures);
			// dcassert(AdcSupports::g_debugUnknownAdcFeatures.empty());
			const auto& l_debugUnknownFeatures = AdcSupports::g_debugUnknownAdcFeatures;
			for (auto i = l_debugUnknownFeatures.begin(); i != l_debugUnknownFeatures.end(); ++i)
			{
				l_debugFeatures += i->first + "[" + i->second + "]" + ',';
			}
			AdcSupports::g_debugUnknownAdcFeatures.clear();
		}
		{
			CFlyFastLock(NmdcSupports::g_debugCsUnknownNmdcConnection);
			dcassert(NmdcSupports::g_debugUnknownNmdcConnection.empty());
			const auto& l_debugUnknownConnections = NmdcSupports::g_debugUnknownNmdcConnection;
			for (auto i = l_debugUnknownConnections.begin(); i != l_debugUnknownConnections.end(); ++i)
			{
				l_debugConnections += *i + ',';
			}
			NmdcSupports::g_debugUnknownNmdcConnection.clear();
		}
		if (!l_debugFeatures.empty())
		{
			LogManager::message("Founded unknown ADC supports: " + l_debugFeatures);
		}
		if (!l_debugConnections.empty())
		{
			LogManager::message("Founded unknown NMDC connections: " + l_debugConnections);
		}
#endif // FLYLINKDC_COLLECT_UNKNOWN_FEATURES
		
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
		CFlyTickDelta l_delta(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_SHUTDOWN_CORE]);
#endif
#ifdef _DEBUG
		dcdebug("shutdown start - User::g_user_counts = %d OnlineUser::g_online_user_counts = %d\n", int(User::g_user_counts), int(OnlineUser::g_online_user_counts));
#endif
#ifdef STRONG_USE_DHT
		dht::DHT::getInstance()->stop(true); // [+] IRainman fix.
#ifdef _DEBUG
		dcdebug("shutdown (after closing last hub (DHT::stop) - User::g_user_counts = %d OnlineUser::g_online_user_counts = %d\n", int(User::g_user_counts), int(OnlineUser::g_online_user_counts)); // [+] IRainman fix.
#endif
#endif
#ifdef FLYLINKDC_USE_TORRENT
		DownloadManager::getInstance()->shutdown_torrent();
#endif
		QueueManager::getInstance()->saveQueue(true);
		SettingsManager::getInstance()->save();
		ConnectionManager::getInstance()->shutdown();
		MappingManager::getInstance()->close();
		
		preparingCoreToShutdown(); // Зовем тут второй раз т.к. вероятно при автообновлении оно не зовется.
		
#ifdef FLYLINKDC_USE_DNS
		Socket::dnsCache.waitShutdown(); // !SMT!-IP
#endif
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
		BufferedSocket::waitShutdown();
#endif
		
#ifdef IRAINMAN_USE_STRING_POOL
		StringPool::deleteInstance(); // [+] IRainman opt.
#endif
		MappingManager::deleteInstance();
		ConnectivityManager::deleteInstance();
		WebServerManager::deleteInstance();
		if (pGuiInitProc)
		{
			pGuiInitProc(pGuiParam);
		}
		ADLSearchManager::deleteInstance();
		FinishedManager::deleteInstance();
		ShareManager::deleteInstance();
#ifdef STRONG_USE_DHT
		dht::DHT::deleteInstance();
#endif
#ifdef USE_FLYLINKDC_VLD
		VLDDisable(); // TODO VLD показывает там лики - не понял пока как победить OpenSSL
#endif
// FLYLINKDC_CRYPTO_DISABLE
		CryptoManager::deleteInstance();
#ifdef USE_FLYLINKDC_VLD
		VLDEnable(); // TODO VLD показывает там лики - не понял пока как победить OpenSSL
#endif
		ThrottleManager::deleteInstance();
		DownloadManager::deleteInstance();
		UploadManager::deleteInstance();
		QueueManager::deleteInstance();
		ConnectionManager::deleteInstance();
		SearchManager::deleteInstance();
		UserManager::deleteInstance(); // [+] IRainman core
		FavoriteManager::deleteInstance();
		ClientManager::deleteInstance();
		HashManager::deleteInstance();
#ifdef FLYLINKDC_USE_GPU_TTH
		GPGPUTTHManager::deleteInstance();
#endif // FLYLINKDC_USE_GPU_TTH
		
		CFlylinkDBManager::deleteInstance();
		CFlylinkDBManager::shutdown_engine();
		TimerManager::deleteInstance();
		SettingsManager::deleteInstance();
		ToolbarManager::shutdown();
		extern SettingsManager* g_settings;
		g_settings = nullptr;
		
#ifdef FLYLINKDC_USE_SYSLOG
		closelog();
#endif
		
		::WSACleanup();
#ifdef _DEBUG
		dcdebug("shutdown end - User::g_user_counts = %d OnlineUser::g_online_user_counts = %d\n", int(User::g_user_counts), int(OnlineUser::g_online_user_counts));
		//dcassert(User::g_user_counts == 2);
		// ClientManager::g_uflylinkdc and ClientManager::g_me destroyed only with the full completion of the program, all the other user must be destroyed already by this time.
		dcassert(OnlineUser::g_online_user_counts == 0);
		dcassert(UploadQueueItem::g_upload_queue_item_count == 0);
		dcdebug("shutdown start - UploadQueueItem::g_upload_queue_item_count = %d \n", int(UploadQueueItem::g_upload_queue_item_count));
#endif
		
		// [~] IRainman fix.
	}
	
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	g_fly_server_stat.saveShutdownMarkers();
#endif
}

/**
 * @file
 * $Id: DCPlusPlus.cpp 569 2011-07-25 19:48:51Z bigmuscle $
 */
