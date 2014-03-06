//-----------------------------------------------------------------------------
//(c) 2007-2013 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#include "stdinc.h"

#include "SettingsManager.h"
#include "LogManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "ConnectionManager.h"
#include "StringTokenizer.h"
#include "../FlyFeatures/flyServer.h"

using sqlite3x::database_error;
using sqlite3x::sqlite3_transaction;
using sqlite3x::sqlite3_reader;

bool g_DisableSQLJournal    = false;
bool g_UseWALJournal        = false;
bool g_EnableSQLtrace       = false; // http://www.sqlite.org/c3ref/profile.html
bool g_UseSynchronousOff    = false;

size_t CFlylinkDBManager::g_count_queue_source = 0;
/*
bool CAccountManager::IntegrityCheck ()
{
    CRegistryResult result;
    //Select all our required information from the accounts database
    bool bOk = m_pDatabaseManager->QueryWithResultf ( m_hDbConnection, &result, "PRAGMA integrity_check" );

    // Get result as a string
    SString strResult;
    if ( result.nRows && result.nColumns )
    {
        CRegistryResultCell& cell = result.Data[0][0];
        if ( cell.nType == SQLITE_TEXT )
            strResult = std::string ( (const char *)cell.pVal, cell.nLength - 1 );
    }

    // Process result
    if ( !bOk || !strResult.BeginsWithI ( "ok" ) )
    {
        CLogger::ErrorPrintf ( "%s", *strResult );
        CLogger::ErrorPrintf ( "%s\n", *m_pDatabaseManager->GetLastErrorMessage () );
        CLogger::ErrorPrintf ( "Errors were encountered loading '%s' database\n", *ExtractFilename ( PathConform ( "internal.db" ) ) );
        CLogger::ErrorPrintf ( "Maybe now is the perfect time to panic.\n" );
        CLogger::ErrorPrintf ( "See - http://wiki.multitheftauto.com/wiki/fixdb\n" );
        CLogger::ErrorPrintf ( "************************\n" );
        return true; // Allow server to continue
    }

    // Check can update file
    m_pDatabaseManager->Execf ( m_hDbConnection, "DROP TABLE IF EXISTS write_test" );
    m_pDatabaseManager->Execf ( m_hDbConnection, "CREATE TABLE IF NOT EXISTS write_test (id INTEGER PRIMARY KEY, value INTEGER)" );
    m_pDatabaseManager->Execf ( m_hDbConnection, "INSERT OR IGNORE INTO write_test (id, value) VALUES(1,2)" ) ;
    bOk = m_pDatabaseManager->QueryWithResultf ( m_hDbConnection, NULL, "UPDATE write_test SET value=3 WHERE id=1" );
    if ( !bOk )
    {
        CLogger::ErrorPrintf ( "%s\n", *m_pDatabaseManager->GetLastErrorMessage () );
        CLogger::ErrorPrintf ( "Errors were encountered updating '%s' database\n", *ExtractFilename ( PathConform ( "internal.db" ) ) );
        CLogger::ErrorPrintf ( "Database might be locked by another process, or damaged.\n" );
        CLogger::ErrorPrintf ( "See - http://wiki.multitheftauto.com/wiki/fixdb\n" );
        CLogger::ErrorPrintf ( "************************\n" );
        return false;
    }
    m_pDatabaseManager->Execf ( m_hDbConnection, "DROP TABLE write_test" );
    return true;
}

*/
//========================================================================================================
int gf_busy_handler(void *p_params, int p_tryes)
{
	//CFlylinkDBManager *l_db = (CFlylinkDBManager *)p_params;
	Sleep(1000);
	LogManager::getInstance()->message("SQLite database is locked. try: " + Util::toString(p_tryes), true);
	if (p_tryes && p_tryes % 5 == 0)
	{
		const string l_message = STRING(DATA_BASE_LOCKED_STRING);
		MessageBox(NULL, Text::toT(l_message).c_str(), _T(APPNAME) _T(" ") T_VERSIONSTRING, MB_OK | MB_ICONERROR | MB_TOPMOST);
	}
	return 1;
}
//========================================================================================================
static void gf_trace_callback(void* p_udp, const char* p_sql)
{
	if (BOOLSETTING(LOG_SQLITE_TRACE))
	{
		StringMap params;
		params["sql"] = p_sql;
		LOG_FORCE_FILE(TRACE_SQLITE, params); // Всегда в файл
	}
}
//========================================================================================================
//static void profile_callback( void* p_udp, const char* p_sql, sqlite3_uint64 p_time)
//{
//	const string l_log = "profile_callback - " + string(p_sql) + " time = "+ Util::toString(p_time);
//	LogManager::getInstance()->message(l_log,true);
//}
//========================================================================================================
void CFlylinkDBManager::pragma_executor(const char* p_pragma)
{
	static const char* l_db_name[] = {"main", "media_db", "dht_db", "stat_db", "location_db", "user_db"};
	for (int i = 0; i < _countof(l_db_name); ++i)
	{
		string l_sql = "pragma ";
		l_sql += l_db_name[i];
		l_sql += '.';
		l_sql += p_pragma;
		l_sql += ';';
		m_flySQLiteDB.executenonquery(l_sql);
		LogManager::getInstance()->message("  [SQLite]" + l_sql, true);
	}
}
//========================================================================================================
bool CFlylinkDBManager::safeAlter(const char* p_sql)
{
	try
	{
		m_flySQLiteDB.executenonquery(p_sql);
		return true;
	}
	catch (const database_error& e)
	{
		if (e.getError().find("duplicate column name:") == string::npos) // Логируем только неизвестные ошибки
		{
			LogManager::getInstance()->message("safeAlter: " + e.getError(), true);
		}
	}
	return false;
}
//========================================================================================================
void CFlylinkDBManager::errorDB(const string& p_txt)
{
	string l_message = p_txt + "\r\n" + STRING(DATA_BASE_ERROR_STRING);
	const string l_error = "CFlylinkDBManager::errorDB. p_txt = " + p_txt;
	const char* l_rnrn = "\r\n";
	l_message += l_rnrn;
	l_message += l_rnrn;
	l_message += "Database locations:\r\n";
	const char* l_db_fil_names[] = { "FlylinkDC_log.sqlite",
	                                 "FlylinkDC_mediainfo.sqlite",
	                                 "FlylinkDC_dht.sqlite",
	                                 "FlylinkDC_stat.sqlite",
	                                 "FlylinkDC_locations.sqlite",
	                                 "FlylinkDC_user.sqlite"
	                               };
	for (int i = 0; i < _countof(l_db_fil_names); ++i)
	{
		l_message += "  * ";
		l_message += Util::getConfigPath();
		l_message += l_db_fil_names[i];
		l_message += l_rnrn;
	}
	if (p_txt.find("database disk image is malformed") != string::npos ||
	        p_txt.find("unable to open database:")         != string::npos)   // TODO - пополнять эти ошибки из автоапдейта?
	{
		l_message += l_rnrn;
		l_message += l_rnrn;
		l_message += "Try backup and delete all database files!";
	}
	Util::setRegistryValueString(FLYLINKDC_REGISTRY_SQLITE_ERROR , Text::toT(l_error));
	LogManager::getInstance()->message(p_txt, true); // Всегда логируем в файл (т.к. база может быть битой)
	MessageBox(NULL, Text::toT(l_message).c_str(), _T(APPNAME) _T(" ") T_VERSIONSTRING, MB_OK | MB_ICONERROR | MB_TOPMOST);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	bool l_is_send = CFlyServerAdapter::CFlyServerJSON::pushError(l_error);
	if (!l_is_send)
	{
#endif //FLYLINKDC_USE_MEDIAINFO_SERVER     
		// TODO - скинуть ошибку в файл и не грузить crash-server логическими ошибками
		// https://www.crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=51924
		throw database_error(l_error.c_str());
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	}
#endif //FLYLINKDC_USE_MEDIAINFO_SERVER 
}
//========================================================================================================
CFlylinkDBManager::CFlylinkDBManager()
{
#ifdef _DEBUG
	m_is_load_global_ratio = false;
#endif
	m_count_json_stat = 1; // Первый раз думаем, что в таблице что-то есть
	m_count_fly_location_ip_record = -1;
	m_last_path_id = -1;
	m_convert_ftype_stop_key = 0;
	m_queue_id = 0;
	m_DIC.resize(e_DIC_LAST - 1);
	try
	{
		// http://www.sql.ru/forum/1034900/executenonquery-ne-podkluchaet-dopolnitelnyy-fayly-tablic-bd-esli-v-puti-k-nim-est
		TCHAR l_dir_buffer[MAX_PATH];
		l_dir_buffer[0] = 0;
		DWORD dwRet = GetCurrentDirectory(MAX_PATH, l_dir_buffer);
		if (!dwRet)
		{
			errorDB("SQLite - CFlylinkDBManager: error GetCurrentDirectory " + Util::translateError(GetLastError()));
		}
		else
		{
			if (SetCurrentDirectory(Text::toT(Util::getConfigPath()).c_str()))
			{
				m_flySQLiteDB.open("FlylinkDC.sqlite");
				sqlite3_busy_handler(m_flySQLiteDB.get_db(), gf_busy_handler, this);
				// m_flySQLiteDB.setbusytimeout(1000);
				// TODO - sqlite3_busy_handler
				// Пример реализации обработчика -
				// https://github.com/iso9660/linux-sdk/blob/d819f98a72776fced31131b1bc22a4bcb4c492bb/SDKLinux/LFC/Data/sqlite3db.cpp
				// https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=17660
				if (g_EnableSQLtrace)
				{
					sqlite3_trace(m_flySQLiteDB.get_db(), gf_trace_callback, NULL);
					// sqlite3_profile(m_flySQLiteDB.get_db(), profile_callback, NULL);
				}
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_log.sqlite' as log_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_mediainfo.sqlite' as media_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_dht.sqlite' as dht_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_stat.sqlite' as stat_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_locations.sqlite' as location_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_user.sqlite' as user_db");
				
#ifdef FLYLINKDC_USE_LEVELDB
				// Тут обязательно полный путь. иначе при смене рабочего каталога levelDB не сомжет открыть базу.
				const string l_full_path_level_db = Util::getConfigPath() + "tth-history.leveldb";
				m_flyLevelDB.open_level_db(l_full_path_level_db);
#endif // FLYLINKDC_USE_LEVELDB
				SetCurrentDirectory(l_dir_buffer);
			}
			else
			{
				errorDB("SQLite - CFlylinkDBManager: error SetCurrentDirectory l_db_path = " + Util::getConfigPath()
				        + " Error: " +  Util::translateError(GetLastError()));
			}
		}
		
#ifdef IRAINMAN_SQLITE_USE_EXCLUSIVE_LOCK_MODE
		if (BOOLSETTING(SQLITE_USE_EXCLUSIVE_LOCK_MODE))
		{
			// Производительность от этого не возрастает. пусть включет только ежик
			pragma_executor("locking_mode=EXCLUSIVE");
		}
#endif
		pragma_executor("page_size=4096");
		if (g_DisableSQLJournal || BOOLSETTING(SQLITE_USE_JOURNAL_MEMORY))
		{
			pragma_executor("journal_mode=MEMORY");
		}
		else
		{
			if (g_UseWALJournal)
			{
				pragma_executor("journal_mode=WAL");
			}
			else
			{
				pragma_executor("journal_mode=PERSIST");
			}
			pragma_executor("journal_size_limit=16384"); // http://src.chromium.org/viewvc/chrome/trunk/src/sql/connection.cc
			pragma_executor("secure_delete=OFF"); // http://www.sqlite.org/pragma.html#pragma_secure_delete
			if (g_UseSynchronousOff)
			{
				pragma_executor("synchronous=OFF");
			}
			else
			{
				pragma_executor("synchronous=FULL");
			}
#ifndef FLYLINKDC_USE_GATHER_IDENTITY_STAT
			pragma_executor("count_changes=OFF");  //
#endif
		}
		pragma_executor("temp_store=MEMORY");
		
		// Log_DB
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS log_db.fly_log(id integer PRIMARY KEY AUTOINCREMENT,"
		    "sdate text not null, type integer not null,body text, hub text, nick text,\n"
		    "ip text, file text, source text, target text,fsize int64,fchunk int64,extra text,userCID text);");
		// DHT_DB
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS dht_db.fly_dht_node(\n"
		    "cid char(24) not null,\n"
		    "expires int64,\n"
		    "type integer,\n"
		    "verified integer,\n"
		    "ip int64,\n"
		    "port integer,\n"
		    "key char(24),\n"
		    "key_ip int64, primary key(cid));");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS dht_db.fly_dht_file(\n"
		    "tth char(24) not null,\n"
		    "cid char(24) not null,\n"
		    "ip int64,\n"
		    "port integer,\n"
		    "size int64 not null,\n"
		    "expires int64,\n"
		    "partial integer, primary key(tth,cid))");
		// l_flySQLiteDB_dht.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS dht_db.iu_fly_dht_file ON fly_dht_file(tth,cid);");
		// MEDIA_DB
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS media_db.fly_server_cache(tth char(24) PRIMARY KEY NOT NULL, fly_audio text,fly_audio_br text,fly_video text,fly_xy text);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS media_db.fly_media(\n" // id integer primary key autoincrement, TODO - id в этой табличке нам не нужна...
		    "tth_id integer not null,\n"
		    "stream_type integer not null,\n"
		    "channel integer not null,\n"
		    "param text not null,\n"
		    "value text not null);");
		// TODO - потестить как sqlite выбирает план
		// l_flySQLiteDB_Mediainfo.executenonquery("CREATE INDEX IF NOT EXISTS media_db.i_fly_media_tth_id ON fly_media(tth_id);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS media_db.iu_fly_media_param ON fly_media(tth_id,stream_type,channel,param);");
		
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_revision(rev integer NOT NULL);");
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_tth(\n"
		                              "tth char(24) PRIMARY KEY NOT NULL);");
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_hash(\n"
		                              "id integer PRIMARY KEY AUTOINCREMENT NOT NULL, tth char(24) NOT NULL UNIQUE);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_file(dic_path integer not null,\n"
		    "name text not null,\n"
		    "size int64 not null,\n"
		    "stamp int64 not null,\n"
		    "tth_id int64 not null,\n"
		    "hit int64 default 0,\n"
		    "stamp_share int64 default 0,\n"
		    "bitrate integer,\n"
		    "ftype integer default -1,\n"
		    "media_x integer,\n"
		    "media_y integer,\n"
		    "media_video text,\n"
		    "media_audio text\n"
		    ");");
		    
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_file_name ON fly_file(dic_path,name);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS i_fly_file_tth_id ON fly_file(tth_id);");
		//[-] TODO потестить на большой шаре
		// m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS i_fly_file_dic_path ON fly_file(dic_path);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_hash_block(tth_id integer PRIMARY KEY NOT NULL,\n"
		    "tiger_tree blob,file_size int64 not null,block_size int64);");
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_path(\n"
		                              "id integer PRIMARY KEY AUTOINCREMENT NOT NULL, name text NOT NULL UNIQUE);");
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_dic(\n"
		                              "id integer PRIMARY KEY AUTOINCREMENT NOT NULL,dic integer NOT NULL, name text NOT NULL);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_dic_name ON fly_dic(name,dic);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_ratio(id integer PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
		    "dic_ip integer not null,dic_nick integer not null, dic_hub integer not null,\n"
		    "upload int64 default 0,download int64 default 0);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_ratio ON fly_ratio(dic_nick,dic_hub,dic_ip);");
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		m_flySQLiteDB.executenonquery("CREATE VIEW IF NOT EXISTS v_fly_ratio AS\n"
		                              "SELECT fly_ratio.id id, fly_ratio.upload upload,\n"
		                              "fly_ratio.download download,\n"
		                              "userip.name userip,\n"
		                              "nick.name nick,\n"
		                              "hub.name hub\n"
		                              "FROM fly_ratio\n"
		                              "INNER JOIN fly_dic userip ON fly_ratio.dic_ip = userip.id\n"
		                              "INNER JOIN fly_dic nick ON fly_ratio.dic_nick = nick.id\n"
		                              "INNER JOIN fly_dic hub ON fly_ratio.dic_hub = hub.id");
		                              
		m_flySQLiteDB.executenonquery("CREATE VIEW IF NOT EXISTS v_fly_ratio_all AS\n"
		                              "SELECT fly_ratio.id id, fly_ratio.upload upload,\n"
		                              "fly_ratio.download download,\n"
		                              "nick.name nick,\n"
		                              "hub.name hub,\n"
		                              "fly_ratio.dic_ip dic_ip,\n"
		                              "fly_ratio.dic_nick dic_nick,\n"
		                              "fly_ratio.dic_hub dic_hub\n"
		                              "FROM fly_ratio\n"
		                              "INNER JOIN fly_dic nick ON fly_ratio.dic_nick = nick.id\n"
		                              "INNER JOIN fly_dic hub ON fly_ratio.dic_hub = hub.id");
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
		m_flySQLiteDB.executenonquery("CREATE VIEW IF NOT EXISTS v_fly_dup_file AS\n"
		                              "SELECT tth_id,count(*) cnt_dup,\n"
		                              "max((select name from fly_path where id = dic_path)) path_1,\n"
		                              "min((select name from fly_path where id = dic_path)) path_2,\n"
		                              "max(name) name_max,\n"
		                              "min(name) name_min\n"
		                              "FROM fly_file group by tth_id having count(*) > 1");
		                              
		const int l_rev = m_flySQLiteDB.executeint("select max(rev) from fly_revision");
		if (l_rev < 322)
		{
			safeAlter("ALTER TABLE fly_file add column hit int64 default 0");
			safeAlter("ALTER TABLE fly_file add column stamp_share int64 default 0");
			safeAlter("ALTER TABLE fly_file add column bitrate integer");
			sqlite3_transaction l_trans(m_flySQLiteDB);
			// рехеш всего музона
			m_flySQLiteDB.executenonquery("delete from fly_file where name like '%.mp3' and (bitrate=0 or bitrate is null)");
			l_trans.commit();
		}
		if (l_rev < 358)
		{
			safeAlter("ALTER TABLE fly_file add column ftype integer default -1");
			sqlite3_transaction l_trans(m_flySQLiteDB);
			m_flySQLiteDB.executenonquery("update fly_file set ftype=1 where ftype=-1 and "
			                              "(name like '%.mp3' or name like '%.ogg' or name like '%.wav' or name like '%.flac' or name like '%.wma')");
			m_flySQLiteDB.executenonquery("update fly_file set ftype=2 where ftype=-1 and "
			                              "(name like '%.rar' or name like '%.zip' or name like '%.7z' or name like '%.gz')");
			m_flySQLiteDB.executenonquery("update fly_file set ftype=3 where ftype=-1 and "
			                              "(name like '%.doc' or name like '%.pdf' or name like '%.chm' or name like '%.txt' or name like '%.rtf')");
			m_flySQLiteDB.executenonquery("update fly_file set ftype=4 where ftype=-1 and "
			                              "(name like '%.exe' or name like '%.com' or name like '%.msi')");
			m_flySQLiteDB.executenonquery("update fly_file set ftype=5 where ftype=-1 and "
			                              "(name like '%.jpg' or name like '%.gif' or name like '%.png')");
			m_flySQLiteDB.executenonquery("update fly_file set ftype=6 where ftype=-1 and "
			                              "(name like '%.avi' or name like '%.mpg' or name like '%.mov' or name like '%.divx')");
			l_trans.commit();
		}
		if (l_rev < 341)
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			m_flySQLiteDB.executenonquery("delete from fly_file where tth_id=0");
			l_trans.commit();
		}
		if (l_rev < 365)
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			m_flySQLiteDB.executenonquery("update fly_file set ftype=6 where name like '%.mp4' or name like '%.fly'");
			l_trans.commit();
		}
		if (l_rev <= 377)
		{
			m_flySQLiteDB.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_queue(id integer PRIMARY KEY NOT NULL,\n"
			    "Target text not null,\n"  // убрал UNIQUE для исключения перестройки индекса (уникальность поддерживается мапой)
			    "Size int64 not null,\n"
			    "Priority integer not null,\n"
			    //"FreeBlocks text,\n"  // [-] merge,  new DB format
			    //"VerifiedParts text,\n"   // [-] merge,  new DB format
			    "Sections text,\n"
			    "Added int64 not null,\n"
			    "TTH char(24) not null,\n"
			    "TempTarget text,\n"
			    //"Downloaded int64,\n" // [-] merge, obsolete
			    "AutoPriority integer not null,\n"
			    "MaxSegments integer,\n"
			    "CID char(24),\n"
			    "Nick text,\n"
			    "CountSubSource integer,\n"
			    "HubHint text\n"
			    ");");
			m_flySQLiteDB.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_queue_source(id integer PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
			    "fly_queue_id integer not null,\n"
			    "CID char(24) not null,\n"
			    "Nick text,\n"
			    "HubHint text\n"
			    ");");
			m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS\n"
			                              "i_fly_queue_source_id ON fly_queue_source(fly_queue_id);");
			                              
		}
		if (l_rev > 376 && l_rev <= 377)
		{
			safeAlter("ALTER TABLE fly_queue add column CID char(24)");
			safeAlter("ALTER TABLE fly_queue add column Nick text");
			safeAlter("ALTER TABLE fly_queue add column CountSubSource integer");
		}
		if (l_rev <= 379)
		{
			m_flySQLiteDB.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_ignore(nick text PRIMARY KEY NOT NULL);");
		}
		if (l_rev <= 381)
		{
			m_flySQLiteDB.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_registry(segment integer not null, key text not null,val_str text, val_number int64,tick_count int not null);");
			m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS\n"
			                              "iu_fly_registry_key ON fly_registry(segment,key);");
			// TODO m_flySQLiteDB.executenonquery(
			// TODO             "CREATE TABLE IF NOT EXISTS fly_recent(Name text PRIMARY KEY NOT NULL,Description text, Users int,Shared int64,Server text);");
		}
		m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_geoip");
		if (is_table_exists("fly_country_ip"))
		{
			// Перезагрузим локации в отдельный файл базы
			// Для этого скинем метку времени для файлов данных ч тобы при следующем запуске выполнилась перезагрузка
			// и удалим таблицы в основной базе данных
			set_registry_variable_int64(e_TimeStampGeoIP, 0);
			set_registry_variable_int64(e_TimeStampCustomLocation, 0);
			m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_country_ip");
			m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_location_ip");
			m_flySQLiteDB.executenonquery("DROP TABLE IF EXISTS fly_location_ip_lost");
		}
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_country_ip(start_ip integer not null,stop_ip integer not null,country text,flag_index integer);");
		safeAlter("ALTER TABLE location_db.fly_country_ip add column country text");
		
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_country_ip ON fly_country_ip(start_ip,stop_ip);");
		/*
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS i_fly_country_ip ON fly_country_ip(start_ip,stop_ip);");
		ALTER TABLE location_db.fly_country_ip ADD COLUMN idx INTEGER;
		UPDATE fly_country_ip SET idx = (stop_ip - (stop_ip % 65536));
		CREATE INDEX IF NOT EXISTS location_db.i_idx_fly_country_ip ON fly_country_ip(idx);
		*/
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_location_ip(start_ip integer not null,stop_ip integer not null,location text,flag_index integer);");
		    
		safeAlter("ALTER TABLE location_db.fly_location_ip add column location text");
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_location_ip_lost(ip text PRIMARY KEY not null,is_send_fly_server integer);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS "
		                              "location_db.i_fly_location_ip ON fly_location_ip(start_ip,stop_ip);"); // Индекс делаем не уникальный
		                              
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_identity(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,hub text not null,key text not null, value text not null,\n"
		    "count_get integer, count_set integer,last_time_get text not null, last_time_set text not null);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS\n"
		                              "stat_db.iu_fly_identity ON fly_identity(hub,key,value);");
		                              
