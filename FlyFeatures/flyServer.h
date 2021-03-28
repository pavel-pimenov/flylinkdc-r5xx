/*
 * Copyright (C) 2011-2021 FlylinkDC++ Team http://flylinkdc.com
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

#if  !defined(_FLY_SERVER_H)
#define _FLY_SERVER_H

#pragma once

#include <string>
#include <unordered_set>

#include <boost/asio/ip/address_v4.hpp>

#include "../client/CFlyThread.h"
#include "../client/CFlyMediaInfo.h"
#include "../client/MerkleTree.h"
#include "../client/Util.h"
#include "../client/SettingsManager.h"
#include "../client/LogManager.h"
#include "../zlib-ng/zlib-ng.h"
#include "libtorrent/sha1_hash.hpp"

namespace sel {
	class State;
}

class SearchResult;

bool getMediaInfo(const string& p_name, CFlyMediaInfo& p_media, int64_t p_size, const TTHValue& p_tth, bool p_force = false);
//=======================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
//=======================================================================
struct CServerItem
{
	CServerItem(const string& p_ip = BaseUtil::emptyString, const uint16_t p_port = 0) : m_ip(p_ip), m_port(p_port), m_time_response(0)
	{
	}
	string getServerAndPort() const
	{
		return getIp() + ':' + Util::toString(getPort());
	}
	bool init(const string& p_ip_port)
	{
		m_time_response = 0;
		const auto l_port_pos = p_ip_port.find(':');
		if (l_port_pos != string::npos)
		{
			m_ip = p_ip_port.substr(0, l_port_pos);
			m_port = atoi(p_ip_port.c_str() + l_port_pos + 1);
			const bool l_result = !m_ip.empty() && m_port;
			dcassert(l_result);
			return l_result;
		}
		else
		{
			dcassert(0);
			m_ip.clear();
			m_port = 0;
		}
		return false;
	}
	GETSET(string, m_ip, Ip);
	GETSET(uint16_t, m_port, Port);
	GETSET(uint32_t, m_time_response, TimeResponse);
};
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
//=======================================================================
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
class  CFlyServerStatistics
{
	public:
		enum TypeTimeMark
		{
			TIME_START_GUI,
			TIME_START_CORE,
			TIME_SHUTDOWN_GUI,
			TIME_SHUTDOWN_CORE,
			TIME_LAST
		};
		CFlyServerStatistics()
		{
			memset(&m_time_mark, 0, sizeof(m_time_mark));
		}
		DWORD m_time_mark[TIME_LAST];
		string m_upnp_router_name;
		string m_upnp_status;
		
		void startTick(TypeTimeMark p_id)
		{
			m_time_mark[p_id] = GetTickCount();
		}
		void stopTick(TypeTimeMark p_id)
		{
			m_time_mark[p_id] = GetTickCount() - m_time_mark[p_id];
		}
		void saveShutdownMarkers();
};
extern CFlyServerStatistics g_fly_server_stat;
#endif // FLYLINKDC_USE_GATHER_STATISTICS
//=======================================================================
class CFlyServerConfig
{
	public:
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		enum type_server
		{
			TYPE_FLYSERVER_TCP = 1,
			TYPE_FLYSERVER_HTTP = 2 // TODO
		};
#endif
		CFlyServerConfig() :
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			m_min_file_size(0),
			m_type(TYPE_FLYSERVER_TCP),
			m_send_full_mediainfo(false),
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
			m_collect_lost_location(false),
#endif
			m_zlib_compress_level(Z_BEST_COMPRESSION),
#endif
			m_time_load_config(0),
			m_time_reload_config(0)
		{
		}
		~CFlyServerConfig()
		{
		}
	public:
		bool isSupportFile(const string& p_file_ext, uint64_t p_size) const;
		static bool isSupportTag(const string& p_tag);
		static bool isErrorLog(unsigned p_error_code);
		static bool isGuardTCPPort(uint16_t p_port);
		static bool isExcludeCIDfromErrorLog(unsigned p_error_code);
		static bool isBlockIP(const string& p_ip);
		static void addBlockIP(const string& p_ip);
		void ConvertInform(string& p_inform) const;
	private:
		static StringSet g_include_tag;
		static StringSet g_exclude_tag;
		static std::unordered_set<uint16_t> g_guard_tcp_port;
		static FastCriticalSection g_cs_guard_tcp_port;
		static FastCriticalSection g_cs_mirror_test_port;
		static std::unordered_set<unsigned> g_exclude_error_log;
		static std::unordered_set<unsigned> g_exclude_cid_error_log;
		static std::vector<std::string> g_exclude_tag_inform;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		bool     m_send_full_mediainfo; // Если = true на сервер шлем данные если есть полная информация о медиа-файле
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		bool     m_collect_lost_location;
#endif
		int8_t         m_zlib_compress_level;
		type_server    m_type;
		uint64_t       m_min_file_size;
// TODO boost::flat_set
		StringSet m_scan;
//
		static std::vector<CServerItem> g_mirror_read_only_servers;
		static std::vector<CServerItem> g_mirror_test_port_servers;
		static std::vector<CServerItem> g_torrent_dht_servers;
        static std::vector<std::string> g_hublist_url;
        static std::vector<std::string> g_promo_hub_url;
        static std::string g_osago_url;
        static CServerItem g_local_test_server;
		static CServerItem g_main_server;
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
		static CServerItem g_stat_server;
#endif
	public:
        static const std::string& getOSAGOUrl()
        {
            return g_osago_url;
        }
        static const std::vector<CServerItem>& getTorrentDHTServer()
		{
			dcassert(!g_torrent_dht_servers.empty());
			return g_torrent_dht_servers;
		}
        static const std::vector<std::string>& getHubListUrl()
        {
            dcassert(!g_hublist_url.empty());
            return g_hublist_url;
        }
        static const std::vector<std::string>& getPromoHubUrl()
        {
            dcassert(!g_promo_hub_url.empty());
            return g_promo_hub_url;
        }
        
		static const CServerItem& getStatServer();
		static const CServerItem& getTestPortServer();
		static const std::vector<CServerItem> getMirrorTestPortServerArray();
		
		static const CServerItem& getRandomMirrorServer(bool p_is_set);
		bool isInit() const
		{
			return !m_scan.empty();
		}
		bool isFullMediainfo() const
		{
			return m_send_full_mediainfo;
		}
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		bool isCollectLostLocation() const
		{
			return m_collect_lost_location;
		}
#endif
		int getZlibCompressLevel() const
		{
			return m_zlib_compress_level;
		}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
	private:
		uint64_t m_time_load_config;
		uint32_t m_time_reload_config;
		static const uint32_t TIME_TO_RELOAD_CONFIG_IF_ERROR = 1000 * 60 * 30; // 30m
		static const uint32_t TIME_TO_RELOAD_CONFIG_IF_SUCCESFUL = 1000 * 60 * 60 * 12; // 12h
		
	public:
		static bool isSpam(const string& p_line);
		static void loadTorrentSearchEngine();
		static string g_lua_source_search_engine;
		static bool torrentGetTop(HWND p_wnd, int p_message);
		static bool torrentSearch(HWND p_wnd, int p_message, const tstring& p_search);
	private:
		static bool torrentSearchParser(HWND p_wnd, int p_message, string p_search_url,
			int p_index, sel::State& p_lua_parser, string p_local_agent, string p_error_base,
			string p_root_torrent_url, string p_tracker_name,
			unsigned p_num_page);

		static std::vector<string> g_spam_urls;
		static StringSet g_parasitic_files;
		static StringSet g_mediainfo_ext;
		static StringSet g_virus_ext;
		static StringSet g_ignore_flood_command;
		static StringSet g_block_share_ext;
		static StringSet g_video_ext;
		static StringSet g_custom_compress_ext;
		static StringSet g_block_share_name;
		static StringList g_block_share_mask;
		
		static inline bool isCheckName(const StringSet& p_StringList, const string& p_name)
		{
			return p_StringList.find(p_name) != p_StringList.end();
		}
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		static bool SyncAntivirusDB(bool& p_is_need_reload);
#endif
	public:
#ifdef FLYLINKDC_USE_XXX_BLOCK
		 static bool SyncXXXBlockDB();
#endif
		
		void loadConfig();
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		static void SyncAntivirusDBSafe();
#endif
		
		static string getAllMediainfoExt()
		{
			return Util::toString(',', g_mediainfo_ext);
		}
		static bool isParasitFile(const string& p_file);
		static bool isMediainfoExt(const string& p_ext);
		static bool isVirusExt(const string& p_ext);
        static bool isVirusEnd(const string& p_end);
        static bool isIgnoreFloodCommand(const string& p_command);
		static bool isCompressExt(const string& p_ext);
		static bool isBlockShareExt(const string& p_name, const string& p_ext);
		static bool isVideoShareExt(const string& p_ext);
		static std::vector<StringPair> getDeadHub();
		static int getAlternativeHub(string& p_url);
		string DBDelete();
		static DWORD    g_winet_connect_timeout;
		static DWORD    g_winet_receive_timeout;
		static DWORD    g_winet_send_timeout;
		static uint16_t g_winet_min_response_time_for_log;
		static uint16_t g_max_ddos_connect_to_me;
		static uint16_t g_max_unique_tth_search;
		static uint16_t g_max_unique_file_search;
		static uint16_t g_ban_ddos_connect_to_me;
		static uint16_t g_interval_flood_command;
		static uint16_t g_max_flood_command;
		static uint16_t g_ban_flood_command;
		static uint16_t g_unique_files_for_virus_detect;
		static DWORD    g_max_size_for_virus_detect;
        static DWORD    g_max_size_search_v_detect;
        static bool     g_is_append_cid_error_log;
		static bool     g_is_use_hit_media_files;
		static bool     g_is_use_hit_binary_files;
		static bool     g_is_use_statistics;
        static bool     g_is_use_log_redirect;

#ifdef USE_SUPPORT_HUB
		static string   g_support_hub;
#ifdef FLYLINKDC_USE_SUPPORT_HUB_EN
		static string   g_support_hub_en;
#endif
#endif // USE_SUPPORT_HUB
		static string   g_support_upnp;
		static std::vector<std::string> g_mapping_hubs;
		//static std::unordered_set<unsigned long> g_block_ip;
		static std::unordered_set<std::string> g_block_ip_str;
		static FastCriticalSection g_cs_block_ip;

		static std::unordered_set<std::string> g_block_hubs;
		static std::vector<std::string> g_block_hubs_mask;
		static std::vector<std::string> g_promo_hubs[2];
		static std::unordered_set<std::string> g_detect_search_bot;
		static string g_regex_find_ip;
		static string g_faq_search_does_not_work;
#ifdef FLYLINKDC_USE_XXX_BLOCK
		static string   g_xxx_block_db_url;
#endif
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		static string   g_antivirus_db_url;
#endif
};
//=======================================================================
extern CFlyServerConfig g_fly_server_config; // TODO: cleanup call of this.
//=======================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
class CFlyTTHKey
{
	public:
		TTHValue m_tth;
		std::shared_ptr<libtorrent::sha1_hash> m_sha1;
		int64_t m_file_size;
		bool m_is_sha1_for_file;
		CFlyTTHKey(const libtorrent::sha1_hash& p_sha1, int64_t p_file_size, bool p_is_sha1_for_file) :
			m_sha1(std::make_shared<libtorrent::sha1_hash>(p_sha1)), m_file_size(p_file_size), m_is_sha1_for_file(p_is_sha1_for_file)
		{
		}
		CFlyTTHKey(const TTHValue& p_tth, int64_t p_file_size):
			m_tth(p_tth), m_file_size(p_file_size), m_is_sha1_for_file(false)
		{
		}
		bool operator < (const CFlyTTHKey& p_val) const
		{
			return (m_file_size < p_val.m_file_size) || (m_file_size == p_val.m_file_size && m_tth < p_val.m_tth);
		}
};
//=======================================================================
class CFlyServerKey : public CFlyTTHKey
{
	public:
		uint32_t m_hit;
		uint32_t m_time_hash;
		uint32_t m_time_file;
		CFlyMediaInfo m_media;
		bool m_only_ext_info; // Запрашиваем только расширенную информацию
		// TODO убрать этот параметр т.к. он используется только при точечном запроса
		bool m_only_counter;  // Запрашиваем только счетчики
		bool m_is_cache;      // Признак найденности в локальном кеше
		CFlyServerKey(const TTHValue& p_tth, int64_t p_file_size):
			CFlyTTHKey(p_tth, p_file_size), m_hit(0), m_time_hash(0), m_time_file(0), m_only_ext_info(false), m_only_counter(false), m_is_cache(false)
		{
			//  m_file_name = Util::getFileName(p_file_name);
		}
};
//=======================================================================
struct CFlyServerInfo
{
	public:
		bool m_already_processed;
		CFlyServerInfo() : m_already_processed(false)
		{
		}
		static string getMediaInfoAsText(const TTHValue& p_tth, int64_t p_file_size);
};
//=======================================================================
typedef std::vector<CFlyServerKey> CFlyServerKeyArray;
typedef std::vector<std::pair<CFlyTTHKey, string> > CFlyTTHKeyDownloadArray;

//=======================================================================
struct CFlyVirusFileList
{
	string m_nick;
	string m_ip;
	string m_hub_url;
	string m_virus_path;
	time_t m_time;
	std::map<CFlyTTHKey, std::vector<string> > m_files;
	CFlyVirusFileList() : m_time(0)
	{
	}
};
//=======================================================================
typedef std::vector<CFlyVirusFileList> CFlyVirusFileListArray;
//=======================================================================
struct CFlyVirusFileInfo
{
	string m_nick;
	string m_file_name;
	string m_ip;
	string m_ip_from_user;
	string m_hub_name;
	string m_hub_url;
	string m_virus_path;
	time_t m_time;
	unsigned m_count_file;
	unsigned m_virus_level;
	CFlyVirusFileInfo() : m_count_file(0), m_virus_level(0), m_time(0)
	{
	}
};
//=======================================================================
typedef std::vector<CFlyVirusFileInfo> CFlyVirusFileInfoArray;
typedef std::map<CFlyTTHKey, CFlyVirusFileInfoArray > CFlyAntivirusTTHArray;
//=======================================================================
class CFlyServerAdapter
{
	public:
		explicit CFlyServerAdapter(const DWORD p_dwMilliseconds = INFINITE):
			m_dwMilliseconds(p_dwMilliseconds)
#ifdef _DEBUG
			, m_debugWaits(false)
#endif
		{
		}
		virtual ~CFlyServerAdapter()
		{
			dcassert(m_debugWaits);
			dcassert(g_GetFlyServerArray.empty());
			dcassert(g_SetFlyServerArray.empty());
			dcassert(g_tth_media_file_map.empty());
		}
		virtual void mergeFlyServerInfo() = 0;
        void waitForFlyServerStop();
        bool is_fly_server_active() const;
        void addFlyServerTask(uint64_t p_tick, bool p_is_force);
        static void clearFlyServerQueue();

	protected:
        static CFlyServerKeyArray  g_GetFlyServerArray;    // Запросы на получения медиаинформации. TODO - сократить размер структуры для запроса.
        static ::CriticalSection  g_cs_get_array_fly_server;
        static CFlyServerKeyArray g_SetFlyServerArray;    // Запросы на передачу медиаинформации если она у нас есть в базе и ее ниразу не слали.
        static ::CriticalSection  g_cs_set_array_fly_server;

        static ::CriticalSection g_cs_tth_media_map;
        static std::unordered_map<TTHValue, uint64_t> g_tth_media_file_map;
        static void clear_tth_media_map();


		static std::unordered_map<TTHValue, std::pair<CFlyServerInfo*, CFlyServerCache> > g_fly_server_cache;
		static ::CriticalSection g_cs_fly_server;
		void prepare_mediainfo_to_fly_serverL();
		static void push_mediainfo_to_fly_server();
		static void post_message_for_update_mediainfo(const HWND p_hMediaWnd);
	private:
		dcdrun(bool m_debugWaits;)
		const DWORD m_dwMilliseconds;
	public:
		class CFlyServerQueryThread : public Thread
		{
			public:
				explicit CFlyServerQueryThread(CFlyServerAdapter* p_adapter) : m_previous_tick(0), m_adapter(p_adapter)
				{
				}
				~CFlyServerQueryThread()
				{
				}
				int run()
				{
					if (!g_fly_server_config.isInit())
					{
						g_fly_server_config.loadConfig();
					}
					if (g_fly_server_config.isInit())
					{
						m_adapter->mergeFlyServerInfo();
					}
					return 0;
				}
				void processTask(uint64_t p_tick)
				{
					try
					{
						if (p_tick > m_previous_tick + g_minimal_interval_in_ms)
						{
							m_previous_tick = p_tick;
							if (is_active())
							{
								return; // Поток еще работает. пропустим...
							}
							else
							{
								start(256);  // Запустим на обработку.
							}
						}
					}
					catch (const ThreadException& e)
					{
						LogManager::message("Error processTask: " + e.getError()); // fix https://drdump.com/Problem.aspx?ProblemID=240533
					}
				}
				static void setMinimalIntervalInMilliSecond(uint16_t p_new_interval)
				{
					g_minimal_interval_in_ms = p_new_interval > 500 ? p_new_interval : 2000;
				}
			private:
				static uint16_t g_minimal_interval_in_ms;
				uint64_t m_previous_tick;
				CFlyServerAdapter* m_adapter;
		};
		
		
		std::unique_ptr<CFlyServerQueryThread> m_query_thread;
};

class CFlyServerJSON
{
	public:
		static bool login();
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
		static bool pushStatistic(const bool p_is_sync_run);
#endif
		static bool pushError(unsigned p_error_code, string p_error, bool p_is_include_disk_info = false);
		static bool setTestPortOK(unsigned short p_port, const std::string& p_type);
		static bool isTestPortOK(unsigned short p_port, const std::string& p_type, bool p_is_assert = false);
		static bool pushTestPort(const std::vector<unsigned short>& p_udp_port,
		                         const std::vector<unsigned short>& p_tcp_port,
		                         string& p_external_ip,
		                         int p_timer_value,
		                         const string& p_name_test);
		                         
		static string g_fly_server_id;
		static CFlyTTHKeyDownloadArray g_download_counter;
		static void addDownloadCounter(const CFlyTTHKey& p_file, const string& p_file_name);
		static bool sendDownloadCounter(bool p_is_only_db_if_network_error);
		
		static CFlyAntivirusTTHArray g_antivirus_counter;
		static void addAntivirusCounter(const CFlyTTHKey& p_key, const CFlyVirusFileInfo& p_file_info);
		static void addAntivirusCounter(const SearchResult &p_search_result, int p_count_file, int p_level);
		static bool sendAntivirusCounter(bool p_is_only_db_if_network_error);
		
		static CFlyVirusFileListArray g_antivirus_file_list;
		static void addAntivirusCounter(const CFlyVirusFileList& p_file_list);
		
		static string connect(const CFlyServerKeyArray& p_fileInfoArray, bool p_is_fly_set_query, bool p_is_ext_info_for_single_file = false);
		static string postQueryTestPort(CFlyLog& p_log, const string& p_body, bool& p_is_send, bool& p_is_error);
		static string postQuery(bool p_is_set,
		                        bool p_is_stat_server,
		                        bool p_is_disable_zlib_in,
		                        bool p_is_disable_zlib_out,
		                        const char* p_query,
		                        const string& p_body,
		                        bool& p_is_send,
		                        bool& p_is_error,
		                        DWORD p_time_out = 0,
		                        const CServerItem* p_server = nullptr);
	private:
		static ::CriticalSection g_cs_error_report;
		static ::CriticalSection g_cs_download_counter;
		static ::CriticalSection g_cs_antivirus_counter;
		static FastCriticalSection g_cs_test_port;
		static string g_last_error_string;
		static int g_count_dup_error_string;
		static DWORD g_last_error_code;
		typedef std::map<std::pair<unsigned short, string>, std::pair<bool, uint64_t> > CFlyTestPortResult;
		static CFlyTestPortResult g_test_port_map;
};

//=======================================================================
#else
// Кривизна - поправить
class CFlyServerAdapter
{
};
class CFlyServerJSON
{
public:
    static bool pushError(unsigned p_error_code, string p_error, bool p_is_include_disk_info = true)
    {
        return false;
    }
};
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER

#endif // _FLY_SERVER_H
