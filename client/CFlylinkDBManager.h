//-----------------------------------------------------------------------------
//(c) 2007-2016 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#pragma once

#ifndef CFlylinkDBManager_H
#define CFlylinkDBManager_H

#include <vector>
#include <boost/unordered/unordered_map.hpp>
#include "QueueItem.h"
#include "Singleton.h"
#include "CFlyThread.h"
#include "sqlite/sqlite3x.hpp"
#include "CFlyMediaInfo.h"
#include "LogManager.h"

#define FLYLINKDC_USE_LEVELDB

#ifdef FLYLINKDC_USE_LEVELDB

#include "leveldb/status.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/cache.h"
#include "leveldb/filter_policy.h"

#endif
#include "libtorrent/session.hpp"

using sqlite3x::sqlite3_connection;
using sqlite3x::sqlite3_command;
using sqlite3x::sqlite3_reader;
using sqlite3x::sqlite3_transaction;
using sqlite3x::database_error;

class CFlySQLCommand
{
	public:
		CFlySQLCommand() {}
		sqlite3_command* get_sql()
		{
			return m_sql.get();
		}
		sqlite3_command* operator->()
		{
			return m_sql.get();
		}
		sqlite3_command* init(sqlite3_connection& p_db, const char* p_sql)
		{
			CFlyFastLock(m_cs);
			if (!m_sql.get())
			{
				m_sql = unique_ptr<sqlite3_command>(new sqlite3_command(p_db, p_sql));
			}
			return m_sql.get();
		}
		int sqlite3_changes() const
		{
			return m_sql->get_connection().sqlite3_changes();
		}
	private:
		unique_ptr<sqlite3_command> m_sql;
		FastCriticalSection m_cs;
};
#ifdef FLYLINKDC_USE_LEVELDB
class CFlyLevelDB
{
		leveldb::DB* m_db;
		leveldb::Options      m_options;
		leveldb::ReadOptions  m_readoptions;
		leveldb::ReadOptions  m_iteroptions;
		leveldb::WriteOptions m_writeoptions;
		
	public:
		CFlyLevelDB();
		~CFlyLevelDB();
		static void shutdown();
		
		bool open_level_db(const string& p_db_name, bool& p_is_destroy);
		bool get_value(const void* p_key, size_t p_key_len, string& p_result);
		bool set_value(const void* p_key, size_t p_key_len, const void* p_val, size_t p_val_len);
		bool get_value(const TTHValue& p_tth, string& p_result)
		{
			return get_value(p_tth.data, p_tth.BYTES, p_result);
		}
		bool set_value(const TTHValue& p_tth, const string& p_status)
		{
			dcassert(!p_status.empty());
			if (!p_status.empty())
				return set_value(p_tth.data, p_tth.BYTES, p_status.c_str(), p_status.length());
			else
				return false;
		}
		uint32_t set_bit(const TTHValue& p_tth, uint32_t p_mask);
};
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
#pragma pack(push, 1)
struct CFlyIPMessageCache
{
	uint32_t m_message_count;
	unsigned long m_ip;
	CFlyIPMessageCache(uint32_t p_message_count = 0, unsigned long p_ip = 0) : m_message_count(), m_ip(p_ip)
	{
	}
};
#pragma pack(pop)
class CFlyLevelDBCacheIP : public CFlyLevelDB
{
	public:
		void set_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick, uint32_t p_message_count, const boost::asio::ip::address_v4& p_last_ip);
		CFlyIPMessageCache get_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick);
	private:
		void create_key(uint32_t p_hub_id, const string& p_nick, std::vector<char>& p_key)
		{
			p_key.resize(sizeof(uint32_t) + p_nick.size());
			memcpy(&p_key[0], &p_hub_id, sizeof(p_hub_id));
			memcpy(&p_key[0] + sizeof(p_hub_id), p_nick.c_str(), p_nick.size());
		}
};
#endif // FLYLINKDC_USE_IPCACHE_LEVELDB
#endif // FLYLINKDC_USE_LEVELDB

enum eTypeTransfer
{
	e_TransferDownload = 0,
	e_TransferUpload = 1
};

enum eTypeDIC
{
	e_DIC_HUB = 1,
	e_DIC_NICK = 2,
	e_DIC_IP = 3,
	e_DIC_COUNTRY = 4,
	e_DIC_LOCATION = 5,
	e_DIC_LAST
};
struct CFlyIPRange
{
	uint32_t m_start_ip;
	uint32_t m_stop_ip;
	CFlyIPRange() //: m_start_ip(0), m_stop_ip(0)
	{
	}
	CFlyIPRange(uint32_t p_start_ip, uint32_t p_stop_ip): m_start_ip(p_start_ip), m_stop_ip(p_stop_ip)
	{
	}
};