#endif // FLYLINKDC_USE_GATHER_IDENTITY_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_statistic(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,stat_value_json text not null,stat_time int64, flush_time int64);");
		// Таблицы - мертвые
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_last_ip(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
		                              "dic_nick integer not null, dic_hub integer not null,dic_ip integer not null);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_last_ip ON fly_last_ip(dic_nick,dic_hub);");
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_last_ip_nick_hub("
		                              "nick text not null, dic_hub integer not null,ip text);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_last_ip_nick_hub ON fly_last_ip_nick_hub(nick,dic_hub);");
		// Они не используются в версиях r502 но для отката назад нужны
		
#ifdef FLYLINKDC_USE_COLLECT_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_dc_command_log(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL\n"
		    ",hub text not null,command text not null,server text,port text, sender_nick text, counter int64, last_time text not null);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS stat_db.iu_fly_dc_command_log ON fly_dc_command_log(hub,command);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_event(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL\n"
		    ",type text not null, event_key text not null, event_value text, ip text, port text, hub text, tth char(39), event_time text);");
#endif
		if (l_rev <= 500)
		{
			safeAlter("ALTER TABLE fly_queue add column Sections text");
			
			// [!] brain-ripper
			// have to delete follow columns, but SQLite doesn't support DROP command.
			// TODO some workaround.
			/*
			safeAlter("ALTER TABLE fly_queue drop column FreeBlocks");
			safeAlter("ALTER TABLE fly_queue drop column VerifiedParts");
			safeAlter("ALTER TABLE fly_queue drop column Downloaded");
			*/
		}
		if (l_rev <= 502)
			safeAlter("ALTER TABLE fly_hash_block add column block_size int64");
		if (l_rev < 403 || l_rev == 500)
		{
			safeAlter("ALTER TABLE fly_file add column media_x integer");
			safeAlter("ALTER TABLE fly_file add column media_y integer");
			safeAlter("ALTER TABLE fly_file add column media_video text");
			if (safeAlter("ALTER TABLE fly_file add column media_audio text"))
			{
				// получилось добавить колонку - значит стартанули первый раз - удалим все записи для перехеша с помощью либы
				sqlite3_transaction l_trans(m_flySQLiteDB);
				const string l_where = "delete from fly_file where " + g_fly_server_config.DBDelete();
				m_flySQLiteDB.executenonquery(l_where);
				l_trans.commit();
			}
		}
		
		if (l_rev < VERSION_NUM)
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			m_flySQLiteDB.executenonquery("insert into fly_revision(rev) values(" A_VERSION_NUM_STR ");");
			l_trans.commit();
		}
		//safeAlter("ALTER TABLE fly_queue add column HubHint text"); // TODO - колонки не используются. удалить?
		//safeAlter("ALTER TABLE fly_queue_source add column HubHint text"); // TODO - колонки не используются. удалить?
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS user_db.user_info(\n"
		                              "nick text not null, dic_hub integer not null, last_ip integer, message_count integer);");
		m_flySQLiteDB.executenonquery("DROP INDEX IF EXISTS user_db.iu_user_info;"); //старый индекс был (nick,dic_hub)
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS user_db.iu_user_info_hub_nick ON user_info(dic_hub,nick);");
		
		/*
		{
		    // Конвертим ip в бинарный формат
		    auto_ptr<sqlite3_command> l_src_sql = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
		                                                                    "select nick,dic_hub,ip from fly_last_ip_nick_hub"));
		    try
		    {
		        sqlite3_reader l_q = l_src_sql->executereader();
		        sqlite3_transaction l_trans(m_flySQLiteDB);
		        auto_ptr<sqlite3_command> l_trg_sql = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
		                                                                        "insert or replace into user_db.user_info (nick,dic_hub,last_ip) values(?,?,?)"));
		        while (l_q.read())
		        {
		            boost::system::error_code ec;
		            const auto l_ip = boost::asio::ip::address_v4::from_string(l_q.getstring(2), ec);
		            dcassert(!ec);
		            if (!ec)
		            {
		                l_trg_sql.get()->bind(1, l_q.getstring(0), SQLITE_TRANSIENT);
		                l_trg_sql.get()->bind(2, l_q.getint64(1));
		                l_trg_sql.get()->bind(3, sqlite_int64(l_ip.to_ulong()));
		                l_trg_sql.get()->executenonquery();
		            }
		        }
		        l_trans.commit();
		    }
		    catch (const database_error& e)
		    {
		        // Гасим ошибки БД при конвертации
		        LogManager::getInstance()->message("[SQLite] Error convert user_db.user_info = " + e.getError());
		    }
		}
		*/
		load_all_hub_into_cacheL();
		//safeAlter("ALTER TABLE fly_last_ip_nick_hub add column message_count integer");
		
		/*      {
		            sqlite3_transaction l_trans(m_flySQLiteDB);
		            sqlite3_command(m_flySQLiteDB,"delete from fly_queue where size=-1"));
		            m_del_fly_queue->executenonquery();
		            l_trans.commit();
		        }
		*/
		
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - CFlylinkDBManager: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::load_all_hub_into_cacheL()
{
	auto_ptr<sqlite3_command> l_load_all_dic(new sqlite3_command(m_flySQLiteDB,
	                                                             "select id,name from fly_dic where dic=1"));
	sqlite3_reader l_q = l_load_all_dic.get()->executereader();
	while (l_q.read())
	{
		const auto& l_res = m_DIC[e_DIC_HUB - 1].insert(std::make_pair(l_q.getstring(1), l_q.getint(0)));
		dcassert(l_res.second == true);
	}
}
//========================================================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
bool CFlylinkDBManager::find_fly_server_cache(const TTHValue& p_tth, CFlyServerCache& p_value)
{
	Lock l(m_cs);
	try
	{
		if (!m_select_fly_server_cache.get())
			m_select_fly_server_cache = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                          "select fly_audio,fly_audio_br,fly_video,fly_xy from media_db.fly_server_cache where tth=?"));
		m_select_fly_server_cache.get()->bind(1, p_tth.data, 24, SQLITE_STATIC);
		sqlite3_reader l_q = m_select_fly_server_cache.get()->executereader();
		if (l_q.read())
		{
			p_value.m_audio = l_q.getstring(0);
			p_value.m_audio_br = l_q.getstring(1);
			p_value.m_video = l_q.getstring(2);
			p_value.m_xy = l_q.getstring(3);
			return true;
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - find_fly_server_cache: " + e.getError());
	}
	return false;
}
//========================================================================================================
void CFlylinkDBManager::save_fly_server_cache(const TTHValue& p_tth, const CFlyServerCache& p_value)
{
	Lock l(m_cs);
	try
	{
		if (!m_insert_fly_server_cache.get())
			m_insert_fly_server_cache = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                          "insert or replace into media_db.fly_server_cache (tth,fly_audio,fly_audio_br,fly_video,fly_xy) values(?,?,?,?,?)"));
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_insert_fly_server_cache.get()->bind(1, p_tth.data, 24, SQLITE_STATIC);
		m_insert_fly_server_cache.get()->bind(2, p_value.m_audio, SQLITE_STATIC);
		m_insert_fly_server_cache.get()->bind(3, p_value.m_audio_br, SQLITE_STATIC);
		m_insert_fly_server_cache.get()->bind(4, p_value.m_video, SQLITE_STATIC);
		m_insert_fly_server_cache.get()->bind(5, p_value.m_xy, SQLITE_STATIC);
		m_insert_fly_server_cache.get()->executenonquery();
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_fly_server_cache: " + e.getError());
	}
}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
//========================================================================================================
void CFlylinkDBManager::identity_initL(const string& p_hub, const string& p_key, const string& p_value)
{
	try
	{
		if (!m_insert_identity_stat.get())
			m_insert_identity_stat = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                       "insert into stat_db.fly_identity (hub,key,value,count_get,count_set,last_time_get,last_time_set)\n"
			                                                                       "values(?,?,?,0,0,strftime('%d.%m.%Y %H:%M:%S','now','localtime'),strftime('%d.%m.%Y %H:%M:%S','now','localtime'))"));
		m_insert_identity_stat.get()->bind(1, p_hub, SQLITE_STATIC);
		m_insert_identity_stat.get()->bind(2, p_key, SQLITE_STATIC);
		m_insert_identity_stat.get()->bind(3, p_value, SQLITE_STATIC);
		m_insert_identity_stat.get()->executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - identity_initL: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::identity_set(string p_key, string p_value, const string& p_hub /*= "-" */)
{
	dcassert(!p_key.empty());
	if (p_value.empty())
		p_value = "null";
	if (p_key.size() > 2)
		p_key = p_key.substr(0, 2);
	if (p_key.empty())
		p_key = "null";
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_update_identity_stat_set.get())
			m_update_identity_stat_set = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                           "update stat_db.fly_identity set count_set = count_set+1, last_time_set = strftime('%d.%m.%Y %H:%M:%S','now','localtime')\n"
			                                                                           "where hub = ? and key=? and value =?"));
		m_update_identity_stat_set.get()->bind(1, p_hub, SQLITE_STATIC);
		m_update_identity_stat_set.get()->bind(2, p_key, SQLITE_STATIC);
		m_update_identity_stat_set.get()->bind(3, p_value, SQLITE_STATIC);
		m_update_identity_stat_set.get()->executenonquery();
		if (m_flySQLiteDB.sqlite3_changes() == 0)
		{
			identity_initL(p_hub, p_key, p_value);
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - identity_set: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::identity_get(string p_key, string p_value, const string& p_hub /*= "-" */)
{
	dcassert(!p_key.empty());
	if (p_value.empty())
		p_value = "null";
	if (p_key.size() > 2)
		p_key = p_key.substr(0, 2);
	if (p_key.empty())
		p_key = "null";
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_update_identity_stat_get.get())
			m_update_identity_stat_get = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                           "update stat_db.fly_identity set count_get = count_get+1, last_time_get = strftime('%d.%m.%Y %H:%M:%S','now','localtime')\n"
			                                                                           "where hub = ? and key=? and value =?"));
		m_update_identity_stat_get.get()->bind(1, p_hub, SQLITE_STATIC);
		m_update_identity_stat_get.get()->bind(2, p_key, SQLITE_STATIC);
		m_update_identity_stat_get.get()->bind(3, p_value, SQLITE_STATIC);
		m_update_identity_stat_get.get()->executenonquery();
		if (m_flySQLiteDB.sqlite3_changes() == 0)
		{
			identity_initL(p_hub, p_key, p_value);
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - identity_get: " + e.getError());
	}
}
#endif // FLYLINKDC_USE_GATHER_IDENTITY_STAT
//========================================================================================================
#ifdef FLYLINKDC_USE_COLLECT_STAT
void CFlylinkDBManager::push_event_statistic(const std::string& p_event_type, const std::string& p_event_key,
                                             const string& p_event_value,
                                             const string& p_ip,
                                             const string& p_port,
                                             const string& p_hub,
                                             const string& p_tth
                                            )
{
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_insert_event_stat.get())
			m_insert_event_stat = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                    "insert into stat_db.fly_event(type,event_key,event_value,ip,port,hub,tth,event_time) values(?,?,?,?,?,?,?,strftime('%d.%m.%Y %H:%M:%S','now','localtime'))"));
		m_insert_event_stat.get()->bind(1, p_event_type, SQLITE_STATIC);
		m_insert_event_stat.get()->bind(2, p_event_key, SQLITE_STATIC);
		m_insert_event_stat.get()->bind(3, p_event_value, SQLITE_STATIC);
		m_insert_event_stat.get()->bind(4, p_ip, SQLITE_STATIC);
		m_insert_event_stat.get()->bind(5, p_port, SQLITE_STATIC);
		m_insert_event_stat.get()->bind(6, p_hub, SQLITE_STATIC);
		m_insert_event_stat.get()->bind(7, p_tth, SQLITE_STATIC);
		m_insert_event_stat.get()->executenonquery();
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - push_event_statistic: " + e.getError());
	}
}


