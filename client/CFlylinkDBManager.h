//-----------------------------------------------------------------------------
//(c) 2007-2013 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#ifndef CFlylinkDBManager_H
#define CFlylinkDBManager_H

#include <vector>
#include <boost/unordered/unordered_map.hpp>
#include "QueueItem.h"
#include "Singleton.h"
#include "Thread.h"
#include "sqlite/sqlite3x.hpp"
#include "../dht/DHTType.h"
#include "CFlyMediaInfo.h"

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
		bool get_value(const void* p_key, size_t p_key_len, bool p_cache, string& p_result);
		bool set_value(const void* p_key, size_t p_key_len, const void* p_val, size_t p_val_len);
		bool get_value(const TTHValue& p_tth, bool p_cache, string& p_result)
		{
			return get_value(p_tth.data, p_tth.BYTES, p_cache, p_result);
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
	int64_t m_dic_country_location_ip;
	uint32_t m_start_ip;
	uint32_t m_stop_ip;
	uint16_t m_flag_index;
	CFlyLocationIP() : m_dic_country_location_ip(0), m_start_ip(0), m_stop_ip(0), m_flag_index(0)
	{
	}
};
struct CFlyLocationDesc : public CFlyLocationIP
{
	tstring m_description;
};
typedef std::vector<CFlyLocationIP> CFlyLocationIPArray;

struct CFlyFileInfo
{
	int64_t  m_size;
	int64_t  m_TimeStamp;
	TTHValue m_tth;
	uint32_t m_hit;
	int64_t m_StampShare;
	CFlyMediaInfo m_media;
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
	bool     m_found;
#endif
	bool     m_recalc_ftype;
	char     m_ftype;
};
typedef boost::unordered_map<string, CFlyFileInfo> CFlyDirMap;
struct CFlyPathItem
{
	__int64 m_path_id;
	bool    m_found;
};
enum eTypeSegment
{
	e_ExtraSlot = 1,
	e_RecentHub = 2,
	e_SearchHistory = 3,
	e_TimeStampGeoIP = 4,
	e_TimeStampCustomLocation = 5,
	e_CMDDebugFilterState = 6
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
typedef boost::unordered_map<string, CFlyRegistryValue> CFlyRegistryMap;
typedef boost::unordered_map<string, CFlyPathItem> CFlyPathCache;
class CFlylinkDBManager : public Singleton<CFlylinkDBManager>
{
	public:
		CFlylinkDBManager();
		~CFlylinkDBManager();
		void push_download_tth(const TTHValue& p_tth);
		void push_add_share_tth_(const TTHValue& p_tth);
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		void store_all_ratio_and_last_ip(uint32_t p_dic_hub,
		                                 const string& p_nick,
		                                 const CFlyUploadDownloadMap& p_upload_download_stats,
		                                 const string& p_last_ip);
		uint32_t get_dic_hub_id(const string& p_hub);
		uint32_t get_ip_id(const string& p_ip);
		void load_global_ratio();
		CFlyRatioItem load_ratio(uint32_t p_hub_id, const string& p_nick, CFlyUserRatioInfo& p_ratio_info, const string& p_last_ip);
		string load_last_ip(uint32_t p_hub_id, const string& p_nick);
		
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
		
		const TTHValue* get_tth(const __int64 p_tth_id);
		bool getTree(const TTHValue& p_root, TigerTree& p_tt);
		unsigned __int64 getBlockSize(const TTHValue& p_root, __int64 p_size);
		__int64 get_path_id(string p_path, bool p_create, bool p_case_convet);
		__int64 addTree(const TigerTree& tt);
		const TTHValue* findTTH(const string& aPath, const string& aFileName);
		