struct CFlyP2PGuardIP : public CFlyIPRange
{
	string m_note;
	CFlyP2PGuardIP()
	{
	}
	CFlyP2PGuardIP(const std::string& p_note, uint32_t p_start_ip, uint32_t p_stop_ip) :
		CFlyIPRange(p_start_ip, p_stop_ip), m_note(p_note)
	{
	}
};

struct CFlyLocationIP : public CFlyIPRange
{
	uint16_t m_flag_index;
	string m_location;
	CFlyLocationIP() // : m_flag_index(0)
	{
	}
	CFlyLocationIP(const std::string& p_location, uint32_t p_start_ip, uint32_t p_stop_ip, uint16_t p_flag_index) :
		CFlyIPRange(p_start_ip, p_stop_ip), m_location(p_location), m_flag_index(p_flag_index)
	{
	}
};
struct CFlyLocationDesc : public CFlyLocationIP
{
	tstring m_description;
};
typedef std::vector<CFlyLocationIP> CFlyLocationIPArray;
typedef std::vector<CFlyP2PGuardIP> CFlyP2PGuardArray;

struct CFlyTransferHistogram
{
	std::string m_date;
	unsigned m_count;
	unsigned m_date_as_int;
	uint64_t m_actual;
	uint64_t m_file_size;
	CFlyTransferHistogram() : m_count(0), m_date_as_int(0), m_actual(0), m_file_size(0)
	{
	}
};
typedef std::vector<CFlyTransferHistogram> CFlyTransferHistogramArray;

struct CFlyAntivirusItem
{
	boost::asio::ip::address_v4 m_ip;
	int64_t m_share;
	CFlyAntivirusItem(): m_share(0)
	{
	}
};
struct CFlyLastIPCacheItem
{
	boost::asio::ip::address_v4 m_last_ip;
	uint32_t m_message_count;
	bool m_is_item_dirty;
	// TODO - добавить сохранение страны + провайдера и индексов иконок.
	CFlyLastIPCacheItem(): m_message_count(0), m_is_item_dirty(false)
	{
	}
};

struct CFlyFileInfo
{
	int64_t  m_size;
	int64_t  m_TimeStamp;
	TTHValue m_tth;
	uint32_t m_hit;
	int64_t m_StampShare;
	std::shared_ptr<CFlyMediaInfo> m_media_ptr;
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
	bool     m_found;
#endif
	bool     m_recalc_ftype;
	char     m_ftype;
	CFlyFileInfo()
	{
	}
};
typedef boost::unordered_map<string, CFlyFileInfo> CFlyDirMap;
struct CFlyPathItem
{
	__int64 m_path_id;
	bool    m_is_found;
	bool    m_is_no_mediainfo;
	CFlyPathItem(__int64 p_path_id = 0, bool p_is_found = false, bool p_is_no_mediainfo = false)
		: m_path_id(p_path_id),
		  m_is_found(p_is_found),
		  m_is_no_mediainfo(p_is_no_mediainfo)
	{
	}
};
enum eTypeSegment
{
	e_ExtraSlot = 1,
	e_RecentHub = 2,
	e_SearchHistory = 3,
	// 4-5 - пропускаем старые маркеры e_TimeStampGeoIP + e_TimeStampCustomLocation
	e_CMDDebugFilterState = 6,
	e_TimeStampGeoIP = 7,
	e_TimeStampCustomLocation = 8,
	// e_IsTTHLevelDBConvert = 9,
	e_IncopatibleSoftwareList = 10,
	// 11, - не занимать
	e_DeleteCounterAntivirusDB = 12,
	// 13 - устаревший e_TimeStampAntivirusDB
	e_MergeCounterAntivirusDB = 14,
	// 15 - устаревший e_TimeStampP2PGuard
	e_TimeStampAntivirusDB = 16,
	e_TimeStampIBlockListCom = 17,
	e_TimeStampP2PGuard = 18,
	e_autoAddSupportHub = 19,
	e_autoAddFirstSupportHub = 20,
	e_LastShareSize = 21,
	e_autoAdd1251SupportHub = 22
};
struct CFlyRegistryValue
{
	string m_val_str;
	__int64  m_val_int64;
	explicit CFlyRegistryValue(__int64 p_val_int64 = 0)
		: m_val_int64(p_val_int64)
	{
	}
	CFlyRegistryValue(const string &p_str, __int64 p_val_int64 = 0)
		: m_val_int64(p_val_int64), m_val_str(p_str)
	{
	}
	operator bool() const
	{
		return m_val_int64 != 0;
	}
	tstring toT() const
	{
		return Text::toT(m_val_str);
	}
};
struct CFlyBaseDirItem
{
	string m_synonym;
	int64_t m_path_id;
	CFlyBaseDirItem(): m_path_id(0)
	{
	}
	CFlyBaseDirItem(const string& p_synonym, int64_t p_path_id): m_synonym(p_synonym), m_path_id(p_path_id)
	{
	}
};
struct CFlyDirItem : public CFlyBaseDirItem
{
	string m_path;
	CFlyDirItem(const string& p_synonym, const string& p_path, int64_t p_path_id): CFlyBaseDirItem(p_synonym, p_path_id), m_path(p_path)
	{
	}
};
typedef std::vector<CFlyDirItem> CFlyDirItemArray;
typedef std::unordered_map<string, CFlyRegistryValue> CFlyRegistryMap;
typedef boost::unordered_map<string, CFlyPathItem> CFlyPathCache;
class CFlylinkDBManager : public Singleton<CFlylinkDBManager>
{
	public:
		CFlylinkDBManager();
		~CFlylinkDBManager();
		static void shutdown_engine();
		void flush();
		