void CFlylinkDBManager::push_dc_command_statistic(const std::string& p_hub, const std::string& p_command,
                                                  const string& p_server, const string& p_port, const string& p_sender_nick)
{
	dcassert(!p_hub.empty() && !p_command.empty());
	if (!p_hub.empty() && !p_command.empty())
	{
		Lock l(m_cs);
		try
		{
			__int64 l_counter = 0;
			{
				if (!m_select_statistic_dc_command.get())
					m_select_statistic_dc_command = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                              "select counter from stat_db.fly_dc_command_log where hub = ? and command = ?"));
				m_select_statistic_dc_command.get()->bind(1, p_hub, SQLITE_STATIC);
				m_select_statistic_dc_command.get()->bind(2, p_command, SQLITE_STATIC);
				sqlite3_reader l_q = m_select_statistic_dc_command.get()->executereader();
				while (l_q.read())
				{
					l_counter = l_q.getint64(0);
				}
			}
			sqlite3_transaction l_trans(m_flySQLiteDB);
			if (!m_insert_statistic_dc_command.get())
				m_insert_statistic_dc_command = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                              "insert or replace into stat_db.fly_dc_command_log(hub, command, server, port, sender_nick, counter, last_time) values(?,?,?,?,?,?,strftime('%d.%m.%Y %H:%M:%S','now','localtime'))"));
			m_insert_statistic_dc_command.get()->bind(1, p_hub, SQLITE_STATIC);
			m_insert_statistic_dc_command.get()->bind(2, p_command, SQLITE_STATIC);
			m_insert_statistic_dc_command.get()->bind(3, p_server, SQLITE_STATIC);
			m_insert_statistic_dc_command.get()->bind(4, p_port, SQLITE_STATIC);
			m_insert_statistic_dc_command.get()->bind(5, p_sender_nick, SQLITE_STATIC);
			m_insert_statistic_dc_command.get()->bind(6, l_counter + 1);
			m_insert_statistic_dc_command.get()->executenonquery();
			l_trans.commit();
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - push_dc_command_statistic: " + e.getError());
		}
	}
	else
	{
		// Log
	}
}
#endif // FLYLINKDC_USE_COLLECT_STAT
//========================================================================================================
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
void CFlylinkDBManager::push_json_statistic(const std::string& p_value)
{
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_insert_statistic_json.get())
			m_insert_statistic_json = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                        "insert into stat_db.fly_statistic (stat_value_json,stat_time) values(?,strftime('%s','now','localtime'))"));
		// TODO stat_time пока не используется, но пусть будет :)
		m_insert_statistic_json.get()->bind(1, p_value, SQLITE_STATIC);
		m_insert_statistic_json.get()->executenonquery();
		l_trans.commit();
		++m_count_json_stat;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - push_json_statistic: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::flush_lost_json_statistic()
{
	if (BOOLSETTING(USE_STATICTICS_SEND)) // Отсылка статистики разрешена?
	{
		Lock l(m_cs);
		try
		{
			std::vector<__int64> l_json_array_id;
			if (m_count_json_stat)
			{
				if (!m_select_statistic_json.get())
					m_select_statistic_json = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                        "select id,stat_value_json from stat_db.fly_statistic limit 30"));
				// where flush_time is null (пока не используем это поле и не храним локальную статистику - не придумал как подчищать данные)
				sqlite3_reader l_q = m_select_statistic_json.get()->executereader();
				while (l_q.read())
				{
					bool l_is_send = false;
					const auto l_id = l_q.getint64(0);
					const std::string l_post_query = l_q.getstring(1);
					/*const std::string l_result =*/
					CFlyServerAdapter::CFlyServerJSON::postQuery(true, true, false, false, false, "fly-stat", l_post_query, l_is_send); // [!] PVS V808 'l_result' object of 'basic_string' type was created but was not utilized. cflylinkdbmanager.cpp 545
					if (l_is_send)
					{
						l_json_array_id.push_back(l_id);
					}
				}
			}
			if (l_json_array_id.size() < 30)
				m_count_json_stat = 0; // Все записи будут удалены ниже. скидываем счетчик в ноль чтобы не делать больше селекта
			else
				m_count_json_stat = 1; // Записи еще возможно есть - через минуту пробуем снова.
			if (!l_json_array_id.empty())
			{
				// Отметим факт пересылки статистики на сервер
				sqlite3_transaction l_trans(m_flySQLiteDB);
				if (!m_delete_statistic_json.get())
					m_delete_statistic_json = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                        "delete from stat_db.fly_statistic where id=?")); // Пока не сохраняем историю в локальной базе.
				// "update stat_db.fly_statistic set flush_time = strftime('%s','now','localtime') where id=?"));
				for (auto i = l_json_array_id.cbegin(); i != l_json_array_id.cend(); ++i)
				{
					m_delete_statistic_json.get()->bind(1, *i);
					m_delete_statistic_json.get()->executenonquery();
				}
				l_trans.commit();
			}
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - flush_lost_json_statistic: " + e.getError());
		}
	}
}
#endif // FLYLINKDC_USE_GATHER_STATISTICS
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
//========================================================================================================
void CFlylinkDBManager::get_lost_location(std::vector<std::string>& p_lost_ip_array)
{
	if (is_fly_location_ip_valid())
	{
		Lock l(m_cs);
		try
		{
			{
				if (!m_select_location_lost.get())
					m_select_location_lost = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                       "select ip from location_db.fly_location_ip_lost where is_send_fly_server is null limit 100")); // За один раз больше 100 не шлем
				sqlite3_reader l_q = m_select_location_lost.get()->executereader();
				while (l_q.read())
				{
					p_lost_ip_array.push_back(l_q.getstring(0));
				}
			}
			// Отметим факт пересылки массива IP на сервер
			sqlite3_transaction l_trans(m_flySQLiteDB);
			if (!m_update_location_lost.get())
				m_update_location_lost = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                       "update location_db.fly_location_ip_lost set is_send_fly_server=1 where ip=?"));
			for (auto i = p_lost_ip_array.cbegin(); i != p_lost_ip_array.cend(); ++i)
			{
				m_update_location_lost.get()->bind(1, *i, SQLITE_STATIC);
				m_update_location_lost.get()->executenonquery();
			}
			l_trans.commit();
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - get_lost_location: " + e.getError());
		}
	}
}
//========================================================================================================
void CFlylinkDBManager::save_lost_location(const string& p_ip)
{
	dcassert(!p_ip.empty());
	Lock l(m_cs);
	if (m_count_fly_location_ip_record < 0)
	{
		if (!m_select_count_location.get())
			m_select_count_location = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                        "select count(*) from location_db.fly_location_ip"));
		sqlite3_reader l_count_q = m_select_count_location.get()->executereader();
		if (l_count_q.read())
		{
			m_count_fly_location_ip_record = l_count_q.getint(0);
		}
	}
	if (is_fly_location_ip_valid())
	{
		if (m_lost_location_cache.find(p_ip) == m_lost_location_cache.end())
		{
			try
			{
				sqlite3_transaction l_trans(m_flySQLiteDB);
				if (!m_insert_location_lost.get())
					m_insert_location_lost = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                       "insert into location_db.fly_location_ip_lost (ip) values(?)"));
				try
				{
					m_insert_location_lost.get()->bind(1, p_ip, SQLITE_STATIC);
					m_insert_location_lost.get()->executenonquery();
					l_trans.commit();
				}
				catch (const database_error&)
				{
					// Гасим попытку вставить дубликатную запись - это нормально.
				}
				m_lost_location_cache.insert(p_ip);
			}
			catch (const database_error& e)
			{
				errorDB("SQLite - save_lost_location: " + e.getError());
			}
		}
	}
}
#endif
//========================================================================================================
void CFlylinkDBManager::get_location(uint32_t p_ip, int32_t& p_index)
{
	dcassert(p_ip);
	FastLock l(m_cache_location_cs);
	const uint16_t l_flag_index = find_cache_locationL(p_ip, p_index);
	if (l_flag_index == 0)
	{
		get_location_sql(p_ip, p_index);
	}
}
//========================================================================================================
void CFlylinkDBManager::get_location_sql(uint32_t p_ip, int32_t& p_index)
{
	dcassert(p_ip);
	Lock l(m_cs);
	try
	{
		if (!m_select_location.get())
			m_select_location = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                  "select location,start_ip,stop_ip,flag_index "
			                                                                  "from location_db.fly_location_ip where start_ip <= ? and stop_ip > ? limit 1"));
		m_select_location.get()->bind(1, __int64(p_ip));
		m_select_location.get()->bind(2, __int64(p_ip));
		sqlite3_reader l_q = m_select_location.get()->executereader();
		CFlyLocationDesc l_location;
		uint16_t l_first_index = 0;
		p_index = 0;
		while (l_q.read())
		{
			l_location.m_description = Text::toT(l_q.getstring(0));
			l_location.m_start_ip   = l_q.getint(1);
			l_location.m_stop_ip    = l_q.getint(2);
			l_location.m_flag_index = l_q.getint(3);
			if (l_location.m_flag_index)
				l_first_index  = l_location.m_flag_index;
			m_location_cache.push_back(l_location);
			p_index = m_location_cache.size();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_location_sql: " + e.getError());
	}
	return;
}
//========================================================================================================
void CFlylinkDBManager::save_location(const CFlyLocationIPArray& p_geo_ip)
{
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_delete_location.get())
			m_delete_location = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                  "delete from location_db.fly_location_ip"));
		m_delete_location.get()->executenonquery();
		m_count_fly_location_ip_record = 0;
		if (!m_insert_location.get())
			m_insert_location = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                  "insert into location_db.fly_location_ip (start_ip,stop_ip,location,flag_index) values(?,?,?,?)"));
		for (auto i = p_geo_ip.begin(); i != p_geo_ip.end(); ++i)
		{
			const auto &l_insert_location_get = m_insert_location.get(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_insert_location.get()' expression repeatedly. cflylinkdbmanager.cpp 563
			dcassert(i->m_start_ip  && !i->m_location.empty());
			l_insert_location_get->bind(1, __int64(i->m_start_ip));
			l_insert_location_get->bind(2, __int64(i->m_stop_ip));
			l_insert_location_get->bind(3, i->m_location, SQLITE_STATIC);
			l_insert_location_get->bind(4, i->m_flag_index);
			l_insert_location_get->executenonquery();
			++m_count_fly_location_ip_record;
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_location: " + e.getError());
	}
}
//========================================================================================================
uint16_t CFlylinkDBManager::find_cache_locationL(uint32_t p_ip, int32_t& p_index)
{
	dcassert(p_ip);
	p_index = 0;
	for (auto i =  m_location_cache.begin(); i !=  m_location_cache.end(); ++i, ++p_index)
	{
		if (p_ip >= i->m_start_ip && p_ip < i->m_stop_ip)
		{
			++p_index;
			return i->m_flag_index;
		}
	}
	return 0;
}
//========================================================================================================
bool CFlylinkDBManager::find_cache_countryL(uint32_t p_ip, uint16_t& p_index)
{
	dcassert(p_ip);
	p_index = 0;
	dcassert(m_country_cache.size() <= 0xFFFF);
	for (auto i =  m_country_cache.begin(); i !=  m_country_cache.end(); ++i)
	{
		++p_index;
		if (p_ip >= i->m_start_ip && p_ip < i->m_stop_ip)
		{
			return true;
		}
	}
	p_index = 0;
	return false;
}
//========================================================================================================
void CFlylinkDBManager::get_country(uint32_t p_ip, uint16_t& p_index)
{
	dcassert(p_ip);
	FastLock l(m_cache_location_cs);
	const bool l_is_find = find_cache_countryL(p_ip, p_index);
	if (l_is_find == false)
	{
		CFlyLocationDesc l_location;
		if (get_country_sqlite(p_ip, l_location))
		{
			m_country_cache.push_back(l_location);
			p_index = m_country_cache.size();
		}
	}
}
//========================================================================================================
uint8_t CFlylinkDBManager::get_country_sqlite(uint32_t p_ip, CFlyLocationDesc& p_location)
{
	dcassert(p_ip);
	Lock l(m_cs);
	try
	{
		if (!m_select_geoip.get())
			m_select_geoip = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                               "select country, flag_index,start_ip,stop_ip "
			                                                               "from location_db.fly_country_ip where start_ip <= ? and stop_ip > ?"));
		m_select_geoip.get()->bind(1, __int64(p_ip));
		m_select_geoip.get()->bind(2, __int64(p_ip));
		sqlite3_reader l_q = m_select_geoip.get()->executereader();
		if (l_q.read())
		{
			p_location.m_description = Text::toT(l_q.getstring(0));
			p_location.m_flag_index = l_q.getint(1);
			p_location.m_start_ip = l_q.getint(2);
			p_location.m_stop_ip = l_q.getint(3);
		}
		else
		{
			p_location.m_flag_index = 0;
		}
		dcassert(l_q.read() == false); // Второго диапазона в GeoIPCountryWhois.csv быть не должно!
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_country_sqlite: " + e.getError());
	}
	return uint8_t(p_location.m_flag_index);
}
//========================================================================================================
void CFlylinkDBManager::save_geoip(const CFlyLocationIPArray& p_geo_ip)
{
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_delete_geoip.get())
			m_delete_geoip = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                               "delete from location_db.fly_country_ip"));
		m_delete_geoip.get()->executenonquery();
		if (!m_insert_geoip.get())
			m_insert_geoip = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                               "insert into location_db.fly_country_ip (start_ip,stop_ip,country,flag_index) values(?,?,?,?)"));
		for (auto i = p_geo_ip.begin(); i != p_geo_ip.end(); ++i)
		{
			const auto &l_insert_geoip_get = m_insert_geoip.get(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_insert_geoip.get()' expression repeatedly. cflylinkdbmanager.cpp 620
			dcassert(i->m_start_ip  && !i->m_location.empty());
			l_insert_geoip_get->bind(1, __int64(i->m_start_ip));
			l_insert_geoip_get->bind(2, __int64(i->m_stop_ip));
			l_insert_geoip_get->bind(3, i->m_location, SQLITE_STATIC);
			l_insert_geoip_get->bind(4, i->m_flag_index);
			l_insert_geoip_get->executenonquery();
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_geoip: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::set_registry_variable_int64(eTypeSegment p_TypeSegment, __int64 p_value)
{
	CFlyRegistryMap l_store_values;
	l_store_values[Util::toString(p_TypeSegment)] = p_value;
	save_registry(l_store_values, p_TypeSegment, false);
}
//========================================================================================================
__int64 CFlylinkDBManager::get_registry_variable_int64(eTypeSegment p_TypeSegment)
{
	CFlyRegistryMap l_values;
	load_registry(l_values, p_TypeSegment);
	return l_values[Util::toString(p_TypeSegment)].m_val_int64;
}
//========================================================================================================
void CFlylinkDBManager::load_registry(TStringList& p_values, int p_Segment)
{
	p_values.clear();
	CFlyRegistryMap l_values;
	Lock l(m_cs);
	load_registry(l_values, p_Segment);
	p_values.reserve(l_values.size());
	for (auto k = l_values.cbegin(); k != l_values.cend(); ++k)
	{
		p_values.push_back(Text::toT(k->first));
	}
}
//========================================================================================================
void CFlylinkDBManager::save_registry(const TStringList& p_values, int p_Segment)
{
	Lock l(m_cs);
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
//========================================================================================================
void CFlylinkDBManager::load_registry(CFlyRegistryMap& p_values, int p_Segment)
{
	Lock l(m_cs);
	try
	{
		if (!m_get_registry.get())
			m_get_registry = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                               "select key,val_str,val_number from fly_registry where segment=?"));
		m_get_registry->bind(1, p_Segment);
		sqlite3_reader l_q = m_get_registry.get()->executereader();
		while (l_q.read())
		{
			const auto& l_res = p_values.insert(CFlyRegistryMap::value_type(
			                                        l_q.getstring(0),
			                                        CFlyRegistryValue(l_q.getstring(1), l_q.getint64(2))));
			dcassert(l_res.second);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_registry: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::save_registry(const CFlyRegistryMap& p_values, int p_Segment, bool p_is_cleanup_old_value)
{
	const int l_tick = GET_TICK();
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_insert_registry.get())
			m_insert_registry = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                  "insert or replace into fly_registry (segment,key,val_str,val_number,tick_count) values(?,?,?,?,?)"));
		for (auto k = p_values.cbegin(); k != p_values.cend(); ++k)
		{
			const auto &l_insert_registry_get = m_insert_registry.get(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_insert_registry.get()' expression repeatedly. cflylinkdbmanager.cpp 702
			l_insert_registry_get->bind(1, p_Segment);
			l_insert_registry_get->bind(2, k->first, SQLITE_TRANSIENT);
			l_insert_registry_get->bind(3, k->second.m_val_str, SQLITE_TRANSIENT);
			l_insert_registry_get->bind(4, k->second.m_val_int64);
			l_insert_registry_get->bind(5, l_tick);
			l_insert_registry_get->executenonquery();
		}
		if (p_is_cleanup_old_value)
		{
			if (!m_delete_registry.get())
				m_delete_registry = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                  "delete from fly_registry where segment=? and tick_count<>?"));
			m_delete_registry->bind(1, p_Segment);
			m_delete_registry->bind(2, l_tick);
			m_delete_registry.get()->executenonquery();
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_registry: " + e.getError());
	}
}
//========================================================================================================
#ifdef STRONG_USE_DHT
int CFlylinkDBManager::find_dht_files(const TTHValue& p_tth, dht::SourceList& p_source_list)
{
	Lock l(m_cs);
	int l_count = 0;
	try
	{
		if (!m_find_dht_files.get())
			m_find_dht_files = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                 "select tth,cid,ip,port,size,partial from dht_db.fly_dht_file where tth=?"));
		m_find_dht_files.get()->bind(1, p_tth.data, 24, SQLITE_STATIC);
		sqlite3_reader l_q = m_find_dht_files.get()->executereader();
		for (; l_q.read(); ++l_count)
		{
			vector<uint8_t> l_cid_buf;
			dht::Source l_source;
			l_q.getblob(1, l_cid_buf);
			dcassert(l_cid_buf.size() == 24);
			if (l_cid_buf.size() == 24)
				l_source.setCID(CID(&l_cid_buf[0]));
			l_source.setIp(l_q.getstring(2));
			dcassert(l_q.getint(3) > 0 && l_q.getint(3) < 0xFFFF);
			l_source.setUdpPort(static_cast<uint16_t>(l_q.getint(3)));
			l_source.setSize(l_q.getint64(4));
			l_source.setPartial(l_q.getint(5) ? 1 : 0);
//				l_source.setExpires(l_q.getint64(6));
			p_source_list.push_back(l_source);
		}
		//LogManager::getInstance()->message("[dht] find_dht_files TTH = " + p_tth.toBase32(),true);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - find_dht_files: " + e.getError());
	}
	return l_count;
}
//========================================================================================================
void CFlylinkDBManager::check_expiration_dht_files(uint64_t p_Tick)
{
	Lock l(m_cs);
	try
	{
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			if (!m_check_expiration_dht_files.get())
				m_check_expiration_dht_files = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                             "delete from dht_db.fly_dht_file where expires < strftime('%s','now','localtime')-86400")); // 24 hours
			m_check_expiration_dht_files.get()->executenonquery();
			l_trans.commit();
		}
		//LogManager::getInstance()->message("[dht] check_expiration_dht_files p_Tick = " + Util::toString(p_Tick),true);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - check_expiration_dht_files: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::save_dht_files(const dht::TTHArray& p_dht_files)
{
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_save_dht_files.get())
			m_save_dht_files = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                 "insert or replace into dht_db.fly_dht_file(tth,cid,ip,port,size,partial,expires)\n"
			                                                                 "values(?,?,?,?,?,?,strftime('%s','now','localtime'))"));
		const auto &l_save_dht_files_get = m_save_dht_files.get(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_save_dht_files.get()' expression repeatedly. cflylinkdbmanager.cpp 797
		for (auto i = p_dht_files.cbegin(); i != p_dht_files.cend(); ++i)
		{
			l_save_dht_files_get->bind(1, i->getTTH().data, 24, SQLITE_STATIC);
			l_save_dht_files_get->bind(2, i->getCID().data(), 24, SQLITE_STATIC);
			l_save_dht_files_get->bind(3, i->getIp(), SQLITE_STATIC);
			dcassert(i->getUdpPort() > 0 && i->getUdpPort() < 0xFFFF);
			l_save_dht_files_get->bind(4, i->getUdpPort());
			l_save_dht_files_get->bind(5, static_cast<__int64>(i->getSize()));
			l_save_dht_files_get->bind(6, i->getPartial());
			l_save_dht_files_get->executenonquery();
// Нельзя делать логирования внутри транзакции
//			LogManager::getInstance()->message("[dht] save_dht_file TTH = " + i->getTTH().toBase32() + " size = " + Util::toString(i->getSize()),true);
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_dht_files: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::save_dht_nodes(const std::vector<dht::BootstrapNode>& p_dht_nodes) // [!] IRainman opt: replace dqueue to vector.
{
	Lock l(m_cs);
	try
	{
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			if (!m_delete_dht_nodes.get())
				m_delete_dht_nodes = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB, "delete from dht_db.fly_dht_node"));
			m_delete_dht_nodes.get()->executenonquery();
			if (!m_save_dht_nodes.get())
				m_save_dht_nodes = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                 "insert into dht_db.fly_dht_node(cid,ip,port,key,key_ip,expires)\n"
				                                                                 "values(?,?,?,?,?,strftime('%s','now','localtime'))")); // ,type,verified
			const auto &l_sav_dht_nodes_get = m_save_dht_nodes.get(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_save_dht_nodes.get()' expression repeatedly. cflylinkdbmanager.cpp 830
			for (auto k = p_dht_nodes.cbegin(); k != p_dht_nodes.cend(); ++k)
			{
				l_sav_dht_nodes_get->bind(1, k->m_cid.data(), 24, SQLITE_STATIC);
				l_sav_dht_nodes_get->bind(2, k->m_ip, SQLITE_STATIC);
				l_sav_dht_nodes_get->bind(3, k->m_udpPort);
				if (!k->m_udpKey.m_ip.empty())
					l_sav_dht_nodes_get->bind(4, k->m_udpKey.m_key.data(), 24, SQLITE_STATIC);
				else
					l_sav_dht_nodes_get->bind(4, k->m_udpKey.m_key.data(), 0, SQLITE_STATIC);
				l_sav_dht_nodes_get->bind(5, k->m_udpKey.m_ip, SQLITE_STATIC);
				l_sav_dht_nodes_get->executenonquery();
			}
			l_trans.commit();
		}
		//LogManager::getInstance()->message("[dht] save_dht_nodes",true);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_dht_nodes: " + e.getError());
	}
}
//========================================================================================================
bool CFlylinkDBManager::load_dht_nodes(std::vector<dht::BootstrapNode>& p_dht_nodes) // [!] IRainman opt: replace dqueue to vector and return bool.
{
	p_dht_nodes.reserve(500);
	Lock l(m_cs);
	try
	{
		if (!m_load_dht_nodes.get())
			m_load_dht_nodes = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                 "select cid,ip,port,key,key_ip from dht_db.fly_dht_node\n" // load nodes; when file is older than 7 days, bootstrap from database later
			                                                                 "where expires >= strftime('%s','now','localtime') - (86400 * 7)"
			                                                                )); //type,verified
			                                                                
		sqlite3_reader l_q = m_load_dht_nodes.get()->executereader();
		while (l_q.read())
		{
			dht::BootstrapNode l_nodes;
			vector<uint8_t> l_cid_buf;
			l_q.getblob(0, l_cid_buf);
			if (l_cid_buf.size() == 24)
				l_nodes.m_cid = CID(&l_cid_buf[0]);
			l_nodes.m_ip = l_q.getstring(1);
			l_nodes.m_udpPort = l_q.getint(2);
			l_nodes.m_udpKey.m_ip = l_q.getstring(4);
			if (!l_nodes.m_udpKey.m_ip.empty())
			{
				vector<uint8_t> l_key_buf;
				l_q.getblob(3, l_key_buf);
				if (l_key_buf.size() == 24)
					l_nodes.m_udpKey.m_key = CID(&l_key_buf[0]);
			}
			p_dht_nodes.push_back(l_nodes);
		}
		//if (!p_dht_nodes.empty())
		//  LogManager::getInstance()->message("[dht] load_dht_nodes p_dht_nodes.size() = " + Util::toString(p_dht_nodes.size()),true);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_dht_nodes: " + e.getError());
	}
	return !p_dht_nodes.empty();
}
#endif // STRONG_USE_DHT
//========================================================================================================
void CFlylinkDBManager::load_ignore(StringSet& p_ignores)
{
	Lock l(m_cs);
	try
	{
		if (!m_get_ignores.get())
			m_get_ignores = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                              "select nick from fly_ignore"));
		sqlite3_reader l_q = m_get_ignores.get()->executereader();
		while (l_q.read())
		{
			const string l_nick = l_q.getstring(0);
			p_ignores.insert(l_nick);
			LogManager::getInstance()->message(STRING(IGNORE_USER_BY_NAME) + ": " + l_nick, true);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_ignore: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::save_ignore(const StringSet& p_ignores)
{
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_delete_ignores.get())
			m_delete_ignores = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                 "delete from fly_ignore"));
		m_delete_ignores.get()->executenonquery();
		if (!m_insert_ignores.get())
			m_insert_ignores = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                 "insert or replace into fly_ignore (nick) values(?)"));
		for (auto k = p_ignores.cbegin(); k != p_ignores.cend(); ++k)
		{
			m_insert_ignores.get()->bind(1, (*k), SQLITE_TRANSIENT);
			m_insert_ignores.get()->executenonquery();
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_ignore: " + e.getError());
	}
}
//========================================================================================================
// [+] brain-ripper
static string getSectionStringL(const QueueItemPtr& p_QueueItem)
{
	string l_strSections;
	for (auto i = p_QueueItem->getDoneL().cbegin(); i != p_QueueItem->getDoneL().cend(); ++i)
	{
		l_strSections += Util::toString(i->getStart());
		l_strSections += ' ';
		l_strSections += Util::toString(i->getSize());
		l_strSections += ' ';
	}
	if (!l_strSections.empty()) // TODO ?
	{
		l_strSections.resize(l_strSections.size() - 1);
	}
	
	return l_strSections;
}
//========================================================================================================
// [+] brain-ripper
//TODO - унести как метод в QueueItem
static void setSectionString(const QueueItemPtr& p_QueueItem, const string& strSectionString)
{
	if (!p_QueueItem || strSectionString.empty())
		return;
		
	const StringTokenizer<string> SectionTokens(strSectionString, ' ');
	const StringList &Sections = SectionTokens.getTokens();
	
	if (!Sections.empty())
	{
		// must be multiply of 2
		dcassert((Sections.size() & 1) == 0);
		
		if ((Sections.size() & 1) == 0)
		{
			WLock l(*QueueItem::g_cs); // [+] IRainman fix.
			for (auto i = Sections.cbegin(); i < Sections.cend(); i += 2)
			{
				int64_t start = Util::toInt64(i->c_str());
				int64_t size = Util::toInt64((i + 1)->c_str());
				
				p_QueueItem->addSegmentL(Segment(start, size));
			}
		}
	}
}
//========================================================================================================
size_t CFlylinkDBManager::load_queue()
{
	Lock l(m_cs);
	g_count_queue_source = 0;
	try
	{
		QueueManager* qm = QueueManager::getInstance();
		static const string l_sql = "select id,"
		                            "Target,"
		                            "Size,"
		                            "Priority,"
		                            //"FreeBlocks,"    // [-] merge,  new DB format
		                            //"VerifiedParts," // [-] merge,  new DB format
		                            "Sections,"    // [+] merge,  new DB format
		                            "Added,"
		                            "TTH,"
		                            "TempTarget,"
		                            //"Downloaded,"    //[-] merge, obsolete
		                            "AutoPriority,"
		                            "MaxSegments,"
		                            "CID," //12
		                            "Nick,"
		                            "CountSubSource"
		                            //",HubHint "
		                            " from fly_queue where size > 0"; // todo убрать в будущих версиях
		if (!m_get_fly_queue.get())
			m_get_fly_queue = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB, l_sql));
		sqlite3_reader l_q = m_get_fly_queue.get()->executereader();
		vector<__int64> l_bad_targets;
		while (l_q.read())
		{
			const int64_t l_size = l_q.getint64(2);
			
			//[-] brain-ripper
			// int l_flags = QueueItem::FLAG_RESUME;
			int l_flags = 0;
			
			if (l_size == 0)
				continue;
			const string l_tgt = l_q.getstring(1);
			string l_target;
			try
			{
				//[-] brain-ripper
				//l_target = QueueManager::checkTarget(l_tgt, l_size, l_flags);
				l_target = QueueManager::checkTarget(l_tgt, l_size);
				
				if (l_target.empty())
					continue;
			}
			catch (const Exception& e)
			{
				l_bad_targets.push_back(l_q.getint64(0));
				LogManager::getInstance()->message("SQLite - load_queue[1]: " + l_tgt + e.getError(), true);
				continue;
			}
			//const string l_freeBlocks = l_q.getstring(4);
			//const string l_verifiedBlocks = l_q.getstring(5);
			const QueueItem::Priority l_p = QueueItem::Priority(l_q.getint(3));
			time_t l_added = static_cast<time_t>(l_q.getint64(5));
			if (l_added == 0)
				l_added = GET_TIME();
			TTHValue l_tthRoot;
			if (!l_q.getblob(6, l_tthRoot.data, 24))
				continue;
			//if(tthRoot.empty()) ?
			//  continue;
			const string l_tempTarget = l_q.getstring(7);
			const uint8_t l_maxSegments = uint8_t(l_q.getint(9));
			
			const __int64 l_ID = l_q.getint64(0);
			m_queue_id = std::max(m_queue_id, l_ID);
			QueueItemPtr qi = qm->fileQueue.find(l_target); //TODO после отказа от конвертации XML варианта очереди можно удалить
			if (!qi)
			{
				// [-] brain-ripper
				//if ((l_maxSegments > 1) || !l_freeBlocks.empty())
				//  l_flags |= QueueItem::FLAG_MULTI_SOURCE;
				
				// [-] brain-ripper
				//qi = qm->fileQueue.add(l_target, l_size, Flags::MaskType(l_flags), l_p, l_tempTarget, l_downloaded,
				//                       l_added, l_freeBlocks, l_verifiedBlocks, l_tthRoot);
				qi = qm->fileQueue.add(l_target, l_size, Flags::MaskType(l_flags), l_p, l_tempTarget,
				                       l_added, l_tthRoot);
				                       
				qi->setAutoPriority(l_q.getint(8) != 0);
				qi->setMaxSegments(max((uint8_t)1, l_maxSegments));
				qi->setFlyQueueID(l_ID);
				qi->setDirty(false); // [+]PPA загрузили из очереди - сделаем ее чистую
				qm->fire(QueueManagerListener::Added(), qi);
			}
			
			// [+] brain-ripper
			setSectionString(qi, l_q.getstring(4));
			
			// [!] IRainman fix: do not lose sources with nick is empty: https://code.google.com/p/flylinkdc/issues/detail?id=849
			CID l_cid;
			if (l_q.getblob(10, l_cid.get_data_for_write(), 24))
			{
				const string l_nick = l_q.getstring(11);
				//const string l_hub_hint = l_q.getstring(13);
				addSource(qi, l_cid, l_nick/*, l_hub_hint*/);
			}
			// [~] IRainman fix
			const int l_CountSubSource = l_q.getint(12);
			qi->setFlyCountSourceInSQL(l_CountSubSource); // https://code.google.com/p/flylinkdc/issues/detail?id=933
			if (l_CountSubSource > 0 || l_cid.isZero()) // [!] IRainman fix: do not lose sources with nick is empty: https://code.google.com/p/flylinkdc/issues/detail?id=849
			{
				if (!m_get_fly_queue_source.get())
					m_get_fly_queue_source = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                       "select cid,nick from fly_queue_source where fly_queue_id=?"));
				// "select cid,nick,HubHint from fly_queue_source where fly_queue_id=?"));
				// TODO - возможно появление дублей https://code.google.com/p/flylinkdc/issues/detail?id=931
				// добавлять distinct - не стал т.к. нагрузит базу лишней сортировкой в нормальных условиях
				// TODO - Добавить контроль дубликатности CID
				m_get_fly_queue_source.get()->bind(1, l_ID);
				sqlite3_reader l_q_source = m_get_fly_queue_source.get()->executereader();
				dcdrun(int l_testCountSubSource = 0);
				while (l_q_source.read())
				{
					g_count_queue_source++;
					CID l_cid;
					if (l_q_source.getblob(0, l_cid.get_data_for_write(), 24))
					{
						const string l_nick = l_q.getstring(1);
						//const string l_hub_hint = l_q.getstring(2);
						addSource(qi, l_cid, l_nick/*, l_hub_hint*/);
						dcdrun(++l_testCountSubSource);
					}
				}
				dcassert(l_CountSubSource == l_testCountSubSource);
			}
			else
			{
				g_count_queue_source++;
			}
			qi->setDirty(false);
		}
		for (auto i = l_bad_targets.cbegin(); i != l_bad_targets.cend(); ++i)
		{
			remove_queue_item(*i);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_queue: " + e.getError());
	}
	return g_count_queue_source;
}
//========================================================================================================
void CFlylinkDBManager::addSource(const QueueItemPtr& p_QueueItem, const CID& p_cid, const string& p_nick/*, const string& p_hub_hint*/)
{
#ifndef IRAINMAN_SAVE_ALL_VALID_SOURCE
	dcassert(!p_nick.empty());
	//dcassert(!p_hub_hint.empty());
#endif
	dcassert(!p_cid.isZero());
	if (!p_cid.isZero())
	{
		UserPtr l_user = ClientManager::getUser(p_cid, true); // Создаем юзера в любом случае - http://www.flylinkdc.ru/2012/09/flylinkdc-r502-beta59.html
		
		// [+] brain-ripper, merge:
		// correctly show names of offline users in queue frame
		l_user->initLastNick(p_nick);
		
		bool wantConnection = false;
		try
		{
			WLock l(*QueueItem::g_cs); // [+] IRainman fix.
			wantConnection = QueueManager::getInstance()->addSourceL(p_QueueItem, l_user, 0, true) && l_user->isOnline(); // Добавить флаг ускоренной загрузки первый раз.
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message("CFlylinkDBManager::addSource, Error = " + e.getError(), true);
		}
		if (wantConnection)
		{
			ConnectionManager::getInstance()->getDownloadConnection(l_user);
		}
		/* [-]
		else
		{
		    dcdebug("p_cid not found p_cid = %s", p_cid.toBase32().c_str());
		}
		[-] */
	}
}
//========================================================================================================
void CFlylinkDBManager::delete_queue_sources(const __int64 p_id)
{
	dcassert(p_id);
	if (!m_del_fly_queue_source.get())
		m_del_fly_queue_source = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
		                                                                       "delete from fly_queue_source where fly_queue_id=?"));
	// TODO - сделать апдейт поля-флажка + фильтр + фоновая зачистка
	m_del_fly_queue_source.get()->bind(1, p_id);
	m_del_fly_queue_source.get()->executenonquery();
}
//========================================================================================================
void CFlylinkDBManager::remove_queue_item(const __int64 p_id)
{
	dcassert(p_id);
	// if (p_id) [-] IRainman fix: FlyQueueID is not set for filelists. Please don't problem maskerate.
	{
		Lock l(m_cs);
		try
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			delete_queue_sources(p_id);
			if (!m_del_fly_queue.get())
				m_del_fly_queue = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                "delete from fly_queue where id=?"));
			m_del_fly_queue->bind(1, p_id);
			m_del_fly_queue->executenonquery();
			l_trans.commit();
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - remove_queue_item: " + e.getError());
		}
	}
}
//========================================================================================================
bool CFlylinkDBManager::merge_queue_item(QueueItemPtr& p_QueueItem)
{
	try
	{
		Lock l(m_cs); // TODO dead lock https://code.google.com/p/flylinkdc/issues/detail?id=1028
		sqlite3_transaction l_trans(m_flySQLiteDB);
		__int64 l_id = p_QueueItem->getFlyQueueID();
		if (!l_id)
			l_id = ++m_queue_id;
		else
		{
			if (p_QueueItem->getFlyCountSourceInSQL()) // Источники писали в базу - есть что удалять? https://code.google.com/p/flylinkdc/issues/detail?id=933
			{
#ifdef _DEBUG
//				LogManager::getInstance()->message("delete_queue_sources(l_id) l_id = " + Util::toString(l_id),true);
#endif
				delete_queue_sources(l_id);
			}
		}
		if (!m_insert_fly_queue.get())
			m_insert_fly_queue = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                   "insert or replace into fly_queue ("
			                                                                   "Target,"
			                                                                   "Size,"
			                                                                   "Priority,"
			                                                                   //"FreeBlocks,"  // [-] merge,  new DB format
			                                                                   //"VerifiedParts,"   // [-] merge,  new DB format
			                                                                   "Sections,"  // [+] merge,  new DB format
			                                                                   "Added,"
			                                                                   "TTH,"
			                                                                   "TempTarget,"
			                                                                   //"Downloaded,"  // [-] merge, obsolete
			                                                                   "AutoPriority,"
			                                                                   "MaxSegments,"
			                                                                   "id,"
			                                                                   "CID,"
			                                                                   "Nick,"
			                                                                   "CountSubSource"
			                                                                   // Хаб не пишем ",HubHint"
			                                                                   ") values(?,?,?,?,?,?,?,?,?,?,?,?,?)"));
		sqlite3_command* l_sql = m_insert_fly_queue.get();
		l_sql->bind(1, p_QueueItem->getTarget(), SQLITE_TRANSIENT);
		l_sql->bind(2, p_QueueItem->getSize());
		l_sql->bind(3, int(p_QueueItem->getPriority()));
		
		// [-] brain-ripper
		// New engine provides "Segment" instead of "Chunk".
		//const bool l_is_multi = p_QueueItem->isSet(QueueItem::FLAG_MULTI_SOURCE) && p_QueueItem->chunkInfo;
		//l_sql->bind(4, l_is_multi ? p_QueueItem->chunkInfo->getFreeChunksString() : "");
		//l_sql->bind(5, l_is_multi ? p_QueueItem->chunkInfo->getVerifiedBlocksString() : "");
		
		// [+] merge
		l_sql->bind(4, getSectionStringL(p_QueueItem), SQLITE_TRANSIENT);
		
		l_sql->bind(5, p_QueueItem->getAdded());
		l_sql->bind(6, p_QueueItem->getTTH().data, 24, SQLITE_TRANSIENT);
		l_sql->bind(7, p_QueueItem->getDownloadedBytes() > 0 ? p_QueueItem->getTempTarget() : Util::emptyString, SQLITE_TRANSIENT);
		l_sql->bind(8, p_QueueItem->getAutoPriority());
		
		// [-] brain-ripper
		//l_sql->bind(9, p_QueueItem->isSet(QueueItem::FLAG_MULTI_SOURCE) ? p_QueueItem->getMaxSegments() : 0);
		l_sql->bind(9, p_QueueItem->getMaxSegments());
		
		l_sql->bind(10, l_id);
		{
			// RLock l(*QueueItem::g_cs);; // [+] IRainman fix. // TODO после исправления dead lock вернуть данную блокировку https://code.google.com/p/flylinkdc/issues/detail?id=1028
			const auto &l_sources = p_QueueItem->getSourcesL(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'p_QueueItem->getSourcesL()' expression repeatedly. cflylinkdbmanager.cpp 1245
			const auto l_count_normal_source = l_sources.size();
#ifdef IRAINMAN_SAVE_BAD_SOURCE
			const auto &l_badSources = p_QueueItem->getBadSourcesL(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'p_QueueItem->getBadSourcesL()' expression repeatedly. cflylinkdbmanager.cpp 1246
			const auto l_count_bad_source = l_badSources.size();
#endif
			const int l_count_total_source = l_count_normal_source
#ifdef IRAINMAN_SAVE_BAD_SOURCE
			                                 + l_count_bad_source
#endif
			                                 ;
			l_sql->bind(13, l_count_total_source > 0 ? l_count_total_source - 1 : 0); // [!] IRainman fix: not save negative value.
			if (l_count_normal_source)
			{
				const auto& s = l_sources.cbegin();
				dcassert(!s->first->getCID().isZero());
#ifndef IRAINMAN_SAVE_ALL_VALID_SOURCE
				dcassert(!s->first->getLastNick().empty());
				//dcassert(!s->getUser().hint.empty());
#endif
				l_sql->bind(11, s->first->getCID().data(), 24, SQLITE_TRANSIENT);
				l_sql->bind(12, s->first->getLastNick(), SQLITE_TRANSIENT);
				// [-] Хаб не пишем.
				// l_sql->bind(14, Util::emptyString, SQLITE_TRANSIENT); // s->getUser().hint
			}
#ifdef IRAINMAN_SAVE_BAD_SOURCE
			else if (l_count_bad_source)
			{
				const auto b = l_badSources.cbegin();
#ifndef IRAINMAN_SAVE_ALL_VALID_SOURCE
				dcassert(!b->getUser()->getCID().isZero());
				//dcassert(!b->getUser()->getLastNick().empty());
#endif
				l_sql->bind(11, b->getUser()->getCID().data(), 24, SQLITE_TRANSIENT);
				l_sql->bind(12, b->getUser()->getLastNick(), SQLITE_TRANSIENT);
				// TODO l_sql->bind(14, Util::emptyString, SQLITE_TRANSIENT); // s->getUser().hint
			}
#endif
			else
			{
				// LogManager::getInstance()->message("l_count_normal_source == 0!",true);
				dcassert(0);
				l_sql->bind(11);
				l_sql->bind(12); // ?
				//TODO l_sql->bind(14, Util::emptyString, SQLITE_TRANSIENT); // ? пустой хинт ?
			}
			l_sql->executenonquery();
#ifdef _DEBUG
//			LogManager::getInstance()->message("insert or replace into fly_queue! l_id = "  + Util::toString(l_id)
//			                                   + " l_count_total_source = " + Util::toString(l_count_total_source),true);
#endif
			int l_cont_insert_sub_source = 0;
			if (l_count_total_source > 1)
			{
				if (!m_insert_fly_queue_source.get())
					m_insert_fly_queue_source = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                          "insert into fly_queue_source(fly_queue_id,CID,Nick) values(?,?,?)"));
				// Урл хаба не используем...
				// "insert into fly_queue_source(fly_queue_id,CID,Nick,HubHint) values(?,?,?,?)"));
				sqlite3_command* l_sql_source = m_insert_fly_queue_source.get();
				auto j = l_sources.cbegin();
				if (l_count_normal_source > 0)
					++j;
				for (; j != l_sources.cend(); ++j)
				{
					const auto &l_user = j->first; // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'j->getUser().user' expression repeatedly. cflylinkdbmanager.cpp 1289
#ifndef IRAINMAN_SAVE_ALL_VALID_SOURCE
					if (j->second.isSet(QueueItem::Source::FLAG_PARTIAL)/* || j->getUser().hint == "DHT"*/)
						continue;
#endif
					const auto &l_cid = l_user->getCID();// [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'j->getUser().user->getCID()' expression repeatedly. cflylinkdbmanager.cpp 1284
					dcassert(!l_cid.isZero());
					if (l_cid.isZero())
					{
#ifdef _DEBUG
//						LogManager::getInstance()->message("[CFlylinkDBManager::merge_queue_item] l_cid.isZero() - skip! insert into fly_queue_source CID = "
//						                                   + l_cid.toBase32() + " nick = " + l_user->getLastNick(),true
//						                                  );
//
#endif
						continue;
					}
					l_sql_source->bind(1, l_id);
#ifndef IRAINMAN_SAVE_ALL_VALID_SOURCE
					dcassert(!l_user->getLastNick().empty());
					//dcassert(!j->getUser().hint.empty());
#endif
					l_sql_source->bind(2, l_cid.data(), 24, SQLITE_TRANSIENT);
					l_sql_source->bind(3, l_user->getLastNick(), SQLITE_TRANSIENT);
					// TODO l_sql_source->bind(4, Util::emptyString, SQLITE_TRANSIENT); // j->getUser().hint
					l_sql_source->executenonquery(); // TODO - копипаст
					l_cont_insert_sub_source++;
#ifdef _DEBUG
//					LogManager::getInstance()->message("[CFlylinkDBManager::merge_queue_item] insert into fly_queue_source CID = "
//					                                   + l_cid.toBase32() + " nick = " + l_user->getLastNick(),true
//					                                  );
//
#endif
				}
#ifdef IRAINMAN_SAVE_BAD_SOURCE
				// [+] IRainman fix: All the bad sources of potential time bad, they must be re-add, not delete. https://code.google.com/p/flylinkdc/issues/detail?id=849
				for (auto j = l_badSources.cbegin() + ((l_count_normal_source == 0 && l_count_bad_source > 0) ? 1 : 0); j != l_badSources.cend(); ++j)
				{
					const auto &l_user = j->getUser(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'j->getUser().user' expression repeatedly. cflylinkdbmanager.cpp 1289
					const auto &l_cid = l_user->getCID();// [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'j->getUser().user->getCID()' expression repeatedly. cflylinkdbmanager.cpp 1284
					l_sql_source->bind(1, l_id);
					dcassert(!l_cid.isZero());
#ifndef IRAINMAN_SAVE_ALL_VALID_SOURCE
					dcassert(!l_user->getLastNick().empty());
					//dcassert(!j->getUser().hint.empty());
#endif
					l_sql_source->bind(2, l_cid.data(), 24, SQLITE_TRANSIENT);
					l_sql_source->bind(3, l_user->getLastNick(), SQLITE_TRANSIENT);
					// TODO l_sql_source->bind(4, Util::emptyString, SQLITE_TRANSIENT); // j->getUser().hint
					l_sql_source->executenonquery(); // TODO - addsou
					l_cont_insert_sub_source++;
#ifdef _DEBUG
//					LogManager::getInstance()->message("[getBadSourcesL] insert into fly_queue_source CID = "
//					                                   + l_cid.toBase32() + " nick = " + l_user->getLastNick(),true
//					                                  );
#endif
				}
#endif // IRAINMAN_SAVE_BAD_SOURCE
			}
			// dcassert(l_count_total_source == l_cont_insert_sub_source + 1);
			p_QueueItem->setFlyCountSourceInSQL(l_cont_insert_sub_source); // Сохраним в памяти сколько записей добавили к дочерней таблице.
			p_QueueItem->setFlyQueueID(l_id);
		}
		l_trans.commit();
		return true;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_queue_item: " + e.getError());
	}
	return false;
}
//========================================================================================================
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
void CFlylinkDBManager::load_global_ratio()
{
	try
	{
		Lock l(m_cs);
		auto_ptr<sqlite3_command> l_select_global_ratio_load =
		    auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB, "select total(upload),total(download) from fly_ratio"));
		// http://www.sqlite.org/lang_aggfunc.html
		// Sum() will throw an "integer overflow" exception if all inputs are integers or NULL and an integer overflow occurs at any point during the computation.
		// Total() never throws an integer overflow.
		sqlite3_reader l_q = l_select_global_ratio_load.get()->executereader();
		if (l_q.read())
		{
			m_global_ratio.m_upload   = l_q.getdouble(0);
			m_global_ratio.m_download = l_q.getdouble(1);
		}
