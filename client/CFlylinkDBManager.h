//-----------------------------------------------------------------------------
//(c) 2007-2014 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#ifndef CFlylinkDBManager_H
#define CFlylinkDBManager_H

#include <vector>
#include <boost/unordered/unordered_map.hpp>
#include "QueueItem.h"
#include "Singleton.h"
#include "CFlyThread.h"
#include "sqlite/sqlite3x.hpp"
#include "../dht/DHTType.h"
#include "CFlyMediaInfo.h"
#include "LogManager.h"

#define FLYLINKDC_USE_LEVELDB // https://code.google.com/p/flylinkdc/issues/detail?id=1097

#ifdef FLYLINKDC_USE_LEVELDB

#include "leveldb/status.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/cache.h"
#include "leveldb/filter_policy.h"

#endif

using std::auto_ptr;
using sqlite3x::sqlite3_connection;
using sqlite3x::sqlite3_command;
using sqlite3x::sqlite3_reader;
using sqlite3x::sqlite3_transaction;
using sqlite3x::database_error;

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
		
		bool open_level_db(const string& p_db_name);
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
#endif // FLYLINKDC_USE_LEVELDB


enum eTypeDIC
{
	e_DIC_HUB = 1,
	e_DIC_NICK = 2,
	e_DIC_IP = 3,
	e_DIC_COUNTRY = 4,
	e_DIC_LOCATION = 5,
	e_DIC_LAST
};

struct CFlyLocationIP
{
	uint32_t m_start_ip;
	uint32_t m_stop_ip;
	uint16_t m_flag_index;
	string m_location;
	CFlyLocationIP()
	{
	}
	CFlyLocationIP(const std::string& p_location, uint32_t p_start_ip, uint32_t p_stop_ip, uint16_t p_flag_index) : m_location(p_location)
		, m_start_ip(p_start_ip), m_stop_ip(p_stop_ip), m_flag_index(p_flag_index)
	{
	}
};
struct CFlyLocationDesc : public CFlyLocationIP
{
	tstring m_description;
};
typedef std::vector<CFlyLocationIP> CFlyLocationIPArray;

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
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
	bool     m_found;
#endif
	bool     m_recalc_ftype;
	char     m_ftype;
	CFlyFileInfo()
	{
	}
	~CFlyFileInfo()
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
	e_IsTTHLevelDBConvert = 9,
	e_IncopatibleSoftwareList = 10,
	// 11, - не занимать
	e_DeleteCounterAntivirusDB = 12,
	e_TimeStampAntivirusDB = 13
};
struct CFlyRegistryValue
{
	string m_val_str;
	__int64  m_val_int64;
	CFlyRegistryValue(__int64 p_val_int64 = 0)
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
typedef boost::unordered_map<string, CFlyRegistryValue> CFlyRegistryMap;
typedef boost::unordered_map<string, CFlyPathItem> CFlyPathCache;
class CFlylinkDBManager : public Singleton<CFlylinkDBManager>
{
	public:
		CFlylinkDBManager();
		~CFlylinkDBManager();
		void shutdown();
		void push_download_tth(const TTHValue& p_tth);
		void push_add_share_tth(const TTHValue& p_tth);
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		void store_all_ratio_and_last_ip(uint32_t p_hub_id,
		                                 const string& p_nick,
		                                 const CFlyUploadDownloadMap* p_upload_download_stats,
		                                 const uint32_t p_message_count,
		                                 const boost::asio::ip::address_v4& p_last_ip);
		uint32_t get_dic_hub_id(const string& p_hub);
		void load_global_ratio();
		void load_all_hub_into_cacheL();
#ifdef _DEBUG
		bool m_is_load_global_ratio;
#endif
		CFlyRatioItem load_ratio(uint32_t p_hub_id, const string& p_nick, CFlyUserRatioInfo& p_ratio_info, const  boost::asio::ip::address_v4& p_last_ip);
		bool load_last_ip_and_user_stat(uint32_t p_hub_id, const string& p_nick, uint32_t& p_message_count, boost::asio::ip::address_v4& p_last_ip);
		void update_last_ip(uint32_t p_hub_id, const string& p_nick, const boost::asio::ip::address_v4& p_last_ip);
	private:
		void update_last_ip_deferredL(uint32_t p_hub_id, const string& p_nick, uint32_t p_message_count, const boost::asio::ip::address_v4& p_last_ip);
		void flush_all_last_ip_and_message_count();
		void add_tree_internal_bind_and_executeL(sqlite3_command* p_sql, const TigerTree& p_tt);
		__int64 add_treeL(const TigerTree& p_tt);
		__int64 get_path_idL(string p_path, bool p_create, bool p_case_convet, bool& p_is_no_mediainfo, bool p_sweep_path);
		__int64 find_path_idL(const string& p_path);
		__int64 create_path_idL(const string& p_path, bool p_is_skip_dup_val_index);
	public:
		CFlyGlobalRatioItem  m_global_ratio;
		double get_ratio() const;
		tstring get_ratioW() const;
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
		