		void push_download_tth(const TTHValue& p_tth);
		void push_add_share_tth(const TTHValue& p_tth);
		void push_add_virus_database_tth(const TTHValue& p_tth);
		static string get_db_size_info();
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		void store_all_ratio_and_last_ip(uint32_t p_hub_id,
		                                 const string& p_nick,
		                                 CFlyUserRatioInfo& p_user_ratio,
		                                 const uint32_t p_message_count,
		                                 const boost::asio::ip::address_v4& p_last_ip,
		                                 bool p_is_last_ip_dirty,
		                                 bool p_is_message_count_dirty,
		                                 bool& p_is_sql_not_found
		                                );
		uint32_t get_dic_hub_id(const string& p_hub);
		void load_global_ratio();
		void load_all_hub_into_cacheL();
#ifdef _DEBUG
		bool m_is_load_global_ratio;
#endif
		bool load_ratio(uint32_t p_hub_id, const string& p_nick, CFlyUserRatioInfo& p_ratio_info, const  boost::asio::ip::address_v4& p_last_ip);
		bool load_last_ip_and_user_stat(uint32_t p_hub_id, const string& p_nick, uint32_t& p_message_count, boost::asio::ip::address_v4& p_last_ip);
		void update_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick,
		                                      const boost::asio::ip::address_v4& p_last_ip,
		                                      const uint32_t p_message_count,
		                                      bool& p_is_sql_not_found,
		                                      bool p_is_last_ip_dirty,
		                                      bool p_is_message_count_dirty
		                                     );
	private:
		void store_all_ratio_internal(uint32_t p_hub_id, const __int64& p_dic_nick,
		                              const __int64& p_ip,
		                              const uint64_t& p_upload,
		                              const uint64_t& p_download
		                             );
		void update_last_ip_deferredL(uint32_t p_hub_id, const string& p_nick, uint32_t p_message_count, boost::asio::ip::address_v4 p_last_ip, bool& p_is_sql_not_found,
		                              bool p_is_last_ip_dirty,
		                              bool p_is_message_count_dirty
		                             );
		void flush_all_last_ip_and_message_count();
		void add_tree_internal_bind_and_executeL(sqlite3_command* p_sql, const TigerTree& p_tt);
		__int64 add_treeL(const TigerTree& p_tt);
		__int64 get_path_idL(string p_path, bool p_create, bool p_case_convet, bool& p_is_no_mediainfo, bool p_sweep_path);
		__int64 find_path_id(const string& p_path);
		__int64 create_path_idL(const string& p_path, bool p_is_skip_dup_val_index);
	public:
		CFlyGlobalRatioItem  m_global_ratio;
		double get_ratio() const;
		tstring get_ratioW() const;
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		
		bool is_table_exists(const string& p_table_name);
		
		enum FileStatus // [+] IRainman fix
		{
			UNKNOWN = 0,
			PREVIOUSLY_DOWNLOADED = 0x01,
			PREVIOUSLY_BEEN_IN_SHARE = 0x02,
			COUPLE = PREVIOUSLY_DOWNLOADED | PREVIOUSLY_BEEN_IN_SHARE,
			VIRUS_FILE_KNOWN = 0x04
		};
		
		FileStatus get_status_file(const TTHValue& p_tth);
		
		bool get_tree(const TTHValue& p_root, TigerTree& p_tt, __int64& p_block_size);
		unsigned __int64 get_block_size_sql(const TTHValue& p_root, __int64 p_size);
		__int64 get_path_id(string p_path, bool p_create, bool p_case_convet, bool& p_is_no_mediainfo, bool p_sweep_path);
		void add_tree(const TigerTree& p_tt);
	private:
		void prepare_scan_folder(const tstring& p_path);
		bool merge_mediainfoL(const __int64 p_tth_id, const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media);
		__int64 merge_fileL(const string& p_path, const string& p_file_name, const int64_t p_time_stamp,
		                    const TigerTree& p_tt, bool p_case_convet,
		                    __int64& p_path_id);
		void inc_hitL(const string& p_Path, const string& p_FileName);
	public:
		void flush_hash();
		
		void load_transfer_history(bool p_is_torrent, eTypeTransfer p_type, int p_day);
		void load_transfer_historgam(bool p_is_torrent, eTypeTransfer p_type, CFlyTransferHistogramArray& p_array);
		void save_transfer_history(bool p_is_torrent, eTypeTransfer p_type, const FinishedItemPtr& p_item);
		void delete_transfer_history(const vector<__int64>& p_id_array);
		
		void save_torrent_resume(const libtorrent::sha1_hash& p_sha1, const std::string& p_name, const std::vector<char>& p_resume);
		void load_torrent_resume(libtorrent::session& p_session);
		void delete_torrent_resume(const libtorrent::sha1_hash& p_sha1);
		
		bool merge_mediainfo(const __int64 p_tth_id, const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media);