#ifdef _DEBUG
		m_is_load_global_ratio = true;
#endif
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_global_ratio: " + e.getError());
	}
}
//========================================================================================================
bool CFlylinkDBManager::load_last_ip_and_user_stat(uint32_t p_hub_id, const string& p_nick, uint32_t& p_message_count, boost::asio::ip::address_v4& p_last_ip)
{
	try
	{
		Lock l(m_cs);
		p_message_count = 0;
		auto l_find_cache_item = m_last_ip_cache.find(p_hub_id);
		if (l_find_cache_item == m_last_ip_cache.end()) // Хаб первый раз? (TODO - добавить задержку на кол-во запросов. если больше N - выполнить пакетную загрузку)
		{
			auto& l_cache_item = m_last_ip_cache[p_hub_id];
			if (!m_select_all_last_ip_and_message_count.get())
			{
				m_select_all_last_ip_and_message_count = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                   "select last_ip,message_count,nick"
				                                                                   //"(select flag_index ||'~'|| location from location_db.fly_location_ip where start_ip <= last_ip and stop_ip > last_ip limit 1) location_id,\n"
				                                                                   //"(select flag_index ||'~'|| country from location_db.fly_country_ip where start_ip <= last_ip and stop_ip > last_ip) country_id\n"
				                                                                   " from user_db.user_info where dic_hub=?"));
			}
			sqlite3_command* l_sql_command = m_select_all_last_ip_and_message_count.get();
			l_sql_command->bind(1, __int64(p_hub_id));
			sqlite3_reader l_q = l_sql_command->executereader();
			while (l_q.read())
			{
				CFlyLastIPCacheItem l_item;
				l_item.m_last_ip = boost::asio::ip::address_v4((unsigned long)l_q.getint64(0));
				l_item.m_message_count = l_q.getint64(1);
				l_cache_item.insert(std::make_pair(l_q.getstring(2), l_item));
			}
		}
		auto& l_hub_cache = m_last_ip_cache[p_hub_id];
		const auto& l_cache_nick_item = l_hub_cache.find(p_nick);
		if (l_cache_nick_item != l_hub_cache.end())
		{
			p_message_count = l_cache_nick_item->second.m_message_count;
			p_last_ip = l_cache_nick_item->second.m_last_ip;
			return true;
		}
