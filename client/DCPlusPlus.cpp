/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
#include "ClientProfileManager.h"
#endif
#include "DetectionManager.h"
#include "WebServerManager.h"
#include "ThrottleManager.h"

#include "CFlylinkDBManager.h"
#include "../FlyFeatures/flyServer.h"

#ifdef PPA_INCLUDE_IPGUARD
#include "IpGuard.h"
#endif // PPA_INCLUDE_IPGUARD
#include "PGLoader.h"
#ifdef SSA_IPGRANT_FEATURE
#include "IPGrant.h"
#endif // SSA_IPGRANT_FEATURE
// #include "HistoryManager.h" //[-] FlylinkDC this functional released in DB Manager

#ifdef STRONG_USE_DHT
#include "../dht/DHT.h"
#endif

#ifdef USE_FLYLINKDC_VLD
#include "C:\Program Files (x86)\Visual Leak Detector\include\vld.h" // VLD качать тут http://vld.codeplex.com/
#endif

void startup(PROGRESSCALLBACKPROC pProgressCallbackProc, void* pProgressParam, GUIINITPROC pGuiInitProc, void *pGuiParam)
{
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	CFlyTickDelta l_delta(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE]);
#endif
	
	// "Dedicated to the near-memory of Nev. Let's start remembering people while they're still alive."
	// Nev's great contribution to dc++
	while (1) break;
	
#ifdef _WIN32
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
	
	LOAD_STEP("Fly server", g_fly_server_config.loadConfig());
	
	LOAD_STEP("Geo IP", Util::loadGeoIp());
	
	LOAD_STEP("Custom Locations", Util::loadCustomlocations());
	
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
	LOAD_STEP("Ensure fav patch", FavoriteManager::newInstance());
	LOAD_STEP("Ignore list", UserManager::newInstance()); // [+] IRainman core
#ifdef SSA_IPGRANT_FEATURE
	IpGrant::newInstance();
#endif
	//HistoryManager::newInstance();//[-] FlylinkDC this functional released in DB Manager
#ifdef PPA_INCLUDE_IPGUARD
	IpGuard::newInstance();
#endif
#ifdef PPA_INCLUDE_IPFILTER
	PGLoader::newInstance();
#endif
	if (pGuiInitProc)
	{
		LOAD_STEP("Gui and FlyFeatures", pGuiInitProc(pGuiParam));
	}
	
	LOAD_STEP_L(SETTINGS, SettingsManager::getInstance()->loadOtherSettings());
	
	FinishedManager::newInstance();
	LOAD_STEP("ADLSearch", ADLSearchManager::newInstance());
	ConnectivityManager::newInstance();
	MappingManager::newInstance();
	//DebugManager::newInstance(); [-] IRainman opt.
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
	DetectionManager::newInstance();
#endif
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
	ClientProfileManager::newInstance();
#endif
	
	LOAD_STEP_L(FAVORITE_HUBS, FavoriteManager::getInstance()->load());
	
// FLYLINKDC_CRYPTO_DISABLE
	LOAD_STEP_L(CERTIFICATES, CryptoManager::getInstance()->loadCertificates());
	
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
TODO:
	please use LOAD_STEP_L
	DetectionManager::getInstance()->load();
#endif
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
TODO:
	please use LOAD_STEP_L
	ClientProfileManager::getInstance()->load();
#endif
	LOAD_STEP_L(WAITING_USERS, UploadManager::getInstance()->load()); // !SMT!-S
//  UploadManager::getInstance()->load(false);//[+]necros

	WebServerManager::newInstance();
	
	LOAD_STEP_L(HASH_DATABASE, HashManager::getInstance()->startup());
	
	LOAD_STEP_L(SHARED_FILES, ShareManager::getInstance()->refresh(true, false, true));
	
	LOAD_STEP_L(DOWNLOAD_QUEUE, QueueManager::getInstance()->loadQueue());
#ifdef IRAINMAN_USE_STRING_POOL
	StringPool::newInstance(); // [+] IRainman opt.
#endif
	
#undef LOAD_STEP
#undef LOAD_STEP_L
}

void preparingCoreToShutdown() // [+] IRainamn fix.
{
	FavoriteManager::getInstance()->prepareClose();
	ClientManager::getInstance()->shutdown();
	TimerManager::getInstance()->shutdown();
	HashManager::getInstance()->shutdown();
	ShareManager::getInstance()->shutdown();
	QueueManager::getInstance()->shutdown();
	SearchManager::getInstance()->disconnect();
	ClientManager::getInstance()->clear();
}