		bool is_table_exists(const string& p_table_name);
		
		enum FileStatus // [+] IRainman fix
		{
			UNKNOWN = 0,
			PREVIOUSLY_DOWNLOADED = 1,
			PREVIOUSLY_BEEN_IN_SHARE = 2,
			COUPLE = PREVIOUSLY_DOWNLOADED | PREVIOUSLY_BEEN_IN_SHARE
		};
		
		FileStatus get_status_file(const TTHValue& p_tth);
		
		bool getTree(const TTHValue& p_root, TigerTree& p_tt);
		unsigned __int64 getBlockSizeSQL(const TTHValue& p_root, __int64 p_size);
		__int64 get_path_id(string p_path, bool p_create, bool p_case_convet, bool& p_is_no_mediainfo, bool p_sweep_path);
		void add_tree(const TigerTree& p_tt);
		__int64 merge_file(const string& p_path, const string& p_file_name, const int64_t p_time_stamp,
		                   const TigerTree& p_tt, bool p_case_convet,
		                   __int64& p_path_id);
	private:
		void prepare_scan_folderL(const tstring& p_path);
		bool merge_mediainfoL(const __int64 p_tth_id, const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media);
		__int64 merge_fileL(const string& p_path, const string& p_file_name, const int64_t p_time_stamp,
		                    const TigerTree& p_tt, bool p_case_convet,
		                    __int64& p_path_id);
	public:
		void flush_hash();
		
		bool merge_mediainfo(const __int64 p_tth_id, const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media);
#ifdef USE_REBUILD_MEDIAINFO
		bool rebuild_mediainfo(const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media, const TTHValue& p_tth);
#endif
		static void errorDB(const string& p_txt);
		