#ifdef USE_REBUILD_MEDIAINFO
		bool rebuild_mediainfo(const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media, const TTHValue& p_tth);
#endif
		static void errorDB(const string& p_txt);
		
		bool check_tth(const string& fname, __int64 path_id, int64_t aSize, int64_t aTimeStamp, TTHValue& p_out_tth);
		void load_path_cache();
		void scan_path(CFlyDirItemArray& p_directories);
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		int sync_antivirus_db(const string& p_antivirus_db, const uint64_t p_unixtime);
		int get_antivirus_record_count();
		void purge_antivirus_db(const uint64_t p_delete_counter, const uint64_t p_unixtime, bool p_is_clean_cache);
#endif
		size_t get_count_folders();
		void sweep_db();
		void load_dir(__int64 p_path_id, CFlyDirMap& p_dir_map, bool p_is_no_mediainfo);
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
		void sweep_files(__int64 p_path_id, const CFlyDirMap& p_sweep_files);
#endif
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
		void log(const int p_area, const StringMap& p_params);
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
		int32_t load_queue();
		void add_sourceL(const QueueItemPtr& p_QueueItem, const CID& p_cid, const string& p_nick, int p_hub_id);
		bool merge_queue_itemL(QueueItemPtr& p_QueueItem);
		void merge_queue_segmentL(const CFlySegment& p_QueueSegment);
	private:
		void merge_queue_sub_itemsL(QueueItemPtr& p_QueueItem, __int64 p_id);
		void remove_queue_itemL(const __int64 p_id);
		void remove_queue_item_sourcesL(const __int64 p_id, const CID& p_cid);
		void clean_registryL(eTypeSegment p_Segment, __int64 p_tick);
	public:
		void merge_queue_all_items(std::vector<QueueItemPtr>& p_QueueItemArray);
		void merge_queue_all_segments(const CFlySegmentArray& p_QueueSegmentArray);
		void remove_queue_item_array(const std::vector<int64_t>& p_id_array);
		void remove_queue_all_items();
		void load_ignore(StringSet& p_ignores);
		void save_ignore(const StringSet& p_ignores);
		void load_registry(CFlyRegistryMap& p_values, eTypeSegment p_Segment);
		void save_registry(const CFlyRegistryMap& p_values, eTypeSegment p_Segment, bool p_is_cleanup_old_value);
		void clean_registry(eTypeSegment p_Segment, __int64 p_tick);
		
		void set_registry_variable_int64(eTypeSegment p_TypeSegment, __int64 p_value);
		__int64 get_registry_variable_int64(eTypeSegment p_TypeSegment);
		void set_registry_variable_string(eTypeSegment p_TypeSegment, const string& p_value);
		string get_registry_variable_string(eTypeSegment p_TypeSegment);
		template <class T> void load_registry(T& p_values, eTypeSegment p_Segment)
		{
			p_values.clear();
			CFlyRegistryMap l_values;
			load_registry(l_values, p_Segment);
			for (auto k = l_values.cbegin(); k != l_values.cend(); ++k)
			{
				p_values.push_back(Text::toT(k->first));
			}
		}
		template <class T>  void save_registry(const T& p_values, eTypeSegment p_Segment)
		{
			CFlyRegistryMap l_values;
			for (auto i = p_values.cbegin(); i != p_values.cend(); ++i)
			{
				const auto& l_res = l_values.insert(CFlyRegistryMap::value_type(
				                                        Text::fromT(*i),
				                                        CFlyRegistryValue()));
				dcassert(l_res.second);
			}
			save_registry(l_values, p_Segment, true);
		}
		
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		bool load_media_info(const TTHValue& p_tth, CFlyMediaInfo& p_media_info, bool p_only_inform);
		bool find_fly_server_cache(const TTHValue& p_tth, CFlyServerCache& p_value);
		void save_fly_server_cache(const TTHValue& p_tth, const CFlyServerCache& p_value);