#if 0
		// В кэше найти не удалось - ищем точечно
		if (!m_select_last_ip_and_message_count.get())
			m_select_last_ip_and_message_count = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                               "select last_ip, message_count from user_db.user_info where nick=? and dic_hub=?"));
		sqlite3_command* l_sql_command = m_select_last_ip_and_message_count.get();
		l_sql_command->bind(1, p_nick, SQLITE_STATIC);
		l_sql_command->bind(2, __int64(p_hub_id));
		sqlite3_reader l_q = l_sql_command->executereader();
		auto& l_item = l_hub_cache[p_nick];
		if (l_q.read())
		{
			p_last_ip = boost::asio::ip::address_v4((unsigned long)l_q.getint64(0));
			p_message_count = l_q.getint64(1);
			l_item.m_last_ip = p_last_ip;
			l_item.m_message_count = p_message_count;
			return true;
		}
		else
		{
			l_item.m_last_ip = boost::asio::ip::address_v4();
			l_item.m_message_count = 0;
		}
#endif
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_last_ip_and_user_stat: " + e.getError());
	}
	return false;
}
//========================================================================================================
CFlyRatioItem CFlylinkDBManager::load_ratio(uint32_t p_hub_id, const string& p_nick, CFlyUserRatioInfo& p_ratio_info, const boost::asio::ip::address_v4& p_last_ip)
{
	dcassert(p_hub_id != 0);
	dcassert(!p_nick.empty());
	CFlyRatioItem l_ip_ratio_item;
	try
	{
		if (!p_last_ip.is_unspecified()) // Если нет в таблице user_db.user_info, в fly_ratio можно не ходить - там ничего нет
		{
			Lock l(m_cs);
			if (!m_select_ratio_load.get())
				m_select_ratio_load = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                    "select upload,download,(select name from fly_dic where id = dic_ip)" // TODO перевести на хранение IP как числа?
				                                                                    "from fly_ratio where dic_nick=(select id from fly_dic where name=? and dic=2) and dic_hub=?\n"));
			sqlite3_command* l_sql_command = m_select_ratio_load.get();
			l_sql_command->bind(1, p_nick, SQLITE_STATIC);
			l_sql_command->bind(2, __int64(p_hub_id));
			sqlite3_reader l_q = l_sql_command->executereader();
			string l_ip_from_ratio;
			while (l_q.read())
			{
				l_ip_from_ratio = l_q.getstring(2);
				// dcassert(!l_ip_from_ratio.empty()); // TODO - сделать зачистку таких
				if (!l_ip_from_ratio.empty())
				{
					const auto l_u = l_q.getint64(0);
					const auto l_d = l_q.getint64(1);
					dcassert(l_d || l_u);
					l_ip_ratio_item.m_upload    += l_u;
					l_ip_ratio_item.m_download  += l_d;
					boost::system::error_code ec;
					const auto l_ip = boost::asio::ip::address_v4::from_string(l_ip_from_ratio, ec);
					dcassert(!ec);
					auto& l_u_d_map = p_ratio_info.find_ip_map(l_ip);
					l_u_d_map.m_download = l_d;
					l_u_d_map.m_upload   = l_u;
				}
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_ratio: " + e.getError());
	}
	return l_ip_ratio_item;
}
//========================================================================================================
uint32_t CFlylinkDBManager::get_dic_hub_id(const string& p_hub)
{
	Lock l(m_cs);
	return get_dic_idL(p_hub, e_DIC_HUB, true);
}
//========================================================================================================
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
__int64 CFlylinkDBManager::get_dic_location_id(const string& p_location)
{
	Lock l(m_cs);
	return get_dic_idL(p_location, e_DIC_LOCATION, true);
}
//========================================================================================================
__int64 CFlylinkDBManager::get_dic_country_id(const string& p_country)
{
	Lock l(m_cs);
	return get_dic_idL(p_country, e_DIC_COUNTRY, true);
}
#if 0
//========================================================================================================
void CFlylinkDBManager::clear_dic_cache_location()
{
	clear_dic_cache(e_DIC_LOCATION);
}
//========================================================================================================
void CFlylinkDBManager::clear_dic_cache_country()
{
	clear_dic_cache(e_DIC_COUNTRY);
}
//========================================================================================================
void CFlylinkDBManager::clear_dic_cache(const eTypeDIC p_DIC)
{
	Lock l(m_cs);
	m_DIC[p_DIC - 1].clear();
}
#endif
//========================================================================================================
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
//========================================================================================================
void CFlylinkDBManager::store_all_ratio_and_last_ip(uint32_t p_hub_id,
                                                    const string& p_nick,
                                                    const CFlyUploadDownloadMap* p_upload_download_stats,
                                                    const uint32_t p_message_count,
                                                    const boost::asio::ip::address_v4& p_last_ip)
{
	Lock l(m_cs);
	try
	{
		dcassert(p_hub_id);
		dcassert(!p_nick.empty());
		__int64 l_dic_nick = 0;
		if (p_upload_download_stats && !p_upload_download_stats->empty()) // Для рейтинга нужно расчитать ID ника
		{
			l_dic_nick = get_dic_idL(p_nick, e_DIC_NICK, true);
		}
		__int64 l_last_ip_id = 0;
		// Если запись 1- проверить что p_last_ip =
		if (p_upload_download_stats)
		{
#ifdef _DEBUG
			if (p_upload_download_stats->size() == 1)
			{
				dcassert(p_upload_download_stats->cbegin()->first == p_last_ip.to_ulong());
			}
#endif
			for (auto i = p_upload_download_stats->cbegin(); i != p_upload_download_stats->cend(); ++i)
			{
				l_last_ip_id = get_dic_idL(boost::asio::ip::address_v4(i->first).to_string(), e_DIC_IP, true); // Внешний цикл для создания последнего IP - вложенные транзакции нельзя
			}
		}
		if (p_upload_download_stats && !p_upload_download_stats->empty())
		{
			sqlite3_transaction l_trans_insert(m_flySQLiteDB); // Рейтинг пока пишем сразу в базу не отложенно
			if (!m_insert_ratio.get())
				m_insert_ratio = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                               "insert or replace into fly_ratio (dic_ip,dic_nick,dic_hub,upload,download) values(?,?,?,?,?)"));
			// TODO провести конвертацию в другой формат и файл БД + отказаться от DIC
			sqlite3_command* l_sql = m_insert_ratio.get();
			l_sql->bind(2, l_dic_nick);
			l_sql->bind(3, __int64(p_hub_id));
			for (auto i = p_upload_download_stats->cbegin(); i != p_upload_download_stats->cend(); ++i)
			{
				__int64 l_last_ip_id = get_dic_idL(boost::asio::ip::address_v4(i->first).to_string(), e_DIC_IP, false); // TODO - второй раз делаем запрос ! криво - изменить структуру fly_ratio
				dcassert(i->second.m_upload != 0 || i->second.m_download != 0);
				if (l_last_ip_id && // Коннект еще не наступил - не пишем в базу 0
				        (i->second.m_upload != 0 || i->second.m_download != 0)) // Если все по нулям - тоже странно
				{
					l_sql->bind(1, __int64(l_last_ip_id));
					l_sql->bind(4, __int64(i->second.m_upload));
					l_sql->bind(5, __int64(i->second.m_download));
					l_sql->executenonquery();
				}
			}
			l_trans_insert.commit();
		}
		// Иначе фиксируем только последний IP
		update_last_ip_deferredL(p_hub_id, p_nick, p_message_count, p_last_ip);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - store_all_ratio_and_last_ip: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::update_last_ip(uint32_t p_hub_id, const string& p_nick, const boost::asio::ip::address_v4& p_last_ip)
{
	//dcassert(!p_last_ip.is_unspecified());
	Lock l(m_cs);
	try
	{
		const __int64 l_message_count = 0; // счетчик мессаг не зануляем
		update_last_ip_deferredL(p_hub_id, p_nick, l_message_count, p_last_ip);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - update_last_ip: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::shutdown()
{
	flush_all_last_ip_and_message_count();
}
//========================================================================================================
void CFlylinkDBManager::flush_all_last_ip_and_message_count()
{
	Lock l(m_cs);
	try
	{
		CFlyLog l_log("[sqlite - flush-user-info]");
		int l_count = 0;
		{
			sqlite3_transaction l_trans_insert(m_flySQLiteDB);
			for (auto h = m_last_ip_cache.begin(); h != m_last_ip_cache.end(); ++h)
			{
				for (auto i = h->second.begin(); i != h->second.end(); ++i)
				{
					if (i->second.m_is_item_dirty)
					{
						++l_count;
						if (!m_insert_store_all_ip_and_message_count.get())
							m_insert_store_all_ip_and_message_count = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
							                                                                    "insert or replace into user_db.user_info (nick,dic_hub,last_ip,message_count) values(?,?,?,?)"));
						sqlite3_command* l_sql = m_insert_store_all_ip_and_message_count.get();
						l_sql->bind(1, i->first, SQLITE_STATIC);
						l_sql->bind(2, __int64(h->first));
						l_sql->bind(3, __int64(i->second.m_last_ip.to_ulong()));
						l_sql->bind(4, __int64(i->second.m_message_count));
						l_sql->executenonquery();
						i->second.m_is_item_dirty = false;
					}
				}
			}
			l_trans_insert.commit();
		}
		l_log.log("Save dirty record user_db.user_info:" + Util::toString(l_count));
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - flush_all_last_ip_and_message_count: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::update_last_ip_deferredL(uint32_t p_hub_id, const string& p_nick, const uint32_t p_message_count, const boost::asio::ip::address_v4& p_last_ip)
{
	dcassert(p_hub_id);
	dcassert(!p_nick.empty());
#if _DEBUG
	{
#if 0
		if (!m_select_store_ip.get())
			m_select_store_ip = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                  "select last_ip, message_count from user_db.user_info where nick=? and dic_hub=?"));
		m_select_store_ip.get()->bind(1, p_nick, SQLITE_STATIC);
		m_select_store_ip.get()->bind(2, __int64(p_hub_id));
		sqlite3_reader l_q = m_select_store_ip.get()->executereader();
		while (l_q.read())
		{
			const auto l_current_count = l_q.getint64(1);
			if (p_message_count < l_current_count)
			{
				dcassert(p_message_count >= l_current_count);
			}
		}
#endif
	}
#endif
	auto& l_hub_cache_item = m_last_ip_cache[p_hub_id][p_nick];
	if (!p_last_ip.is_unspecified() && p_message_count)
	{
#if 0
		if (!m_insert_store_ip_and_message_count.get())
			m_insert_store_ip_and_message_count = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                "insert or replace into user_db.user_info (nick,dic_hub,last_ip,message_count) values(?,?,?,?)"));
		sqlite3_command* l_sql = m_insert_store_ip_and_message_count.get();
		l_sql->bind(1, p_nick, SQLITE_STATIC);
		l_sql->bind(2, __int64(p_hub_id));
		l_sql->bind(3, __int64(p_last_ip.to_ulong()));
		l_sql->bind(4, __int64(p_message_count));
		l_sql->executenonquery();
