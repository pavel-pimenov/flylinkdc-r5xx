/*
 * Copyright (C) 2011-2012 FlylinkDC++ Team http://flylinkdc.com/
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

#include <boost/atomic.hpp>

#include "../client/CFlyThread.h"
#include "../client/CFlyMediaInfo.h"
#include "../client/MerkleTree.h"
#include "../client/Util.h"
#include "../client/SettingsManager.h"
#include "../zlib/zlib.h"

//=======================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
struct CServerItem
{
	CServerItem(const string& p_ip = Util::emptyString, const uint16_t p_port = 0) : m_ip(p_ip), m_port(p_port),m_time_response(0)
	{
	}
	GETSET(string, m_ip, Ip);
	GETSET(uint16_t, m_port, Port);
	GETSET(uint32_t, m_time_response, TimeResponse);
};
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
#ifdef STRONG_USE_DHT
struct DHTServer
{
	DHTServer(const string& p_url, const string& p_agent = Util::emptyString) : m_url(p_url), m_agent(p_agent)
	{
	}
	GETC(string, m_url, Url);
	GETC(string, m_agent, Agent);
};
#endif // STRONG_USE_DHT
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
		memset(&m_time_mark,0,sizeof(m_time_mark));
	}
	DWORD m_time_mark[TIME_LAST];
	string m_upnp_router_name; // http://code.google.com/p/flylinkdc/issues/detail?id=1241
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
 bool isSupportTag (const string& p_tag) const;
 void ConvertInform(string& p_inform) const;
private:
 StringSet m_include_tag; 
 StringSet m_exclude_tag; 
 std::vector<std::string> m_exclude_tag_inform;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
 bool     m_send_full_mediainfo; // Если = true на сервер шлем данные если есть полная информация о медиа-файле
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
 bool     m_collect_lost_location;
#endif
 int8_t      m_zlib_compress_level;
 type_server   m_type;
 uint64_t m_min_file_size;
 // TODO boost::flat_set
 StringSet m_scan;  
//
 static std::vector<CServerItem> g_mirror_read_only_servers;
 static CServerItem g_main_server;
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
 static CServerItem g_stat_server;
#endif
 static CServerItem g_test_port_server;
public:
static CServerItem& getStatServer()
{
	return g_stat_server;
}
static CServerItem& getTestPortServer()
{
	return g_test_port_server;
}
static CServerItem& getRandomMirrorServer(bool p_is_set);
//
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

#ifdef STRONG_USE_DHT
private:
 static std::vector<DHTServer> g_dht_servers;
public:
 static const DHTServer& getRandomDHTServer();
#endif // STRONG_USE_DHT
private:
 static StringSet g_parasitic_files;
 static StringSet g_mediainfo_ext;
 static StringSet g_virus_ext;
 static StringSet g_ignore_flood_command;
 static StringSet g_block_share_ext;
 static StringSet g_custom_compress_ext;
 static StringSet g_block_share_name;
 static StringList g_block_share_mask;

 static bool isCheckName(const StringSet& p_StringList, const string& p_name)
 {
		return p_StringList.find(p_name) != p_StringList.end();
 }
public:
  void loadConfig();
  static bool SyncAntivirusDB();

  static bool isParasitFile(const string& p_file);
  static bool isMediainfoExt(const string& p_ext);
  static bool isVirusExt(const string& p_ext);
  static bool isIgnoreFloodCommand(const string& p_command);
  static bool isCompressExt(const string& p_ext);
  static bool isBlockShare(const string& p_name);
  string DBDelete();
  static DWORD    g_winet_connect_timeout;
  static DWORD    g_winet_receive_timeout;
  static DWORD    g_winet_send_timeout;
  static uint16_t g_winet_min_response_time_for_log;
  static uint16_t g_max_ddos_connect_to_me;
  static uint16_t g_max_unique_tth_search;
  static uint16_t g_ban_ddos_connect_to_me;
  static uint16_t g_min_interval_dth_connect;
  static uint16_t g_interval_flood_command;
  static uint16_t g_max_flood_command;
  static uint16_t g_ban_flood_command;

#ifdef USE_SUPPORT_HUB
  static string   g_support_hub;
#endif // USE_SUPPORT_HUB
  static string   g_faq_search_does_not_work;
  static string   g_antivirus_db_url;
};
//=======================================================================
extern CFlyServerConfig g_fly_server_config; // TODO: cleanup call of this.
//=======================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
class CFlyTTHKey 
{
	public:
		TTHValue m_tth;
		int64_t m_file_size;
		CFlyTTHKey(const TTHValue& p_tth, int64_t p_file_size):
		m_tth(p_tth),m_file_size(p_file_size)
		{
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
			CFlyTTHKey(p_tth,p_file_size),m_hit(0),m_time_hash(0),m_time_file(0), m_only_ext_info(false), m_only_counter(false),m_is_cache(false)
		{
		//	m_file_name = Util::getFileName(p_file_name);
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
		static string getMediaInfoAsText(const TTHValue& p_tth,int64_t p_file_size);
};
//=======================================================================
typedef std::vector<CFlyServerKey> CFlyServerKeyArray;
typedef std::vector<CFlyTTHKey> CFlyTTHKeyArray;
//=======================================================================
class CFlyServerAdapter
{
	public:
		CFlyServerAdapter(const HWND& p_hWnd, const DWORD p_dwMilliseconds = INFINITE):
		     m_hMediaWnd(p_hWnd), 
		     m_dwMilliseconds(p_dwMilliseconds)
#ifdef _DEBUG
			 ,m_debugWaits(false)
#endif
		{
		}
		virtual ~CFlyServerAdapter()
		{
			dcassert(m_debugWaits);
			dcassert(m_GetFlyServerArray.empty());
			dcassert(m_SetFlyServerArray.empty());
			dcassert(m_tth_media_file_map.empty());
		}
		virtual void mergeFlyServerInfo() = 0;
		void waitForFlyServerStop()
		{
			dcdrun(m_debugWaits = true;)
			if(m_query_thread)
				{
			      m_query_thread->join(m_dwMilliseconds); // Дождемся завершения но не более 10 сек
		        }
			}

		bool is_fly_server_active() const
		{
			if(m_query_thread)
			  return m_query_thread->is_active();
			else
			  return false;
		}
		void addFlyServerTask(uint64_t p_tick, bool p_is_force)
		{
			if (p_is_force || BOOLSETTING(ENABLE_FLY_SERVER))
			{
				if(!m_query_thread)
				    m_query_thread = std::unique_ptr<CFlyServerQueryThread>(new CFlyServerQueryThread(this));
				m_query_thread->processTask(p_tick);
			}
		}
		void clearFlyServerQueue()
		{
			Lock l(g_cs_fly_server);
			m_GetFlyServerArray.clear();
			m_SetFlyServerArray.clear();
		}
	protected:
		CFlyServerKeyArray	m_GetFlyServerArray;    // Запросы на получения медиаинформации. TODO - сократить размер структуры для запроса.
		CFlyServerKeyArray	m_SetFlyServerArray;    // Запросы на передачу медиаинформации если она у нас есть в базе и ее ниразу не слали.
		std::unordered_map<TTHValue, uint64_t> m_tth_media_file_map;
		static std::unordered_map<TTHValue, std::pair<CFlyServerInfo*, CFlyServerCache> > g_fly_server_cache;
		static ::CriticalSection g_cs_fly_server;
		void prepare_mediainfo_to_fly_serverL();
		void push_mediainfo_to_fly_server();
		void post_message_for_update_mediainfo();

	private:
		dcdrun(bool m_debugWaits;)
		const HWND& m_hMediaWnd;	
		const DWORD m_dwMilliseconds;
	public:
		class CFlyServerQueryThread : public Thread
		{
			public:
				CFlyServerQueryThread(CFlyServerAdapter* p_adapter) : m_previous_tick(0), m_adapter(p_adapter)
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
						if (p_tick > m_previous_tick + g_minimal_interval_in_ms)
						{
							m_previous_tick = p_tick;
							if(is_active())
							{
								return; // Поток еще работает. пропустим...
							}
							else
							{
							  start(256);  // Запустим на обработку.
						}
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
		
		struct CFlyServerJSON
#ifdef _DEBUG
			: boost::noncopyable
#endif
		{
			static bool login();
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
			static bool pushStatistic(const bool p_is_sync_run);
#endif
			static bool pushError(const string& p_error);
			static bool pushTestPort(const string& p_magic,		
				const std::vector<unsigned short>& p_udp_port,
				const std::vector<unsigned short>& p_tcp_port,
				string& p_external_ip,
				int p_timer_value);
			
			// TODO static void logout();
			static string g_fly_server_id;
			static CFlyTTHKeyArray g_download_counter;
			static void addDownloadCounter (const CFlyTTHKey& p_file);
			static bool sendDownloadCounter();
			static string connect(const CFlyServerKeyArray& p_fileInfoArray, bool p_is_fly_set_query, bool p_is_ext_info_for_single_file = false);
			static string postQuery(bool p_is_set, 
				                    bool p_is_stat_server, 
									bool p_is_test_port_server,
									bool p_is_disable_zlib_in, 
									bool p_is_disable_zlib_out,
									const char* p_query, 
									const string& p_body, 
									bool& p_is_send,
									bool& p_is_error,
									DWORD p_time_out = 0);		
    private:
        static ::CriticalSection g_cs_error_report;
        static string g_last_error_string;
        static int g_count_dup_error_string;
		};

		std::unique_ptr<CFlyServerQueryThread> m_query_thread;
};
//=======================================================================

#endif // FLYLINKDC_USE_MEDIAINFO_SERVER

#endif // _FLY_SERVER_H