#endif
		void add_file(__int64 p_path_id, const string& p_file_name, int64_t p_time_stamp, const TigerTree& p_tth, int64_t p_size, CFlyMediaInfo& p_out_media);
#ifdef STRONG_USE_DHT
		void save_dht_nodes(const std::vector<dht::BootstrapNode>& p_dht_nodes);
		bool load_dht_nodes(std::vector<dht::BootstrapNode>& p_dht_nodes);
		void save_dht_files(const dht::TTHArray& p_dht_files);
		int  find_dht_files(const TTHValue& p_tth, dht::SourceList& p_source_list);
		void check_expiration_dht_files(uint64_t p_Tick);
#endif //STRONG_USE_DHT
		
		void save_geoip(const CFlyLocationIPArray& p_geo_ip);
		void save_p2p_guard(const CFlyP2PGuardArray& p_p2p_guard_ip, const string&  p_manual_marker, int p_type);
		string load_manual_p2p_guard();
		void remove_manual_p2p_guard(const string& p_ip);
		string is_p2p_guard(const uint32_t& p_ip);
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		bool is_avdb_guard(const string& p_nick, int64_t p_share, const uint32_t& p_ip4);
#endif
#ifdef FLYLINKDC_USE_GEO_IP
		void get_country_and_location(uint32_t p_ip, uint16_t& p_country_index, uint32_t& p_location_index, bool p_is_use_only_cache);
		uint16_t get_country_index_from_cache(int16_t p_index)
		{
			dcassert(p_index > 0);
			CFlyFastLock(m_cache_location_cs);
			return m_country_cache[p_index - 1].m_flag_index;
		}
		CFlyLocationDesc get_country_from_cache(uint16_t p_index)
		{
			dcassert(p_index > 0);
			CFlyFastLock(m_cache_location_cs);
			return m_country_cache[p_index - 1];
		}
#endif
		uint16_t get_location_index_from_cache(int32_t p_index)
		{
			dcassert(p_index > 0);
			CFlyFastLock(m_cache_location_cs);
			return m_location_cache_array[p_index - 1].m_flag_index;
		}
		CFlyLocationDesc get_location_from_cache(int32_t p_index)
		{
			dcassert(p_index > 0);
			CFlyFastLock(m_cache_location_cs);
			return m_location_cache_array[p_index - 1];
		}
	private:
#ifdef FLYLINKDC_USE_GEO_IP
		string load_country_locations_p2p_guard_from_db(uint32_t p_ip, uint32_t& p_location_cache_index, uint16_t& p_country_cache_index);
		bool find_cache_country(uint32_t p_ip, uint16_t& p_index);
		bool find_cache_location(uint32_t p_ip, uint32_t& p_location_index, uint16_t& p_flag_index);
		__int64 get_dic_country_id(const string& p_country);
		void clear_dic_cache_country();
#endif
	public:
	
		void save_location(const CFlyLocationIPArray& p_geo_ip);
		__int64 get_dic_location_id(const string& p_location);
		//void clear_dic_cache_location();
		
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		void save_lost_location(const string& p_ip);
		void get_lost_location(std::vector<std::string>& p_lost_ip_array);
