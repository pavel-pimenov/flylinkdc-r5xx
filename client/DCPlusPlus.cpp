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

#include "DCPlusPlus.h"
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "CryptoManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "FinishedManager.h"
#include "ADLSearch.h"
#include "MappingManager.h"
#include "ConnectivityManager.h"
#include "UserManager.h"
#include "ThrottleManager.h"
//#include "GPGPUManager.h"
#include "../FlyFeatures/flyServer.h"
#include "../windows/ToolbarManager.h"
//#include "syslog/syslog.h"

#include "IpGuard.h"
#include "PGLoader.h"
#ifdef SSA_IPGRANT_FEATURE
#include "IPGrant.h"
#endif // SSA_IPGRANT_FEATURE

#ifdef FLYLINKDC_USE_VLD
#include "C:\Program Files (x86)\Visual Leak Detector\include\vld.h" // VLD ������ ��� http://vld.codeplex.com/
#endif

#ifndef _DEBUG
#include "../doctor-dump/CrashRpt.h"
#endif // _DEBUG

void startup(PROGRESSCALLBACKPROC pProgressCallbackProc, void* pProgressParam, GUIINITPROC pGuiInitProc, void *pGuiParam)
{
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
	openlog("flylinkdc", 0, LOG_USER | LOG_INFO);
#endif
	
	CFlyLog l_StartUpLog("[StartUp]");
	
	
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
	
	
	dcassert(pProgressCallbackProc != nullptr);
	
	// �������� ������� ����� ������ ������
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
	LOAD_STEP("P2P Guard", Util::loadP2PGuard()); // ���� ������� ������ ������ - ��������� �������� ����
	LOAD_STEP("iblocklist.com", Util::loadIBlockList());
	
	LOAD_STEP("Custom Locations", Util::loadCustomlocations());
	
#ifdef FLYLINKDC_USE_GPU_TTH
	LOAD_STEP("TTH on GPU", GPGPUTTHManager::newInstance());
#endif
	HashManager::newInstance();
#ifdef FLYLINKDC_USE_VLD
	VLDDisable(); // TODO VLD ���������� ��� ���� - �� ����� ���� ��� �������� OpenSSL
#endif
// FLYLINKDC_CRYPTO_DISABLE
	LOAD_STEP("SSL", CryptoManager::newInstance());
#ifdef FLYLINKDC_USE_VLD
	VLDEnable(); // TODO VLD ���������� ��� ���� - �� ����� ���� ��� �������� OpenSSL
#endif
	SearchManager::newInstance();
	ConnectionManager::newInstance();
	DownloadManager::newInstance();
	UploadManager::newInstance();
	
	LOAD_STEP("Ensure list path", QueueManager::newInstance());
	LOAD_STEP("Create empty share", ShareManager::newInstance());
	LOAD_STEP("Ensure fav path", FavoriteManager::newInstance());
	LOAD_STEP("Ignore list", UserManager::newInstance());
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
	
	LOAD_STEP_L(FAVORITE_HUBS, FavoriteManager::load());
	
// FLYLINKDC_CRYPTO_DISABLE
	LOAD_STEP_L(CERTIFICATES, CryptoManager::getInstance()->loadCertificates());
	
	LOAD_STEP_L(WAITING_USERS, UploadManager::getInstance()->load());
	
	
	LOAD_STEP_L(HASH_DATABASE, HashManager::getInstance()->startup());
	
	LOAD_STEP_L(SHARED_FILES, ShareManager::getInstance()->refresh_share(true, false));
	
	
#undef LOAD_STEP
#undef LOAD_STEP_L
}

void preparingCoreToShutdown()
{
	static bool g_is_first = false;
	if (!g_is_first)
	{
		g_is_first = true;
		CFlyLog l_log("[Core shutdown]");
		ClientManager::shutdown();
		SearchManager::getInstance()->disconnect(true);
		HashManager::getInstance()->shutdown();
		TimerManager::getInstance()->shutdown();
		UploadManager::shutdown();
		ClientManager::prepareClose();
		FavoriteManager::getInstance()->prepareClose();
		ShareManager::getInstance()->shutdown();
		QueueManager::getInstance()->shutdown();
		ClientManager::clear();
		CFlylinkDBManager::getInstance()->flush();
	}
}

void shutdown(GUIINITPROC pGuiInitProc, void *pGuiParam)
{
	// �������� ������� ������� ����������
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
		string l_debugFeatures;
		string l_debugConnections;
		{
			CFlyFastLock(AdcSupports::g_debugCsUnknownAdcFeatures);
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
#ifdef FLYLINKDC_USE_TORRENT
		DownloadManager::getInstance()->shutdown_torrent();
#endif
		QueueManager::saveQueue(true);
		SettingsManager::getInstance()->save();
		ConnectionManager::getInstance()->shutdown();
		
		preparingCoreToShutdown(); // ����� ��� ������ ��� �.�. �������� ��� �������������� ��� �� �������.
		
#ifdef FLYLINKDC_USE_DNS
		Socket::dnsCache.waitShutdown();
#endif
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
		BufferedSocket::waitShutdown();
#endif
		
		ConnectivityManager::deleteInstance();
		if (pGuiInitProc)
		{
			pGuiInitProc(pGuiParam);
		}
		ADLSearchManager::deleteInstance();
		FinishedManager::deleteInstance();
		ShareManager::deleteInstance();
#ifdef FLYLINKDC_USE_VLD
		VLDDisable(); // TODO VLD ���������� ��� ���� - �� ����� ���� ��� �������� OpenSSL
#endif
// FLYLINKDC_CRYPTO_DISABLE
		CryptoManager::deleteInstance();
#ifdef FLYLINKDC_USE_VLD
		VLDEnable(); // TODO VLD ���������� ��� ���� - �� ����� ���� ��� �������� OpenSSL
#endif
		ThrottleManager::deleteInstance();
		DownloadManager::deleteInstance();
		UploadManager::deleteInstance();
		QueueManager::deleteInstance();
		ConnectionManager::deleteInstance();
		SearchManager::deleteInstance();
		UserManager::deleteInstance();
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
		dcassert(OnlineUser::g_online_user_counts == 0);
#endif
		
		
	}
	
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	g_fly_server_stat.saveShutdownMarkers();
#endif
}

/**
 * @file
 * $Id: DCPlusPlus.cpp 569 2011-07-25 19:48:51Z bigmuscle $
 */