		__int64 merge_file(const string& p_path, const string& p_file_name, const int64_t p_time_stamp,
		                   const TigerTree& p_tt, bool p_case_convet,
		                   __int64& p_path_id);
		bool merge_mediainfo(const __int64 p_tth_id, const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media);
#ifdef USE_REBUILD_MEDIAINFO
		bool rebuild_mediainfo(const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media, const TTHValue& p_tth);
#endif
		static void errorDB(const string& p_txt);
	public:
		void Hit(const string& p_Path, const string& p_FileName);
		bool checkTTH(const string& fname, __int64 path_id, int64_t aSize, int64_t aTimeStamp, TTHValue& p_out_tth);
		void LoadPathCache();
		void SweepPath();
		void LoadDir(__int64 p_path_id, CFlyDirMap& p_dir_map);
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
		void SweepFiles(__int64 p_path_id, const CFlyDirMap& p_sweep_files);
#endif
		void log(const int p_area, const StringMap& p_params);
		size_t load_queue();
		void addSource(const QueueItemPtr& p_QueueItem, const CID& p_cid, const string& p_nick/*, const string& p_hub_hint*/);
		bool merge_queue_item(QueueItemPtr& p_QueueItem);
		void remove_queue_item(const __int64 p_id);
		void load_ignore(StringSet& p_ignores);
		void save_ignore(const StringSet& p_ignores);
		void load_registry(CFlyRegistryMap& p_values, int p_Segment);
		void save_registry(const CFlyRegistryMap& p_values, int p_Segment);
		void load_registry(TStringList& p_values, int p_Segment);
		void set_registry_variable_int64(eTypeSegment p_TypeSegment, __int64 p_value);
		__int64 get_registry_variable_int64(eTypeSegment p_TypeSegment);
		void save_registry(const TStringList& p_values, int p_Segment);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		
		__int64 load_media_info(const TTHValue& p_tth, CFlyMediaInfo& p_media_info, bool p_only_inform);
		bool find_fly_server_cache(const TTHValue& p_tth, CFlyServerCache& p_value);
		void save_fly_server_cache(const TTHValue& p_tth, const CFlyServerCache& p_value);
#endif
#ifdef STRONG_USE_DHT
		void save_dht_nodes(const std::vector<dht::BootstrapNode>& p_dht_nodes);
		bool load_dht_nodes(std::vector<dht::BootstrapNode>& p_dht_nodes);
		void save_dht_files(const dht::TTHArray& p_dht_files);
		int  find_dht_files(const TTHValue& p_tth, dht::SourceList& p_source_list);
		void check_expiration_dht_files(uint64_t p_Tick);
#endif //STRONG_USE_DHT
		
		void save_geoip(const CFlyLocationIPArray& p_geo_ip);
		void get_country(uint32_t p_ip, uint8_t& p_index);
		CFlyLocationDesc get_country_from_cache(uint8_t p_index)
		{
			FastLock l(m_cache_location_cs);
			return m_country_cache[p_index - 1];
		}
		CFlyLocationDesc get_location_from_cache(uint32_t p_index)
		{
			FastLock l(m_cache_location_cs);
			return m_location_cache[p_index - 1];
		}
	private:
		uint8_t  get_country_sqlite(uint32_t p_ip, CFlyLocationDesc& p_location);
		bool find_cache_countryL(uint32_t p_ip, uint8_t& p_index);
		uint16_t find_cache_locationL(uint32_t p_ip, uint32_t& p_index);
	public:
		__int64 get_dic_country_id(const string& p_country);
		
		void save_location(const CFlyLocationIPArray& p_geo_ip);
		void get_location(uint32_t p_ip, uint32_t& p_index);
		void get_location_sql(uint32_t p_ip, uint32_t& p_index);
		__int64 get_dic_location_id(const string& p_location);
		
		void save_lost_location(const string& p_ip);
		void get_lost_location(std::vector<std::string>& p_lost_ip_array);
		
		void push_json_statistic(const std::string& p_value);
		void flush_lost_json_statistic();
		
		__int64 convert_tth_history();
		static size_t getCountQueueSources()
		{
			return g_count_queue_source;
		}
	private:
		void delete_queue_sources(const __int64 p_id);
		
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
		
#ifdef FLYLINKDC_USE_LEVELDB
		FastCriticalSection    m_leveldb_cs;
		CFlyLevelDB        m_flyLevelDB;
#endif // FLYLINKDC_USE_LEVELDB
		