#endif
		
#ifdef FLYLINKDC_USE_COLLECT_STAT
		void push_dc_command_statistic(const std::string& p_hub, const std::string& p_command,
		                               const string& p_server, const string& p_port, const string& p_sender_nick);
		void push_event_statistic(const std::string& p_event_type, const std::string& p_event_key,
		                          const string& p_event_value,
		                          const string& p_ip = Util::emptyString,
		                          const string& p_port = Util::emptyString,
		                          const string& p_hub = Util::emptyString,
		                          const string& p_tth = Util::emptyString
		                         );
#endif // FLYLINKDC_USE_COLLECT_STAT
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
		void push_json_statistic(const std::string& p_value, const string& p_type, bool p_is_stat_server);
		void flush_lost_json_statistic(bool& p_is_error);
#endif // FLYLINKDC_USE_GATHER_STATISTICS
		__int64 convert_tth_history();
		static int32_t getCountQueueFiles()
		{
			return g_count_queue_files;
		}
		static int32_t getCountQueueSources()
		{
			return g_count_queue_source;
		}
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
		string get_hub_name(unsigned p_hub_id);
#endif
	private:
		__int64 convert_tth_historyL();
		void delete_queue_sourcesL(const __int64 p_id);
		void convert_fly_hash_block_crate_unicque_tthL(CFlyLogFile& p_convert_log);
		void convert_fly_hash_blockL();
		void convert_fly_hash_block_internalL();
		void clean_fly_hash_blockL();
		
		mutable CriticalSection m_cs;
		// http://leveldb.googlecode.com/svn/trunk/doc/index.html Concurrency
		//  A database may only be opened by one process at a time. The leveldb implementation acquires
		// a lock from the operating system to prevent misuse. Within a single process, the same leveldb::DB
		// object may be safely shared by multiple concurrent threads. I.e., different threads may write into or
		// fetch iterators or call Get on the same database without any external synchronization
		// (the leveldb implementation will automatically do the required synchronization).
		// However other objects (like Iterator and WriteBatch) may require external synchronization.
		// If two threads share such an object, they must protect access to it using their own locking protocol.
		// More details are available in the public header files.
		sqlite3_connection m_flySQLiteDB;
		typedef boost::unordered_map<string, CFlyHashCacheItem> CFlyHashCacheMap;
		CFlyHashCacheMap m_cache_hash_files;
		FastCriticalSection  m_cache_hash_files_cs;
#ifdef FLYLINKDC_USE_LEVELDB
		CFlyLevelDB         m_TTHLevelDB;
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
		CFlyLevelDBCacheIP  m_IPCacheLevelDB;
#endif
#else
		CFlySQLCommand m_get_status_file;
#endif // FLYLINKDC_USE_LEVELDB
		FastCriticalSection m_path_cache_cs;
		CFlyPathCache m_path_cache;
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
	private:
		CFlySQLCommand m_update_identity_stat_get;
		CFlySQLCommand m_update_identity_stat_set;
		CFlySQLCommand m_insert_identity_stat;
		void identity_initL(const string& p_key, const string& p_value, const string& p_hub);
	public:
		void identity_set(string p_key, string p_value, const string& p_hub = "-");
		void identity_get(string p_key, string p_value, const string& p_hub = "-");
#endif // FLYLINKDC_USE_GATHER_IDENTITY_STAT
	private:
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		CFlySQLCommand m_select_fly_server_cache;
		CFlySQLCommand m_insert_fly_server_cache;
		CFlySQLCommand m_delete_mediainfo;
		CFlySQLCommand m_load_mediainfo_ext;
		CFlySQLCommand m_load_mediainfo_ext_only_inform;
		CFlySQLCommand m_load_mediainfo_base;
		CFlySQLCommand m_insert_mediainfo;
		
		void merge_mediainfo_ext(const __int64 l_tth_id, const CFlyMediaInfo& p_media, bool p_delete_old_info);
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
		
		CFlySQLCommand m_update_base_mediainfo;
		
#ifdef STRONG_USE_DHT
		CFlySQLCommand m_load_dht_nodes;
		CFlySQLCommand m_save_dht_nodes;
		CFlySQLCommand m_find_dht_files;
		CFlySQLCommand m_save_dht_files;
		CFlySQLCommand m_check_expiration_dht_files;
		CFlySQLCommand m_delete_dht_nodes;