		void incHit(const string& p_Path, const string& p_FileName);
		bool checkTTH(const string& fname, __int64 path_id, int64_t aSize, int64_t aTimeStamp, TTHValue& p_out_tth);
		void load_path_cache();
		void scan_path(CFlyDirItemArray& p_directories);
		int sync_antivirus_db(const string& p_antivirus_db, uint64_t p_unixtime);
		void purge_antivirus_db(uint64_t p_delete_counter);
		size_t get_count_folders()
		{
			Lock l(m_cs);
			return m_path_cache.size();
		}
		void sweep_db();
		void LoadDir(__int64 p_path_id, CFlyDirMap& p_dir_map, bool p_is_no_mediainfo);
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
		void SweepFiles(__int64 p_path_id, const CFlyDirMap& p_sweep_files);
#endif
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
		void log(const int p_area, const StringMap& p_params);
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
		size_t load_queue();
		void addSource(const QueueItemPtr& p_QueueItem, const CID& p_cid, const string& p_nick/*, const string& p_hub_hint*/);
		bool merge_queue_itemL(QueueItemPtr& p_QueueItem);
		void remove_queue_item(const __int64 p_id);
		void load_ignore(StringSet& p_ignores);
		void save_ignore(const StringSet& p_ignores);
		void load_registry(CFlyRegistryMap& p_values, int p_Segment);
		void save_registry(const CFlyRegistryMap& p_values, int p_Segment, bool p_is_cleanup_old_value);
		void load_registry(TStringList& p_values, int p_Segment);
		void set_registry_variable_int64(eTypeSegment p_TypeSegment, __int64 p_value);
		__int64 get_registry_variable_int64(eTypeSegment p_TypeSegment);
		void set_registry_variable_string(eTypeSegment p_TypeSegment, const string& p_value);
		string get_registry_variable_string(eTypeSegment p_TypeSegment);
		void save_registry(const TStringList& p_values, int p_Segment);
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
		void get_country(uint32_t p_ip, uint16_t& p_index);
		uint16_t get_country_index_from_cache(uint16_t p_index)
		{
			dcassert(p_index > 0);
			FastLock l(m_cache_location_cs);
			return m_country_cache[p_index - 1].m_flag_index;
		}
		CFlyLocationDesc get_country_from_cache(uint16_t p_index)
		{
			dcassert(p_index > 0);
			FastLock l(m_cache_location_cs);
			return m_country_cache[p_index - 1];
		}
		uint16_t get_location_index_from_cache(int32_t p_index)
		{
			dcassert(p_index > 0);
			FastLock l(m_cache_location_cs);
			return m_location_cache[p_index - 1].m_flag_index;
		}
		CFlyLocationDesc get_location_from_cache(int32_t p_index)
		{
			dcassert(p_index > 0);
			FastLock l(m_cache_location_cs);
			return m_location_cache[p_index - 1];
		}
	private:
		uint8_t  get_country_sqlite(uint32_t p_ip, CFlyLocationDesc& p_location);
		bool find_cache_countryL(uint32_t p_ip, uint16_t& p_index);
		uint16_t find_cache_locationL(uint32_t p_ip, int32_t& p_index);
	public:
		__int64 get_dic_country_id(const string& p_country);
		void clear_dic_cache_country();
		
		void save_location(const CFlyLocationIPArray& p_geo_ip);
		void get_location(uint32_t p_ip, int32_t& p_index);
		void get_location_sql(uint32_t p_ip, int32_t& p_index);
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
		void push_json_statistic(const std::string& p_value);
		void flush_lost_json_statistic();
#endif // FLYLINKDC_USE_GATHER_STATISTICS
		void clear_tiger_tree_cache(const TTHValue& p_root);
		__int64 convert_tth_history();
		static size_t getCountQueueSources()
		{
			return g_count_queue_source;
		}
	private:
		void delete_queue_sources(const __int64 p_id);
		void convert_fly_hash_block_crate_unicque_tthL(CFlyLogFile& p_convert_log);
		void convert_fly_hash_blockL();
		void convert_fly_hash_block_internalL();
		void clean_fly_hash_blockL();
		
		mutable CriticalSection m_cs;
		// TODO CriticalSection m_leveldb_cs;
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
		boost::unordered_map<string, CFlyHashCacheItem> m_cache_hash_files;
		
#ifdef FLYLINKDC_USE_LEVELDB
		// FastCriticalSection    m_leveldb_cs;
		CFlyLevelDB        m_flyLevelDB;
#else
		auto_ptr<sqlite3_command> m_get_status_file;
#endif // FLYLINKDC_USE_LEVELDB
		
		CFlyPathCache m_path_cache;
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
	private:
		auto_ptr<sqlite3_command> m_update_identity_stat_get;
		auto_ptr<sqlite3_command> m_update_identity_stat_set;
		auto_ptr<sqlite3_command> m_insert_identity_stat;
		void identity_initL(const string& p_key, const string& p_value, const string& p_hub);
	public:
		void identity_set(string p_key, string p_value, const string& p_hub = "-");
		void identity_get(string p_key, string p_value, const string& p_hub = "-");
#endif // FLYLINKDC_USE_GATHER_IDENTITY_STAT
	private:
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		auto_ptr<sqlite3_command> m_select_fly_server_cache;
		auto_ptr<sqlite3_command> m_insert_fly_server_cache;
		auto_ptr<sqlite3_command> m_delete_mediainfo;
		auto_ptr<sqlite3_command> m_load_mediainfo_ext;
		auto_ptr<sqlite3_command> m_load_mediainfo_ext_only_inform;
		auto_ptr<sqlite3_command> m_load_mediainfo_base;
		auto_ptr<sqlite3_command> m_insert_mediainfo;
		