		CFlyPathCache m_path_cache;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		auto_ptr<sqlite3_command> m_select_fly_server_cache;
		auto_ptr<sqlite3_command> m_insert_fly_server_cache;
		auto_ptr<sqlite3_command> m_delete_mediainfo;
		auto_ptr<sqlite3_command> m_load_mediainfo_ext;
		auto_ptr<sqlite3_command> m_load_mediainfo_ext_only_inform;
		auto_ptr<sqlite3_command> m_load_mediainfo_base;
		void merge_mediainfo_ext(const __int64 l_tth_id, const CFlyMediaInfo& p_media, bool p_delete_old_info);
#endif
		
#ifdef STRONG_USE_DHT
		auto_ptr<sqlite3_command> m_load_dht_nodes;
		auto_ptr<sqlite3_command> m_save_dht_nodes;
		auto_ptr<sqlite3_command> m_find_dht_files;
		auto_ptr<sqlite3_command> m_save_dht_files;
		auto_ptr<sqlite3_command> m_check_expiration_dht_files;
		auto_ptr<sqlite3_command> m_delete_dht_nodes;
#endif // STRONG_USE_DHT
		auto_ptr<sqlite3_command> m_add_tree_find;
		auto_ptr<sqlite3_command> m_select_ratio_load;
		auto_ptr<sqlite3_command> m_select_last_ip;
		auto_ptr<sqlite3_command> m_insert_ratio;
		auto_ptr<sqlite3_command> m_insert_store_ip;
		auto_ptr<sqlite3_command> m_ins_fly_hash_block;
		auto_ptr<sqlite3_command> m_insert_file;
		auto_ptr<sqlite3_command> m_update_base_mediainfo;
		
		auto_ptr<sqlite3_command> m_insert_mediainfo;
		
		auto_ptr<sqlite3_command> m_update_file;
		auto_ptr<sqlite3_command> m_check_tth_sql;
		auto_ptr<sqlite3_command> m_load_dir_sql;
		auto_ptr<sqlite3_command> m_set_ftype;
		auto_ptr<sqlite3_command> m_load_path_cache;
		auto_ptr<sqlite3_command> m_sweep_dir_sql;
		auto_ptr<sqlite3_command> m_sweep_path;
		auto_ptr<sqlite3_command> m_sweep_path_file;
		auto_ptr<sqlite3_command> m_get_path_id;
		auto_ptr<sqlite3_command> m_get_tth_id;
		auto_ptr<sqlite3_command> m_findTTH;
		auto_ptr<sqlite3_command> m_get_tth;
		auto_ptr<sqlite3_command> m_get_status_file;
		auto_ptr<sqlite3_command> m_upload_file;
		auto_ptr<sqlite3_command> m_get_tree;
		auto_ptr<sqlite3_command> m_get_blocksize;  // [+] brain-ripper
		auto_ptr<sqlite3_command> m_insert_fly_hash;
		auto_ptr<sqlite3_command> m_insert_fly_path;
		auto_ptr<sqlite3_command> m_insert_fly_tth;
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
		std::unordered_set<string> m_lost_location_cache;
		
		auto_ptr<sqlite3_command> m_select_statistic_json;
		auto_ptr<sqlite3_command> m_delete_statistic_json;
		auto_ptr<sqlite3_command> m_insert_statistic_json;
		
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
		vector<CFlyCacheDIC> m_DIC;
		
		__int64 findDIC_ID(const string& p_name, const eTypeDIC p_DIC, bool p_cache_result);
		__int64 getDIC_ID(const string& p_name, const eTypeDIC p_DIC, bool p_create);
		
		
		bool safeAlter(const char* p_sql);
		void pragma_executor(const char* p_pragma);
		void updateFileInfo(const string& p_fname, __int64 p_path_id,
		                    int64_t p_Size, int64_t p_TimeStamp, __int64 p_tth_id);
		__int64 get_tth_id(const TTHValue& p_tth, bool p_create);
		
		__int64 m_queue_id;
		__int64 m_last_path_id;
		string  m_last_path;
		int     m_convert_ftype_stop_key;
		static size_t g_count_queue_source;
};
#endif