#endif // STRONG_USE_DHT
		CFlySQLCommand m_fly_hash_block_convert_loop;
		CFlySQLCommand m_fly_hash_block_convert_update;
		CFlySQLCommand m_fly_hash_block_convert_drop_dup;
		CFlySQLCommand m_add_tree_find;
		
		CFlySQLCommand m_select_ratio_load;
		
#ifdef FLYLINKDC_USE_LASTIP_CACHE
		CFlySQLCommand m_select_all_last_ip_and_message_count;
		boost::unordered_map<uint32_t, boost::unordered_map<std::string, CFlyLastIPCacheItem> > m_last_ip_cache;
		FastCriticalSection m_last_ip_cs;
#else
		CFlySQLCommand m_select_last_ip_and_message_count;
		CFlySQLCommand m_select_last_ip;
		CFlySQLCommand m_insert_last_ip_and_message_count;
		CFlySQLCommand m_update_last_ip_and_message_count;
		CFlySQLCommand m_insert_last_ip;
		CFlySQLCommand m_update_last_ip;
		CFlySQLCommand m_insert_message_count;
		CFlySQLCommand m_update_message_count;
#endif // FLYLINKDC_USE_LASTIP_CACHE
		
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		CFlySQLCommand m_insert_antivirus_db;
		CFlySQLCommand m_update_antivirus_db;
		CFlySQLCommand m_find_virus_nick_and_share;
		CFlySQLCommand m_find_virus_nick_and_share_and_ip4;
		
		std::unique_ptr<webrtc::RWLockWrapper> m_virus_cs;
		boost::unordered_set<std::string> m_virus_user;
		boost::unordered_set<int64_t> m_virus_share;
		boost::unordered_set<unsigned long> m_virus_ip4;
		void clear_virus_cacheL();
	public:
		void load_avdb();
		int calc_antivirus_flag(const string& p_nick, const boost::asio::ip::address_v4& p_ip4, int64_t p_share, string& p_virus_path);
		bool is_virus_bot(const string& p_nick, int64_t p_share, const boost::asio::ip::address_v4& p_ip4);
		
#endif
	private:
	
		CFlySQLCommand m_insert_ratio;
		CFlySQLCommand m_update_ratio;
		
#ifdef _DEBUG
		CFlySQLCommand m_check_message_count;
		CFlySQLCommand m_select_store_ip;
#endif
		CFlySQLCommand m_insert_store_all_ip_and_message_count;
		
		CFlySQLCommand m_insert_fly_hash_block;
		//CFlySQLCommand m_update_fly_hash_block;
		CFlySQLCommand m_insert_file;
		CFlySQLCommand m_update_file;
		CFlySQLCommand m_check_tth_sql;
		CFlySQLCommand m_load_dir_sql;
		CFlySQLCommand m_load_dir_sql_without_mediainfo;
		CFlySQLCommand m_set_ftype;
		//CFlySQLCommand m_load_path_cache;
		CFlySQLCommand m_load_path_cache_one_dir;
		CFlySQLCommand m_sweep_dir_sql;
		CFlySQLCommand m_sweep_path_file;
		CFlySQLCommand m_get_path_id;
		CFlySQLCommand m_get_tth_id;
		CFlySQLCommand m_upload_file;
		CFlySQLCommand m_get_tree;
		CFlySQLCommand m_get_blocksize;  // [+] brain-ripper
		CFlySQLCommand m_insert_fly_path;
		CFlySQLCommand m_insert_and_full_update_fly_queue;
		CFlySQLCommand m_update_and_full_update_fly_queue;
		CFlySQLCommand m_select_for_update_fly_queue;
		CFlySQLCommand m_update_segments_fly_queue;
		CFlySQLCommand m_insert_fly_queue_source;
		CFlySQLCommand m_del_fly_queue;
		CFlySQLCommand m_del_fly_queue_source;
		CFlySQLCommand m_del_fly_queue_source_cid;
		CFlySQLCommand m_get_fly_queue;
		CFlySQLCommand m_get_fly_queue_all_source;
		
		CFlySQLCommand m_get_ignores;
		CFlySQLCommand m_insert_ignores;
		CFlySQLCommand m_delete_ignores;
		
		CFlySQLCommand m_select_fly_dic;
		CFlySQLCommand m_insert_fly_dic;
		CFlySQLCommand m_get_registry;
		CFlySQLCommand m_insert_registry;
		CFlySQLCommand m_update_registry;
		CFlySQLCommand m_delete_registry;
		
		FastCriticalSection m_cache_location_cs;
		vector<CFlyLocationDesc> m_location_cache_array;
		struct CFlyCacheIPInfo
		{
			string m_description_p2p_guard;
			uint32_t m_location_cache_index;
			uint16_t m_country_cache_index;
			uint16_t m_flag_location_index;
			CFlyCacheIPInfo() : m_location_cache_index(0), m_country_cache_index(0), m_flag_location_index(0)
			{
			}
		};
		boost::unordered_map<uint32_t, CFlyCacheIPInfo> m_ip_info_cache;
		
		int m_count_fly_location_ip_record;
		bool is_fly_location_ip_valid() const
		{
			return m_count_fly_location_ip_record > 5000;
		}
		CFlySQLCommand m_insert_location;
		CFlySQLCommand m_delete_location;
		
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		CFlySQLCommand m_select_count_location;
		CFlySQLCommand m_select_location_lost;
		CFlySQLCommand m_update_location_lost;
		CFlySQLCommand m_insert_location_lost;
		boost::unordered_set<string> m_lost_location_cache;