#endif
		if (l_hub_cache_item.m_last_ip != p_last_ip || l_hub_cache_item.m_message_count != p_message_count)
		{
			l_hub_cache_item.m_is_item_dirty = true;
		}
		l_hub_cache_item.m_last_ip = p_last_ip;
		l_hub_cache_item.m_message_count = p_message_count;
	}
	else
	{
		if (!p_last_ip.is_unspecified())
		{
#if 0
			if (!m_insert_store_ip.get())
				m_insert_store_ip = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                  "insert or replace into user_db.user_info (nick,dic_hub,last_ip) values(?,?,?)"));
			sqlite3_command* l_sql = m_insert_store_ip.get();
			l_sql->bind(1, p_nick, SQLITE_STATIC);
			l_sql->bind(2, __int64(p_hub_id));
			l_sql->bind(3, __int64(p_last_ip.to_ulong()));
			l_sql->executenonquery();
#endif
			if (l_hub_cache_item.m_last_ip != p_last_ip)
			{
				l_hub_cache_item.m_is_item_dirty = true;
			}
			
			l_hub_cache_item.m_last_ip = p_last_ip;
		}
		if (p_message_count)
		{
#if 0
			if (!m_insert_store_message_count.get())
				m_insert_store_message_count = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                             "insert or replace into user_db.user_info (nick,dic_hub,message_count) values(?,?,?)"));
			sqlite3_command* l_sql = m_insert_store_message_count.get();
			l_sql->bind(1, p_nick, SQLITE_STATIC);
			l_sql->bind(2, __int64(p_hub_id));
			l_sql->bind(3, __int64(p_message_count));
			l_sql->executenonquery();
#endif
			if (l_hub_cache_item.m_message_count != p_message_count)
			{
				l_hub_cache_item.m_is_item_dirty = true;
			}
			l_hub_cache_item.m_message_count = p_message_count;
		}
	}
}
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
//========================================================================================================
bool CFlylinkDBManager::is_table_exists(const string& p_table_name)
{
	dcassert(p_table_name == Text::toLower(p_table_name));
	return m_flySQLiteDB.executeint(
	           "select count(*) from sqlite_master where type = 'table' and lower(tbl_name) = '" + p_table_name + "'") != 0;
}
//========================================================================================================
__int64 CFlylinkDBManager::find_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_cache_result)
{
	if (!m_select_fly_dic.get())
		m_select_fly_dic = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
		                                                                 "select id from fly_dic where name=? and dic=?"));
	sqlite3_command* l_sql = m_select_fly_dic.get();
	l_sql->bind(1, p_name, SQLITE_STATIC);
	l_sql->bind(2, p_DIC);
	__int64 l_dic_id = l_sql->executeint64_no_throw();
	if (l_dic_id && p_cache_result)
		m_DIC[p_DIC - 1][p_name] = l_dic_id;
	return l_dic_id;
}
//========================================================================================================
__int64 CFlylinkDBManager::get_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_create)
{
	dcassert(!p_name.empty());
	if (p_name.empty())
		return 0;
	try
	{
		auto& l_cache_dic = m_DIC[p_DIC - 1];
		if (!p_create)
		{
			auto i =  l_cache_dic.find(p_name);
			if (i != l_cache_dic.end()) // [1] https://www.box.net/shared/8f01665fe1a5d584021f
				return i->second;
			else
				return find_dic_idL(p_name, p_DIC, true);
		}
		__int64& l_Cache_ID = l_cache_dic[p_name];
		if (l_Cache_ID)
			return l_Cache_ID;
		l_Cache_ID = find_dic_idL(p_name, p_DIC, false);
		if (!l_Cache_ID)
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			if (!m_insert_fly_dic.get())
				m_insert_fly_dic = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                 "insert into fly_dic (dic,name) values(?,?)"));
			sqlite3_command* l_sql = m_insert_fly_dic.get();
			l_sql->bind(1, p_DIC);
			l_sql->bind(2, p_name, SQLITE_STATIC);
			l_sql->executenonquery();
			l_trans.commit();
			l_Cache_ID = m_flySQLiteDB.insertid();
		}
		return l_Cache_ID;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_dic_idL: " + e.getError());
	}
	return 0;
}
//========================================================================================================
/*
void CFlylinkDBManager::IPLog(const string& p_hub_ip,
               const string& p_ip,
               const string& p_nick)
{
 Lock l(m_cs);
 try {
    const __int64 l_hub_ip =  get_dic_idL(p_hub_ip,e_DIC_HUB);
    const __int64 l_nick =  get_dic_idL(p_nick,e_DIC_NICK);
    const __int64 l_ip =  get_dic_idL(p_ip,e_DIC_IP);
    sqlite3_transaction l_trans(m_flySQLiteDB);
    sqlite3_command* l_sql = prepareSQL(
     "insert into fly_ip_log(DIC_HUBIP,DIC_NICK,DIC_USERIP,STAMP) values (?,?,?,?);");
    l_sql->bind(1, l_hub_ip);
    l_sql->bind(2, l_nick);
    l_sql->bind(3, l_ip);
    SYSTEMTIME t;
    ::GetLocalTime(&t);
    char l_buf[64];
    sprintf(l_buf,"%02d.%02d.%04d %02d:%02d:%02d",t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute,t.wSecond);
    l_sql->bind(4, l_buf,strlen(l_buf));
    l_sql->executenonquery();
    l_trans.commit();
 }
    catch(const database_error& e)
    {
        errorDB("SQLite - IPLog: " + e.getError());
    }
}
*/
//========================================================================================================
void CFlylinkDBManager::SweepPath()
{
	Lock l(m_cs);
	try
	{
		for (auto i = m_path_cache.cbegin(); i != m_path_cache.cend(); ++i)
		{
			if (i->second.m_is_found == false)
			{
				{
					if (!m_sweep_path_file.get())
						m_sweep_path_file = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
						                                                                  "delete from fly_file where dic_path=?"));
					sqlite3_transaction l_trans(m_flySQLiteDB);
					m_sweep_path_file.get()->bind(1, i->second.m_path_id);
					m_sweep_path_file.get()->executenonquery();
					l_trans.commit();
				}
				if (!m_sweep_path.get())
					m_sweep_path = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                             "delete from fly_path where id=?"));
				sqlite3_transaction l_trans(m_flySQLiteDB);
				m_sweep_path.get()->bind(1, i->second.m_path_id);
				m_sweep_path.get()->executenonquery();
				l_trans.commit();
			}
		}
		LoadPathCache();
		{
			CFlyLog l(STRING(DELETE_FROM_FLY_HASH_BLOCK));
			sqlite3_transaction l_trans(m_flySQLiteDB);
			m_flySQLiteDB.executenonquery("delete FROM fly_hash_block where tth_id not in(select tth_id from fly_file)");
			l_trans.commit();
			m_tiger_tree_cache.clear();
		}
		{
			CFlyLog l("delete from media_db.fly_media where tth_id not in(select tth_id from fly_file)");
			sqlite3_transaction l_trans(m_flySQLiteDB);
			m_flySQLiteDB.executenonquery("delete from media_db.fly_media where tth_id not in(select tth_id from fly_file)");
			l_trans.commit();
		}
#ifdef PPA_USE_VACUUM
		LogManager::getInstance()->message("start vacuum", true); // TODO translate
		m_flySQLiteDB.executenonquery("VACUUM;");
		LogManager::getInstance()->message("stop vacuum", true); // TODO translate