		void merge_mediainfo_ext(const __int64 l_tth_id, const CFlyMediaInfo& p_media, bool p_delete_old_info);
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
		
		auto_ptr<sqlite3_command> m_update_base_mediainfo;
		
#ifdef STRONG_USE_DHT
		auto_ptr<sqlite3_command> m_load_dht_nodes;
		auto_ptr<sqlite3_command> m_save_dht_nodes;
		auto_ptr<sqlite3_command> m_find_dht_files;
		auto_ptr<sqlite3_command> m_save_dht_files;
		auto_ptr<sqlite3_command> m_check_expiration_dht_files;
		auto_ptr<sqlite3_command> m_delete_dht_nodes;
#endif // STRONG_USE_DHT
		auto_ptr<sqlite3_command> m_fly_hash_block_convert_loop;
		auto_ptr<sqlite3_command> m_fly_hash_block_convert_update;
		auto_ptr<sqlite3_command> m_fly_hash_block_convert_drop_dup;
		auto_ptr<sqlite3_command> m_add_tree_find;
		auto_ptr<sqlite3_command> m_select_ratio_load;
		//auto_ptr<sqlite3_command> m_select_last_ip_and_message_count;
		auto_ptr<sqlite3_command> m_select_all_last_ip_and_message_count;
		auto_ptr<sqlite3_command> m_select_antivirus_db;
		auto_ptr<sqlite3_command> m_find_virus_nick;
		
		boost::unordered_map<uint32_t, boost::unordered_map<std::string, CFlyLastIPCacheItem> > m_last_ip_cache;
		CriticalSection m_antivirus_cs;
		boost::unordered_set<std::string> m_virus_user;
		boost::unordered_set<int64_t> m_virus_share;
		boost::unordered_set<unsigned long> m_virus_ip4;
	public:
		int calc_antivirus_flag(const string& p_nick, const boost::asio::ip::address_v4& p_ip4, int64_t p_share);
	private:
	
		auto_ptr<sqlite3_command> m_insert_ratio;
		
#ifdef _DEBUG
		auto_ptr<sqlite3_command> m_check_message_count;
		auto_ptr<sqlite3_command> m_select_store_ip;
#endif
		//auto_ptr<sqlite3_command> m_insert_store_ip;
		//auto_ptr<sqlite3_command> m_insert_store_message_count;
		//auto_ptr<sqlite3_command> m_insert_store_ip_and_message_count;
		auto_ptr<sqlite3_command> m_insert_store_all_ip_and_message_count;
		
		auto_ptr<sqlite3_command> m_insert_fly_hash_block;
		auto_ptr<sqlite3_command> m_update_fly_hash_block;
		auto_ptr<sqlite3_command> m_insert_file;
		auto_ptr<sqlite3_command> m_update_file;
		auto_ptr<sqlite3_command> m_check_tth_sql;
		auto_ptr<sqlite3_command> m_load_dir_sql;
		auto_ptr<sqlite3_command> m_load_dir_sql_without_mediainfo;
		auto_ptr<sqlite3_command> m_set_ftype;
		auto_ptr<sqlite3_command> m_load_path_cache;
		auto_ptr<sqlite3_command> m_sweep_dir_sql;
		auto_ptr<sqlite3_command> m_sweep_path_file;
		auto_ptr<sqlite3_command> m_get_path_id;
		auto_ptr<sqlite3_command> m_get_tth_id;
		auto_ptr<sqlite3_command> m_upload_file;
		auto_ptr<sqlite3_command> m_get_tree;
		auto_ptr<sqlite3_command> m_get_blocksize;  // [+] brain-ripper
		auto_ptr<sqlite3_command> m_insert_fly_hash;
		auto_ptr<sqlite3_command> m_insert_fly_path;
		auto_ptr<sqlite3_command> m_insert_fly_queue;
		auto_ptr<sqlite3_command> m_insert_fly_queue_source;
		auto_ptr<sqlite3_command> m_del_fly_queue;
		auto_ptr<sqlite3_command> m_del_fly_queue_source;
		auto_ptr<sqlite3_command> m_get_fly_queue;
		auto_ptr<sqlite3_command> m_get_fly_queue_source;
		