void shutdown(GUIINITPROC pGuiInitProc, void *pGuiParam, bool p_exp /*= false*/)
{
	// Сохраним маркеры времени завершения
	{
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
		CFlyTickDelta l_delta(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_SHUTDOWN_CORE]);
#endif
#ifdef _DEBUG
		dcdebug("shutdown start - User::g_user_counts = %d OnlineUser::g_online_user_counts = %d\n", int(User::g_user_counts), int(OnlineUser::g_online_user_counts)); // [!] IRainman fix: Issue 1037 [BUG] иногда теряем объект User? https://code.google.com/p/flylinkdc/issues/detail?id=1037
#endif
#ifdef STRONG_USE_DHT
		dht::DHT::getInstance()->stop(true); // [+] IRainman fix.
#ifdef _DEBUG
		dcdebug("shutdown (after closing last hub (DHT::stop) - User::g_user_counts = %d OnlineUser::g_online_user_counts = %d\n", int(User::g_user_counts), int(OnlineUser::g_online_user_counts)); // [+] IRainman fix.
#endif
#endif
		// [!] IRainman fix: see preparingShutdown().
		ConnectionManager::getInstance()->shutdown();
		MappingManager::getInstance()->close();
		
#ifdef PPA_INCLUDE_DNS
		Socket::dnsCache.waitShutdown(); // !SMT!-IP
#endif
		if (!p_exp)
		{
			BufferedSocket::waitShutdown();
		}
		
		QueueManager::getInstance()->saveQueue(true);
		SettingsManager::getInstance()->save();
#ifdef IRAINMAN_USE_STRING_POOL
		StringPool::deleteInstance(); // [+] IRainman opt.
#endif
		MappingManager::deleteInstance();
		ConnectivityManager::deleteInstance();
		WebServerManager::deleteInstance();
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
		ClientProfileManager::deleteInstance();
#endif
#ifdef PPA_INCLUDE_IPGUARD
		IpGuard::deleteInstance();
#endif
		//ToolbarManager::deleteInstance();  // moved to windows\main.cpp. This is Window related object!!!
		//PopupManager::deleteInstance();    // moved to windows\main.cpp. This is Window related object!!!
		//HistoryManager::deleteInstance();//[-] FlylinkDC this functional released in DB Manager
		if (pGuiInitProc)
			pGuiInitProc(pGuiParam);
		ADLSearchManager::deleteInstance();
		FinishedManager::deleteInstance();
		ShareManager::deleteInstance(); // 2012-04-29_06-52-32_7DCSOGEGBL7SCFPXL7QNF2EUVF3XY22Y6PVKEWQ_4F9F3ED2_crash-stack-r502-beta23-build-9860.dmp
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
#ifdef SSA_IPGRANT_FEATURE
		IpGrant::deleteInstance();
#endif
#ifdef PPA_INCLUDE_IPFILTER
		PGLoader::deleteInstance();
#endif
		UserManager::deleteInstance(); // [+] IRainman core
		FavoriteManager::deleteInstance();
		ClientManager::deleteInstance();
		HashManager::deleteInstance();
		CFlylinkDBManager::deleteInstance(); // fix http://code.google.com/p/flylinkdc/issues/detail?id=1355
		LogManager::deleteInstance();
		SettingsManager::deleteInstance();
		TimerManager::deleteInstance();
		//DebugManager::deleteInstance(); [-] IRainman opt.
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
		DetectionManager::deleteInstance();
#endif
		// ResourceManager::deleteInstance(); [-] IRainman opt.
		
#ifdef _WIN32
		::WSACleanup();
#endif
		
		// [!] IRainman fix:
#ifdef _DEBUG
		// [!] IRainman fix: supports cleanup.
		string _debugFeatures;
		string _debugConnections;
		string _debugTag;
		{
			FastLock l(AdcSupports::g_debugCsUnknownAdcFeatures);
			const auto& _debugUnknownFeatures = AdcSupports::g_debugUnknownAdcFeatures;
			for (auto i = _debugUnknownFeatures.begin(); i != _debugUnknownFeatures.end(); ++i)
			{
				_debugFeatures += *i + ',';
			}
		}
		{
			FastLock l(NmdcSupports::g_debugCsUnknownNmdcConnection);
			const auto& _debugUnknownConnections = NmdcSupports::g_debugUnknownNmdcConnection;
			for (auto i = _debugUnknownConnections.begin(); i != _debugUnknownConnections.end(); ++i)
			{
				_debugConnections += *i + ',';
			}
		}
		{
			FastLock l(NmdcSupports::g_debugCsUnknownNmdcTagParam);
			const auto& _debugUnknownNmdcTagParam = NmdcSupports::g_debugUnknownNmdcTagParam;
			for (auto i = _debugUnknownNmdcTagParam.begin(); i != _debugUnknownNmdcTagParam.end(); ++i)
			{
				_debugTag += *i + ',';
			}
		}
		if (!_debugFeatures.empty())
		{
			dcdebug("Founded unknown ADC supports: %s\n", _debugFeatures.c_str());
		}
		if (!_debugConnections.empty())
		{
			dcdebug("Founded unknown NMDC connections: %s\n", _debugConnections.c_str());
		}
		if (!_debugTag.empty())
		{
			dcdebug("Founded unknown NMDC tag param: %s\n", _debugTag.c_str());
		}
#endif // _DEBUG
		
		// [!] IRainman fix: Issue 1037 [BUG] иногда теряем объект User? https://code.google.com/p/flylinkdc/issues/detail?id=1037
#ifdef _DEBUG
		dcdebug("shutdown end - User::g_user_counts = %d OnlineUser::g_online_user_counts = %d\n", int(User::g_user_counts), int(OnlineUser::g_online_user_counts));
		//dcassert(User::g_user_counts == 2); // ClientManager::g_uflylinkdc and ClientManager::g_me destroyed only with the full completion of the program, all the other user must be destroyed already by this time.
		//dcassert(OnlineUser::g_online_user_counts == 0);
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