#endif
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - SweepPath: " + e.getError()); // TODO translate
	}
}
//========================================================================================================
void CFlylinkDBManager::LoadPathCache()
{
	CFlyLog log(STRING(RELOAD_DIR));
	Lock l(m_cs);
	m_convert_ftype_stop_key = 0;
	m_path_cache.clear();
	try
	{
		if (!m_load_path_cache.get())
			m_load_path_cache = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                  "select id,name,(select count(*) from fly_file where dic_path = fly_path.id and (media_audio is not null or media_video is not null)) cnt_mediainfo from fly_path"));
		// Версия 2
		// select id,name,(select 1 from fly_file where dic_path = fly_path.id and (media_audio is not null or media_video is not null) limit 1) cnt_mediainfo from fly_path
		sqlite3_reader l_q = m_load_path_cache.get()->executereader();
		while (l_q.read())
		{
			m_path_cache.insert(std::make_pair(l_q.getstring(1), CFlyPathItem(l_q.getint64(0), false, l_q.getint(2) == 0)));
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - LoadPathCache: " + e.getError()); // TODO translate
		return;
	}
}
//========================================================================================================
__int64 CFlylinkDBManager::get_path_id(string p_path, bool p_create, bool p_case_convet, bool& p_is_no_mediainfo)
{
	Lock l(m_cs);
	p_is_no_mediainfo = false;
	if (m_last_path_id != 0 && m_last_path_id != -1 && p_path == m_last_path)
		return m_last_path_id;
		
	m_last_path = p_path;
	if (p_case_convet)
		p_path = Text::toLower(p_path);
	try
	{
		CFlyPathCache::iterator l_item_iter = m_path_cache.find(p_path);
		if (l_item_iter != m_path_cache.end())
		{
			l_item_iter->second.m_is_found = true;
			m_last_path_id = l_item_iter->second.m_path_id;
			p_is_no_mediainfo = l_item_iter->second.m_is_no_mediainfo;
			return m_last_path_id;
		}
		if (!m_get_path_id.get())
			m_get_path_id = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                              "select id from fly_path where name=?;"));
		m_get_path_id.get()->bind(1, p_path, SQLITE_STATIC);
		m_last_path_id = m_get_path_id.get()->executeint64_no_throw();
		if (m_last_path_id)
			return m_last_path_id;
		else if (p_create)
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			if (!m_insert_fly_path.get())
				m_insert_fly_path = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                  "insert into fly_path (name) values(?)"));
			m_insert_fly_path.get()->bind(1, p_path, SQLITE_STATIC);
			m_insert_fly_path.get()->executenonquery();
			l_trans.commit();
			m_last_path_id = m_flySQLiteDB.insertid();
			m_path_cache[p_path] = CFlyPathItem(m_last_path_id, true, false);
			return m_last_path_id;
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_path_id: " + e.getError()); // TODO translate
	}
	return 0;
}
//========================================================================================================
bool CFlylinkDBManager::findTTH(const string& p_fname, const string& p_fpath, TTHValue& p_tth)
{
	Lock l(m_cs);
	try
	{
		bool p_is_no_mediainfo;
		const __int64 l_path_id = get_path_id(p_fpath, false, true, p_is_no_mediainfo);
		if (!l_path_id)
			return 0;
		if (!m_findTTH.get())
			m_findTTH = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                          "select tth from fly_hash where id = (select tth_id from fly_file where name=? and dic_path=?)"));
		sqlite3_command* l_sql = m_findTTH.get();
		l_sql->bind(1, p_fname, SQLITE_STATIC);
		l_sql->bind(2, l_path_id);
		sqlite3_reader l_q = m_findTTH.get()->executereader();
		if (l_q.read())
		{
			vector<uint8_t> l_tth;
			l_q.getblob(1, l_tth);
			dcassert(l_tth.size() == 24);
			if (l_tth.size() == 24)
			{
				p_tth = TTHValue(&l_tth[0]);
				return true;
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - findTTH: " + e.getError()); // TODO translate
	}
	return 0;
}
//========================================================================================================
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
void CFlylinkDBManager::SweepFiles(__int64 p_path_id, const CFlyDirMap& p_sweep_files)
{
	Lock l(m_cs);
	try
	{
		for (auto i = p_sweep_files.cbegin(); i != p_sweep_files.cend(); ++i)
		{
			if (i->second.m_is_found == false)
			{
				if (!m_sweep_dir_sql.get())
					m_sweep_dir_sql = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                "delete from fly_file where dic_path=? and name=?"));
				sqlite3_transaction l_trans(m_flySQLiteDB);
				m_sweep_dir_sql.get()->bind(1, p_path_id);
				m_sweep_dir_sql.get()->bind(2, i->first);
				m_sweep_dir_sql.get()->executenonquery();
				l_trans.commit();
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - SweepFiles: " + e.getError()); // TODO translate
	}
}
#endif
//========================================================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
bool CFlylinkDBManager::load_media_info(const TTHValue& p_tth, CFlyMediaInfo& p_media_info, bool p_only_inform)
{
	Lock l(m_cs);
	try
	{
		const __int64 l_tth_id = get_tth_idL(p_tth, false);
		if (l_tth_id)
		{
			// Читаем базовую инфу (TODO - если есть в памяти попробовать забрать из нее)
			if (!m_load_mediainfo_base.get())
				m_load_mediainfo_base = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                      "select bitrate,media_x,media_y,media_video,media_audio from fly_file where tth_id = ? limit 1"));
			m_load_mediainfo_base.get()->bind(1, l_tth_id);
			sqlite3_reader l_q_base = m_load_mediainfo_base.get()->executereader();
			if (l_q_base.read()) // забираем всегда одну запись. (limit = 1) в шаре может быть несколько одинаковых файлов в разных каталогах.
			{
				p_media_info.m_bitrate = l_q_base.getint(0);
				p_media_info.m_mediaX = l_q_base.getint(1);
				p_media_info.m_mediaY = l_q_base.getint(2);
				p_media_info.m_video = l_q_base.getstring(3);
				p_media_info.m_audio = l_q_base.getstring(4);
				// p_media_info.calcEscape(); // Тут это не нужно
			}
			if (p_media_info.isMedia()) // Если есть базовая информация - попытаемся забрать дополнительную.
			{
				if (!p_only_inform)
				{
					if (!m_load_mediainfo_ext.get())
						m_load_mediainfo_ext = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
						                                                                     "select stream_type,channel,param,value from media_db.fly_media\n"
						                                                                     "where tth_id = ? order by stream_type,channel"));
					m_load_mediainfo_ext.get()->bind(1, l_tth_id);
				}
				else
				{
					if (!m_load_mediainfo_ext_only_inform.get())
						m_load_mediainfo_ext_only_inform = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
						                                                             "select stream_type,channel,param,value from media_db.fly_media\n"
						                                                             "where tth_id = ? and param = 'Infrom' order by stream_type,channel"));
					m_load_mediainfo_ext_only_inform.get()->bind(1, l_tth_id);
				}
				sqlite3_reader l_q;
				if (p_only_inform)
					l_q = m_load_mediainfo_ext_only_inform.get()->executereader();
				else
					l_q = m_load_mediainfo_ext.get()->executereader();
				while (l_q.read())
				{
					CFlyMediaInfo::ExtItem l_item;
					l_item.m_stream_type = l_q.getint(0);
					l_item.m_channel = l_q.getint(1);
					l_item.m_param = l_q.getstring(2);
					l_item.m_value = l_q.getstring(3);
					p_media_info.m_ext_array.push_back(l_item);
				}
			}
		}
		return l_tth_id != 0;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_media_info: " + e.getError()); // TODO translate
	}
	return false;
}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
//========================================================================================================
void CFlylinkDBManager::LoadDir(__int64 p_path_id, CFlyDirMap& p_dir_map, bool p_is_no_mediainfo)
{
	Lock l(m_cs);
	try
	{
		sqlite3_command* l_sql;
		if (!p_is_no_mediainfo)
		{
			if (!m_load_dir_sql.get())
				m_load_dir_sql = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                               "select size,stamp,tth,name,hit,stamp_share,ftype,bitrate,media_x,media_y,media_video,media_audio from fly_file ff,fly_hash fh where "
				                                                               "ff.dic_path=? and ff.tth_id=fh.id"));
			l_sql = m_load_dir_sql.get();
		}
		else
		{
			if (!m_load_dir_sql_without_mediainfo.get())
				m_load_dir_sql_without_mediainfo = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                             "select size,stamp,tth,name,hit,stamp_share,ftype from fly_file ff,fly_hash fh where "
				                                                             "ff.dic_path=? and ff.tth_id=fh.id"));
				                                                             
			l_sql = m_load_dir_sql_without_mediainfo.get();
		}
		l_sql->bind(1, p_path_id);
		// TODO - если каталог не содержит инфы - не забирать колонки из базы
		// Для реализации нужно еще одно поле - флаг в таблице fly_path
		// Пока не понятно как его актуализацировать
		sqlite3_reader l_q = l_sql->executereader();
		bool l_calc_ftype = false;
		while (l_q.read())
		{
			const string l_name = l_q.getstring(3);
			CFlyFileInfo& l_info = p_dir_map[l_name];
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
			l_info.m_is_found = false;
#endif
			l_info.m_recalc_ftype = false;
			l_info.m_size   = l_q.getint64(0);
			l_info.m_TimeStamp  = l_q.getint64(1);
			l_info.m_StampShare  = l_q.getint64(5);
			if (!l_info.m_StampShare)
				l_info.m_StampShare = l_info.m_TimeStamp;
			l_info.m_hit = uint32_t(l_q.getint(4));
			const int l_ftype = l_q.getint(6);
			if (l_ftype == -1)
			{
				l_info.m_recalc_ftype = true;
				l_calc_ftype = true;
				l_info.m_ftype = char(ShareManager::getFType(l_name));
			}
			else
			{
				l_info.m_ftype = char(Search::TypeModes(l_ftype));
			}
			if (!p_is_no_mediainfo) // Подсказка из кэша каталога.
			{
				const string& l_audio = l_q.getstring(11); // TODO вытащить длительность в отдельный атрибут "4mn 26s | MPEG, 2.0, 128 Kbps"
				const string& l_video = l_q.getstring(10);
				if (!l_audio.empty() || !l_video.empty())
				{
					l_info.m_media_ptr = std::make_shared<CFlyMediaInfo>();
					l_info.m_media_ptr->m_bitrate = uint16_t(l_q.getint(7));
					l_info.m_media_ptr->m_mediaX  = uint16_t(l_q.getint(8));
					l_info.m_media_ptr->m_mediaY  = uint16_t(l_q.getint(9));
					l_info.m_media_ptr->m_video   = l_video;
					l_info.m_media_ptr->m_audio   = l_audio;
					l_info.m_media_ptr->calcEscape();
				}
			}
			else
			{
				dcassert(l_info.m_media_ptr == nullptr);
				l_info.m_media_ptr = nullptr;
			}
			l_q.getblob(2, &l_info.m_tth, 24);
		}
		// тормозит...
		if (l_calc_ftype && m_convert_ftype_stop_key < 200)
		{
			if (!m_set_ftype.get())
				m_set_ftype  = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                             "update fly_file set ftype=? where name=? and dic_path=? and ftype=-1"));
			sqlite3_transaction l_trans(m_flySQLiteDB);
			const auto &l_set_ftype_get = m_set_ftype.get(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_set_ftype.get()' expression repeatedly. cflylinkdbmanager.cpp 1992
			l_set_ftype_get->bind(3, p_path_id);
			for (auto i = p_dir_map.cbegin(); i != p_dir_map.cend(); ++i)
			{
				if (i->second.m_recalc_ftype)
				{
					m_convert_ftype_stop_key++;
					l_set_ftype_get->bind(1, i->second.m_ftype);
					l_set_ftype_get->bind(2, i->first, SQLITE_STATIC);
					l_set_ftype_get->executenonquery();
				}
			}
			l_trans.commit();
		}
		
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - LoadDir: " + e.getError()); // TODO translate
	}
}
//========================================================================================================
void CFlylinkDBManager::updateFileInfo(const string& p_fname, __int64 p_path_id,
                                       int64_t p_Size, int64_t p_TimeStamp, __int64 p_tth_id)
{
	if (!m_update_file.get())
		m_update_file = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
		                                                              "update fly_file set size=?,stamp=?,tth_id=?,stamp_share=? where name=? and dic_path=?;"));
	sqlite3_transaction l_trans(m_flySQLiteDB);
	const auto &l_update_file = m_update_file.get(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_update_file.get()' expression repeatedly. cflylinkdbmanager.cpp 2021
	l_update_file->bind(1, p_Size);
	l_update_file->bind(2, p_TimeStamp);
	l_update_file->bind(3, p_tth_id);
	l_update_file->bind(4, int64_t(File::currentTime()));
	l_update_file->bind(5, p_fname, SQLITE_STATIC);
	l_update_file->bind(6, p_path_id);
	l_update_file->executenonquery();
	l_trans.commit();
}
//========================================================================================================
bool CFlylinkDBManager::checkTTH(const string& p_fname, __int64 p_path_id,
                                 int64_t p_Size, int64_t p_TimeStamp, TTHValue& p_out_tth)
{
	Lock l(m_cs);
	try
	{
		if (!m_check_tth_sql.get())
			m_check_tth_sql = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                "select size,stamp,tth from fly_file ff,fly_hash fh where\n"
			                                                                "fh.id=ff.tth_id and ff.name=? and ff.dic_path=?"));
		// TODO - тут индекс по tth_id не мешается?
		// Протестировать и забрать tth из fly_hash подзапросом
		//
		dcassert(p_fname == Text::toLower(p_fname));
		m_check_tth_sql.get()->bind(1, p_fname, SQLITE_STATIC);
		m_check_tth_sql.get()->bind(2, p_path_id);
		sqlite3_reader l_q = m_check_tth_sql.get()->executereader();
		if (l_q.read())
			// 2012-05-11_23-53-01_WUUQMV7KVCODO3KRNOPB3YHXKYCEBYFA47ZIRFA_5ADD234D_crash-stack-r502-beta26-build-9946.dmp
		{
			const int64_t l_size   = l_q.getint64(0);
			const int64_t l_stamp  = l_q.getint64(1);
			l_q.getblob(2, p_out_tth.data, 24);
			if (l_stamp != p_TimeStamp || l_size != p_Size)
			{
				l_q.close();
				updateFileInfo(p_fname, p_path_id, -1, -1, -1);
				return false;
			}
			return true; //-V612
		}
		return false;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - checkTTH: " + e.getError()); // TODO translate
	}
	return false;
}
//========================================================================================================
// [+] brain-ripper
unsigned __int64 CFlylinkDBManager::getBlockSizeSQL(const TTHValue& p_root, __int64 p_size)
{
	unsigned __int64 l_blocksize = 0;
	Lock l(m_cs);
	try
	{
		if (!m_get_blocksize.get())
			m_get_blocksize = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                "select file_size,block_size from fly_hash_block where tth_id=(select id from fly_hash where tth=?)"));
			                                                                
		m_get_blocksize->bind(1, p_root.data, 24, SQLITE_STATIC);
		sqlite3_reader l_q = m_get_blocksize.get()->executereader();
		if (l_q.read())
		{
#ifdef _DEBUG
			const __int64 l_size_file = l_q.getint64(0);
			dcassert(l_size_file == p_size);
#endif
			l_blocksize = l_q.getint64(1);
			if (l_blocksize == 0)
			{
				l_blocksize = TigerTree::getMaxBlockSize(l_q.getint64(0));
			}
			dcassert(l_blocksize);
			return l_blocksize;
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - getBlockSize: " + e.getError()); // TODO translate
	}
	l_blocksize = TigerTree::getMaxBlockSize(p_size);
	dcassert(l_blocksize);
	return l_blocksize;
}
//========================================================================================================
void CFlylinkDBManager::clear_tiger_tree_cache(const TTHValue& p_root)
{
	Lock l(m_cs);
	m_tiger_tree_cache.erase(p_root);
}
//========================================================================================================
bool CFlylinkDBManager::getTree(const TTHValue& p_root, TigerTree& p_tt)
{
	dcassert(p_root != TTHValue());
	Lock l(m_cs); // TODO dead lock https://code.google.com/p/flylinkdc/issues/detail?id=1028
	try
	{
		const auto& l_cache_tt = m_tiger_tree_cache.find(p_root);
		if (l_cache_tt != m_tiger_tree_cache.end())
		{
#ifdef _DEBUG
			LogManager::getInstance()->message("[!] Cache! bingo! CFlylinkDBManager::getTree TTH Root = " + p_root.toBase32());
#endif
			p_tt = l_cache_tt->second;
			return true;
		}
		if (!m_get_tree.get())
			m_get_tree = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                           "select tiger_tree,file_size,block_size from fly_hash_block where tth_id=(select id from fly_hash where tth=?)"));
		m_get_tree->bind(1, p_root.data, 24, SQLITE_STATIC);
		sqlite3_reader l_q = m_get_tree.get()->executereader();
		if (l_q.read())
		{
			const __int64 l_file_size = l_q.getint64(1);
			__int64 l_blocksize = l_q.getint64(2);
			if (l_blocksize == 0)
				l_blocksize = TigerTree::getMaxBlockSize(l_file_size);
				
			if (l_file_size <= MIN_BLOCK_SIZE)
			{
				p_tt = TigerTree(l_file_size, l_blocksize, p_root);
				m_tiger_tree_cache.insert(make_pair(p_root, p_tt));
				dcassert(p_tt.getRoot() == p_root);
				const auto l_result = p_tt.getRoot() == p_root;
				if (l_result)
				{
					m_tiger_tree_cache.insert(make_pair(p_root, p_tt));
				}
				return l_result;
			}
			vector<uint8_t> l_buf;
			l_q.getblob(0, l_buf);
			if (!l_buf.empty())
			{
				p_tt = TigerTree(l_file_size, l_blocksize, &l_buf[0], l_buf.size());
				dcassert(p_tt.getRoot() == p_root);
				const auto l_result = p_tt.getRoot() == p_root;
				if (l_result)
				{
					m_tiger_tree_cache.insert(make_pair(p_root, p_tt));
				}
				return l_result;
			}
			else
			{
				dcassert(0);
				return false;
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - getTree: " + e.getError()); // TODO translate
	}
	return false;
}
//========================================================================================================
__int64 CFlylinkDBManager::get_tth_idL(const TTHValue& p_tth, bool p_create /*= true*/)
{
	try
	{
		if (!m_get_tth_id.get())
			m_get_tth_id = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                             "select id from fly_hash where tth=?"));
		sqlite3_command* l_sql = m_get_tth_id.get();
		l_sql->bind(1, p_tth.data, 24, SQLITE_STATIC);
		if (const __int64 l_ID = l_sql->executeint64_no_throw())
			return l_ID;
		else if (p_create)
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			if (!m_insert_fly_hash.get())
				m_insert_fly_hash = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                  "insert into fly_hash (tth) values(?)"));
			sqlite3_command* l_sql_insert = m_insert_fly_hash.get();
			l_sql_insert->bind(1, p_tth.data, 24, SQLITE_STATIC);
			l_sql_insert->executenonquery();
			l_trans.commit();
			return m_flySQLiteDB.insertid();
		}
		else
			return 0;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_tth_idL: " + e.getError()); // TODO translate
	}
	return 0;
}
//========================================================================================================
void CFlylinkDBManager::Hit(const string& p_Path, const string& p_FileName)
{
	Lock l(m_cs);
	try
	{
		bool p_is_no_mediainfo;
		const __int64 l_path_id = get_path_id(p_Path, false, true, p_is_no_mediainfo);
		if (!l_path_id)
			return;
		if (!m_upload_file.get())
			m_upload_file = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                              "update fly_file set hit=hit+1 where name=? and dic_path=?"));
		sqlite3_transaction l_trans(m_flySQLiteDB);
		sqlite3_command* l_sql = m_upload_file.get();
		l_sql->bind(1, p_FileName, SQLITE_STATIC);
		l_sql->bind(2, l_path_id);
		l_sql->executenonquery();
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - Hit: " + e.getError()); // TODO translate
	}
}
//========================================================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
void CFlylinkDBManager::merge_mediainfo_ext(const __int64 l_tth_id, const CFlyMediaInfo& p_media, bool p_delete_old_info)
{
	if (p_delete_old_info)
	{
		if (!m_delete_mediainfo.get())
			m_delete_mediainfo = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                   "delete from media_db.fly_media where tth_id=?\n"));
		m_delete_mediainfo.get()->bind(1, l_tth_id);
		m_delete_mediainfo.get()->executenonquery();
	}
	if (p_media.isMediaAttrExists())
	{
		if (!m_insert_mediainfo.get())
			m_insert_mediainfo = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                   "insert or replace into media_db.fly_media\n"
			                                                                   "(tth_id,stream_type,channel,param,value) values (?,?,?,?,?);"));
		sqlite3_command* l_sql = m_insert_mediainfo.get();
		for (auto i = p_media.m_ext_array.cbegin(); i != p_media.m_ext_array.cend(); ++i)
		{
			if (i->m_is_delete == false) // Это параметр временный и его писать в базу не нужно
				// TODO - пересмотреть алгоритм и при генерации массива удалять лишнии записи.
				// убрав лишний флаг-мембер в ExtItem
			{
				l_sql->bind(1, l_tth_id);
				l_sql->bind(2, i->m_stream_type);
				l_sql->bind(3, i->m_channel);
				l_sql->bind(4, i->m_param, SQLITE_STATIC);
				l_sql->bind(5, i->m_value, SQLITE_STATIC);
				l_sql->executenonquery();
			}
		}
	}
}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
//========================================================================================================
#ifdef USE_REBUILD_MEDIAINFO
bool CFlylinkDBManager::rebuild_mediainfo(const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media, const TTHValue& p_tth)
{
	Lock l(m_cs);
	try
	{
		dcassert(p_path_id);
		if (!p_path_id)
			return false;
		const __int64 l_tth_id = get_tth_idL(p_tth, false);
		if (!l_tth_id)
			return false;
		dcassert(l_tth_id);
		merge_mediainfo(l_tth_id, p_path_id, p_file_name, p_media);
		return true;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - rebuild_mediainfo: " + e.getError()); // TODO translate
	}
	return false;
}
#endif // USE_REBUILD_MEDIAINFO
//========================================================================================================
bool CFlylinkDBManager::merge_mediainfo(const __int64 p_tth_id, const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media)
{
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_update_base_mediainfo.get())
			m_update_base_mediainfo = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                        "update fly_file set\n"
			                                                                        "bitrate=?, media_x=?, media_y=?, media_video=?, media_audio=?\n"
			                                                                        "where dic_path=? and name=?"));
		sqlite3_command* l_sql = m_update_base_mediainfo.get();
		if (p_media.m_bitrate)
			l_sql->bind(1, p_media.m_bitrate);
		else
			l_sql->bind(1);
		if (p_media.m_mediaX)
			l_sql->bind(2, p_media.m_mediaX);
		else
			l_sql->bind(2);
		if (p_media.m_mediaY)
			l_sql->bind(3, p_media.m_mediaY);
		else
			l_sql->bind(3);
		if (!p_media.m_video.empty())
			l_sql->bind(4, p_media.m_video, SQLITE_STATIC);
		else
			l_sql->bind(4);
		if (!p_media.m_audio.empty())
			l_sql->bind(5, p_media.m_audio, SQLITE_STATIC);
		else
			l_sql->bind(5);
		l_sql->bind(6, p_path_id);
		dcassert(p_file_name == Text::toLower(p_file_name));
		l_sql->bind(7, p_file_name, SQLITE_STATIC);
		l_sql->executenonquery();
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		merge_mediainfo_ext(p_tth_id, p_media, false);
#endif
		l_trans.commit();
		return true;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_mediainfo: " + e.getError()); // TODO translate
	}
	return false;
}
//========================================================================================================
__int64 CFlylinkDBManager::merge_file(const string& p_Path, const string& p_file_name,
                                      const int64_t p_time_stamp, const TigerTree& p_tt,
                                      bool p_case_convet, __int64& p_path_id)
{
	__int64 l_tth_id = 0;
	Lock l(m_cs);
	try
	{
		bool p_is_no_mediainfo;
		p_path_id = get_path_id(p_Path, true, p_case_convet, p_is_no_mediainfo);
		dcassert(p_path_id);
		if (!p_path_id)
			return 0;
		l_tth_id = addTree(p_tt); // Отдельная транзакция
		dcassert(l_tth_id);
		if (!l_tth_id)
			return 0;
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_insert_file.get())
			m_insert_file = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                              "insert or replace into fly_file (tth_id, dic_path, name, size, stamp, stamp_share, ftype)\n"
			                                                              "values(?,?,?,?,?,?,?);"));
		sqlite3_command* l_sql = m_insert_file.get();
		l_sql->bind(1, l_tth_id);
		l_sql->bind(2, p_path_id);
		dcassert(p_file_name == Text::toLower(p_file_name));
		l_sql->bind(3, p_file_name, SQLITE_STATIC);
		l_sql->bind(4, p_tt.getFileSize());
		l_sql->bind(5, p_time_stamp);
		l_sql->bind(6, int64_t(File::currentTime()));
		l_sql->bind(7, ShareManager::getFType(p_file_name));
		l_sql->executenonquery();
		l_trans.commit();
		return l_tth_id;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_file: " + e.getError()); // TODO translate
	}
	return l_tth_id;
}
//========================================================================================================
__int64 CFlylinkDBManager::addTree(const TigerTree& p_tt)
{
	Lock l(m_cs);
	m_tiger_tree_cache.erase(p_tt.getRoot()); // Сбросим кэш, чтобы случайно не достать старую карту.
	const __int64 l_tth_id = get_tth_idL(p_tt.getRoot(), true); // TODO делается коммит - сделать операцию атомарной
	dcassert(l_tth_id != 0);
	if (!l_tth_id) // Этого не может быть
		return 0;
	try
	{
		const int l_size = p_tt.getLeaves().size() * TTHValue::BYTES;
		const __int64 l_file_size = p_tt.getFileSize();
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_ins_fly_hash_block.get())
			m_ins_fly_hash_block = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                     "insert or replace into fly_hash_block (tth_id,file_size,tiger_tree,block_size) values(?,?,?,?)"));
		sqlite3_command* l_sql = m_ins_fly_hash_block.get();
		l_sql->bind(1, l_tth_id);
		l_sql->bind(2, l_file_size);
		l_sql->bind(3, p_tt.getLeaves()[0].data,  l_file_size > MIN_BLOCK_SIZE ? l_size : 0, SQLITE_STATIC);
		l_sql->bind(4, p_tt.getBlockSize());
		l_sql->executenonquery();
		l_trans.commit();
		return l_tth_id;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - addTree: " + e.getError()); // TODO translate
	}
	return 0;
}
//========================================================================================================
CFlylinkDBManager::~CFlylinkDBManager()
{
	shutdown();
#ifdef _DEBUG
	for (auto h = m_last_ip_cache.cbegin(); h != m_last_ip_cache.cend(); ++h)
	{
		for (auto i = h->second.begin(); i != h->second.end(); ++i)
		{
			dcassert(i->second.m_is_item_dirty == false);
		}
	}
	
	{
		FastLock l(m_cache_location_cs);
		dcdebug("CFlylinkDBManager::m_country_cache size = %d\n", m_country_cache.size());
	}
	{
		FastLock l(m_cache_location_cs);
		dcdebug("CFlylinkDBManager::m_location_cache size = %d\n", m_location_cache.size());
	}
#endif
}
//========================================================================================================
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
double CFlylinkDBManager::get_ratio() const
{
	dcassert(m_is_load_global_ratio);
	if (m_global_ratio.m_download > 0)
		return m_global_ratio.m_upload / m_global_ratio.m_download;
	else
		return 0;
}
//========================================================================================================
tstring CFlylinkDBManager::get_ratioW() const
{
	dcassert(m_is_load_global_ratio);
	if (m_global_ratio.m_download > 0)
	{
		LocalArray<TCHAR, 32> buf;
		snwprintf(buf.data(), buf.size(), _T("%.2f"), get_ratio());
		return buf.data();
	}
	return Util::emptyStringT;
}
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
//========================================================================================================
void CFlylinkDBManager::push_add_share_tth_(const TTHValue& p_tth)
{
#ifdef FLYLINKDC_USE_LEVELDB
	m_flyLevelDB.set_bit(p_tth, PREVIOUSLY_BEEN_IN_SHARE);
#endif // FLYLINKDC_USE_LEVELDB
}
//========================================================================================================
void CFlylinkDBManager::push_download_tth(const TTHValue& p_tth)
{
#ifdef FLYLINKDC_USE_LEVELDB
	m_flyLevelDB.set_bit(p_tth, PREVIOUSLY_DOWNLOADED);
#else
	Lock l(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_insert_fly_tth.get())
			m_insert_fly_tth = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                 "insert or replace into fly_tth(tth) values(?)"));
		m_insert_fly_tth.get()->bind(1, p_tth.data, 24, SQLITE_STATIC);
		m_insert_fly_tth.get()->executenonquery();
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - push_download_tth: " + e.getError()); // TODO translate
	}