		auto_ptr<sqlite3_command> m_get_ignores;
		auto_ptr<sqlite3_command> m_insert_ignores;
		auto_ptr<sqlite3_command> m_delete_ignores;
		
		auto_ptr<sqlite3_command> m_select_fly_dic;
		auto_ptr<sqlite3_command> m_insert_fly_dic;
		auto_ptr<sqlite3_command> m_get_registry;
		auto_ptr<sqlite3_command> m_insert_registry;
		auto_ptr<sqlite3_command> m_delete_registry;
		
		auto_ptr<sqlite3_command> m_select_geoip;
		auto_ptr<sqlite3_command> m_insert_geoip;
		auto_ptr<sqlite3_command> m_delete_geoip;
		
		auto_ptr<sqlite3_command> m_merge_antivirus_db;
		auto_ptr<sqlite3_command> m_delete_antivirus_db;
		
		auto_ptr<sqlite3_command> m_select_location;
		auto_ptr<sqlite3_command> m_select_count_location;
		
		FastCriticalSection m_cache_location_cs;
		vector<CFlyLocationDesc> m_country_cache;
		vector<CFlyLocationDesc> m_location_cache;
		
		int m_count_fly_location_ip_record;
		bool is_fly_location_ip_valid() const
		{
			return m_count_fly_location_ip_record > 5000;
		}
		auto_ptr<sqlite3_command> m_insert_location;
		auto_ptr<sqlite3_command> m_delete_location;
		
		auto_ptr<sqlite3_command> m_select_location_lost;
		auto_ptr<sqlite3_command> m_update_location_lost;
		auto_ptr<sqlite3_command> m_insert_location_lost;
		boost::unordered_set<string> m_lost_location_cache;
		
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
		auto_ptr<sqlite3_command> m_select_statistic_json;
		auto_ptr<sqlite3_command> m_delete_statistic_json;
		auto_ptr<sqlite3_command> m_insert_statistic_json;
#endif  // FLYLINKDC_USE_GATHER_STATISTICS
		
#ifdef FLYLINKDC_USE_COLLECT_STAT
		auto_ptr<sqlite3_command> m_insert_statistic_dc_command;
		auto_ptr<sqlite3_command> m_select_statistic_dc_command;
		
		auto_ptr<sqlite3_command> m_insert_event_stat;
#endif
		
		auto_ptr<sqlite3_command> m_insert_fly_message;
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
		
		__int64 find_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_cache_result);
		__int64 get_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_create);
		//void clear_dic_cache(const eTypeDIC p_DIC);
		
		
		bool safeAlter(const char* p_sql);
		void pragma_executor(const char* p_pragma);
		void updateFileInfo(const string& p_fname, __int64 p_path_id,
		                    int64_t p_Size, int64_t p_TimeStamp, __int64 p_tth_id);
		__int64 get_tth_idL(const TTHValue& p_tth);
		
		__int64 m_queue_id;
		__int64 m_last_path_id;
		string  m_last_path;
		int     m_convert_ftype_stop_key;
		size_t  m_count_json_stat;
		static size_t g_count_queue_source;
		std::unordered_map<TTHValue, TigerTree> m_tiger_tree_cache; // http://code.google.com/p/flylinkdc/issues/detail?id=1418
};
#endif