#endif
#ifdef FLYLINKDC_USE_GEO_IP
		CFlySQLCommand m_select_country_and_location;
		// TODO CFlySQLCommand m_select_only_location;
		CFlySQLCommand m_insert_geoip;
		CFlySQLCommand m_delete_geoip;
		vector<CFlyLocationDesc> m_country_cache;
#endif
#ifdef _DEBUG
		boost::unordered_map<uint32_t, unsigned> m_count_ip_sql_query_guard;
#endif
		CFlySQLCommand m_select_manual_p2p_guard;
		CFlySQLCommand m_delete_manual_p2p_guard;
		CFlySQLCommand m_delete_p2p_guard;
		CFlySQLCommand m_insert_p2p_guard;
		
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
		CFlySQLCommand m_select_statistic_json;
		CFlySQLCommand m_delete_statistic_json;
		CFlySQLCommand m_insert_statistic_json;
#endif  // FLYLINKDC_USE_GATHER_STATISTICS
		
#ifdef FLYLINKDC_USE_COLLECT_STAT
		CFlySQLCommand m_insert_statistic_dc_command;
		CFlySQLCommand m_select_statistic_dc_command;
		
		CFlySQLCommand m_insert_event_stat;
#endif
		
		CFlySQLCommand m_insert_fly_message;
		static inline const string& getString(const StringMap& p_params, const char* p_type)
		{
			const auto& i = p_params.find(p_type);
			if (i != p_params.end())
				return i->second;
			else
				return Util::emptyString;
		}
		typedef boost::unordered_map<string, __int64> CFlyCacheDIC;
		std::vector<CFlyCacheDIC> m_DIC;
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
		typedef boost::unordered_map<unsigned, string> CFlyCacheDICName;
		CFlyCacheDICName m_HubNameMap;
		FastCriticalSection m_hub_dic_fcs;
#endif
		
		
		CFlySQLCommand m_select_transfer_day;
		CFlySQLCommand m_select_transfer_day_torrent;
		CFlySQLCommand m_select_transfer_tth;
		CFlySQLCommand m_select_transfer_histrogram;
		CFlySQLCommand m_select_transfer_histrogram_torrent;
		CFlySQLCommand m_select_transfer_convert_leveldb;
		CFlySQLCommand m_insert_transfer;
		CFlySQLCommand m_insert_transfer_torrent;
		CFlySQLCommand m_delete_transfer;
		
		CFlySQLCommand m_insert_resume_torrent;
		CFlySQLCommand m_select_resume_torrent;
		CFlySQLCommand m_delete_resume_torrent;
		
		__int64 find_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_cache_result);
		__int64 get_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_create);
		//void clear_dic_cache(const eTypeDIC p_DIC);
		
		bool safeAlter(const char* p_sql, bool p_is_all_log = false);
		void pragma_executor(const char* p_pragma);
		void update_file_infoL(const string& p_fname, __int64 p_path_id,
		                       int64_t p_Size, int64_t p_TimeStamp, __int64 p_tth_id);
		__int64 get_tth_idL(const TTHValue& p_tth);
		
		__int64 m_queue_id;
		__int64 m_last_path_id;
		string  m_last_path;
		int     m_convert_ftype_stop_key;
		size_t  m_count_json_stat;
		static int32_t g_count_queue_source;
		static int32_t g_count_queue_files;
		boost::unordered_map<TTHValue, TigerTree> m_tiger_tree_cache;
		FastCriticalSection m_tth_cache_cs;
};
#endif