#endif // FLYLINKDC_USE_LEVELDB
}
//========================================================================================================
CFlylinkDBManager::FileStatus CFlylinkDBManager::get_status_file(const TTHValue& p_tth)
{
#ifdef FLYLINKDC_USE_LEVELDB
	string l_status;
	m_flyLevelDB.get_value(p_tth, true, l_status);
	int l_result = Util::toInt(l_status);
	dcassert(l_result >= 0 && l_result <= 3);
	return static_cast<FileStatus>(l_result); // 1 - скачивал, 2 - был в шаре, 3 - 1+2 и то и то :)
#else
	Lock l(m_cs);
	try
	{
		if (!m_get_status_file.get())
			m_get_status_file = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                  "select 1 from fly_tth where tth=?\r\n"
			                                                                  "union all\r\n"
			                                                                  "select 2 from fly_hash where tth=?"));
		sqlite3_command* l_sql = m_get_status_file.get();
		l_sql->bind(1, p_tth.data, 24, SQLITE_STATIC);
		l_sql->bind(2, p_tth.data, 24, SQLITE_STATIC);
		sqlite3_reader l_q = l_sql->executereader();
		int l_result = 0;
		while (l_q.read())
		{
			l_result += l_q.getint(0);
		}
		dcassert(l_result >= 0 && l_result <= 3); // [+] IRainman fix.
		return static_cast<FileStatus>(l_result); // 1 - скачивал, 2 - был в шаре, 3 - 1+2 и то и то :)
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_status_file: " + e.getError()); // TODO translate
	}
	return UNKNOWN;
#endif // FLYLINKDC_USE_LEVELDB
}
//========================================================================================================
void CFlylinkDBManager::log(const int p_area, const StringMap& p_params)
{
	Lock l(m_cs); // [2] https://www.box.net/shared/9e63916273d37e5b2932
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_insert_fly_message.get())
			m_insert_fly_message = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                     "insert into log_db.fly_log(sdate,type,body,hub,nick,ip,file,source,target,fsize,fchunk,extra,userCID)"
			                                                                     " values(datetime('now','localtime'),?,?,?,?,?,?,?,?,?,?,?,?);"));
		const auto &l_insert_fly_message_get = m_insert_fly_message.get(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_insert_fly_message.get()' expression repeatedly. cflylinkdbmanager.cpp 2482
		l_insert_fly_message_get->bind(1, p_area);
		l_insert_fly_message_get->bind(2, getString(p_params, "message"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(3, getString(p_params, "hubURL"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(4, getString(p_params, "myNI"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(5, getString(p_params, "ip"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(6, getString(p_params, "file"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(7, getString(p_params, "source"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(8, getString(p_params, "target"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(9, getString(p_params, "fileSI"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(10, getString(p_params, "fileSIchunk"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(11, getString(p_params, "extra"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->bind(12, getString(p_params, "userCID"), SQLITE_TRANSIENT);
		l_insert_fly_message_get->executenonquery();
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - log: " + e.getError() + " parrent message: " + getString(p_params, "message"));
	}
}
//========================================================================================================
__int64 CFlylinkDBManager::convert_tth_history()
{
#ifdef FLYLINKDC_USE_LEVELDB
	__int64 l_count = 0;
	if (CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_IsTTHLevelDBConvert) == 0)
	{
		Lock l(m_cs);
		try
		{
			auto_ptr<sqlite3_command> l_sql = auto_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                            "select tth, sum(val) from ( "
			                                                            "select 1 val, tth from fly_tth "
			                                                            "union all "
			                                                            "select 2 val, tth from fly_hash) "
			                                                            "group by tth"));
			sqlite3_reader l_q = l_sql->executereader();
			while (l_q.read())
			{
				vector<uint8_t> l_tth;
				l_q.getblob(0, l_tth);
				dcassert(l_tth.size() == 24);
				if (l_tth.size() == 24)
				{
					m_flyLevelDB.set_bit(TTHValue(&l_tth[0]), l_q.getint(1));
					++l_count;
				}
			}
			CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_IsTTHLevelDBConvert, 1);
			return l_count;
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - convert_tth_history: " + e.getError());
		}
	}
	return l_count;
#endif
}
#ifdef FLYLINKDC_USE_LEVELDB
//========================================================================================================
CFlyLevelDB::CFlyLevelDB(): m_db(nullptr)
{
	m_readoptions.verify_checksums = true;
	m_readoptions.fill_cache = true;
	
	m_iteroptions.verify_checksums = true;
	m_iteroptions.fill_cache = false;
	
	m_writeoptions.sync      = true;
	
	m_options.compression = leveldb::kNoCompression;
	m_options.max_open_files = 10;
	m_options.block_size = 4096;
	m_options.write_buffer_size = 1 << 20;
	m_options.block_cache = leveldb::NewLRUCache(1 * 1024); // 1M
	m_options.paranoid_checks = true;
	m_options.filter_policy = leveldb::NewBloomFilterPolicy(10);
	m_options.create_if_missing = true;
}
//========================================================================================================
CFlyLevelDB::~CFlyLevelDB()
{
	delete m_db;
	delete m_options.filter_policy;
	delete m_options.block_cache;
	delete m_options.env; // http://code.google.com/p/leveldb/issues/detail?id=194 ?
	// TODO - leak delete m_options.comparator;
}
//========================================================================================================
bool CFlyLevelDB::open_level_db(const string& p_db_name)
{
	auto l_status = leveldb::DB::Open(m_options, p_db_name, &m_db);
	if (!l_status.ok())
	{
		const auto l_result_error = l_status.ToString();
		Util::setRegistryValueString(FLYLINKDC_REGISTRY_LEVELDB_ERROR , Text::toT(l_result_error));
		if (l_status.IsIOError())
		{
			LogManager::getInstance()->message("[CFlyLevelDB::open_level_db] l_status.IsIOError() = " + l_result_error, true);
			dcassert(0);
			// most likely there's another instance running or the permissions are wrong
//			messageF(STRING_F(DB_OPEN_FAILED_IO, getNameLower() % Text::toUtf8(ret.ToString()) % APPNAME % dbPath % APPNAME), false, true);
//			exit(0);
		}
		else
		{
			LogManager::getInstance()->message("[CFlyLevelDB::open_level_db] !l_status.IsIOError() the database is corrupted? = " + l_result_error, true);
			dcassert(0);
			// the database is corrupted?
			// messageF(STRING_F(DB_OPEN_FAILED_REPAIR, getNameLower() % Text::toUtf8(ret.ToString()) % APPNAME), false, false);
			// repair(stepF, messageF);
			// try it again
			//ret = leveldb::DB::Open(options, l_pdb_name, &db);
		}
	}
	return l_status.ok();
}
//========================================================================================================
bool CFlyLevelDB::get_value(const void* p_key, size_t p_key_len, bool p_cache, string& p_result)
{
	dcassert(m_db);
	if (m_db)
	{
		const leveldb::Slice l_key((const char*)p_key, p_key_len);
		const auto l_status = m_db->Get(m_readoptions, l_key, &p_result);
		if (!(l_status.ok() || l_status.IsNotFound()))
		{
			const auto l_message = l_status.ToString();
			LogManager::getInstance()->message(l_message, true);
		}
		dcassert(l_status.ok() || l_status.IsNotFound());
		return l_status.ok() || l_status.IsNotFound();
	}
	else
	{
		return false;
	}
}
//========================================================================================================
bool CFlyLevelDB::set_value(const void* p_key, size_t p_key_len, const void* p_val, size_t p_val_len)
{
	dcassert(m_db);
	if (m_db)
	{
		// Lock l (m_leveldb_cs);
		const leveldb::Slice l_key((const char*)p_key, p_key_len);
		const leveldb::Slice l_val((const char*)p_val, p_val_len);
		const auto l_status = m_db->Put(m_writeoptions, l_key, l_val);
		if (!l_status.ok())
		{
			const auto l_message = l_status.ToString();
			LogManager::getInstance()->message(l_message, true);
		}
		return l_status.ok();
	}
	else
	{
		return false;
	}
}
//========================================================================================================
uint32_t CFlyLevelDB::set_bit(const TTHValue& p_tth, uint32_t p_mask)
{
	string l_value;
	if (get_value(p_tth, true, l_value))
	{
		uint32_t l_mask = Util::toInt(l_value);
		l_mask |= p_mask;
		if (set_value(p_tth, Util::toString(l_mask)))
		{
			return l_mask;
		}
	}
	dcassert(0);
	return 0;
}
//========================================================================================================
#endif // FLYLINKDC_USE_LEVELDB
