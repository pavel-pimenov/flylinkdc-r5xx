//-----------------------------------------------------------------------------
//(c) 2007-2018 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#include "stdinc.h"
#include <Shellapi.h>

#include "SettingsManager.h"
#include "LogManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "ConnectionManager.h"
#include "CompatibilityManager.h"
#include "../FlyFeatures/flyServer.h"
#include <boost/algorithm/string.hpp>
#include "FinishedManager.h"

#include "libtorrent/read_resume_data.hpp"

using sqlite3x::database_error;
using sqlite3x::sqlite3_transaction;
using sqlite3x::sqlite3_reader;

//bool g_DisableUserStat = false;
bool g_DisableSQLJournal    = false;
bool g_UseWALJournal        = false;
bool g_EnableSQLtrace       = false; // http://www.sqlite.org/c3ref/profile.html
bool g_UseSynchronousOff    = false;
int g_DisableSQLtrace      = 0;
int64_t g_SQLiteDBSizeFree = 0;
int64_t g_TTHLevelDBSize = 0;
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
int64_t g_IPCacheLevelDBSize = 0;
#endif
int64_t g_SQLiteDBSize = 0;

int32_t CFlylinkDBManager::g_count_queue_source = 0;
int32_t CFlylinkDBManager::g_count_queue_files = 0;

boost::unordered_map<TTHValue, TigerTree> CFlylinkDBManager::g_tiger_tree_cache;
FastCriticalSection CFlylinkDBManager::g_tth_cache_cs;
unsigned CFlylinkDBManager::g_tth_cache_limit = 500;

FastCriticalSection  CFlylinkDBManager::g_resume_torrents_cs;
std::unordered_set<libtorrent::sha1_hash> CFlylinkDBManager::g_resume_torrents;

FastCriticalSection  CFlylinkDBManager::g_delete_torrents_cs;
std::unordered_set<libtorrent::sha1_hash> CFlylinkDBManager::g_delete_torrents;

const char* g_db_file_names[] = {"FlylinkDC.sqlite",
                                 "FlylinkDC_log.sqlite",
                                 "FlylinkDC_mediainfo.sqlite",
                                 "FlylinkDC_stat.sqlite",
                                 "FlylinkDC_locations.sqlite",
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
                                 "FlylinkDC_antivirus.sqlite",
#endif
                                 "FlylinkDC_transfers.sqlite",
                                 "FlylinkDC_user.sqlite",
                                 "FlylinkDC_queue.sqlite"
                                };

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
	Sleep(500);
	LogManager::message("SQLite database is locked. try: " + Util::toString(p_tryes), true);
	if (p_tryes && p_tryes % 5 == 0)
	{
		const string l_message = STRING(DATA_BASE_LOCKED_STRING);
		static int g_MessageBox = 0; // TODO - fix copy-paste
		CFlyBusy l_busy(g_MessageBox);
		if (g_MessageBox <= 1)
		{
			MessageBox(NULL, Text::toT(l_message).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
		}
	}
	return 1;
}
//========================================================================================================
static void gf_trace_callback(void* p_udp, const char* p_sql)
{
	if (g_DisableSQLtrace == 0)
	{
		if (BOOLSETTING(LOG_SQLITE_TRACE) || g_EnableSQLtrace)
		{
			StringMap params;
			params["sql"] = p_sql;
			params["thread_id"] = Util::toString(::GetCurrentThreadId());
			LOG_FORCE_FILE(TRACE_SQLITE, params); // Всегда в файл
		}
	}
}
//========================================================================================================
//static void profile_callback( void* p_udp, const char* p_sql, sqlite3_uint64 p_time)
//{
//	const string l_log = "profile_callback - " + string(p_sql) + " time = "+ Util::toString(p_time);
//	LogManager::message(l_log,true);
//}
//========================================================================================================
void CFlylinkDBManager::pragma_executor(const char* p_pragma)
{
	static const char* l_db_name[] = {"main", "media_db",
	                                  "stat_db", "location_db", "user_db"
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	                                  , "antivirus_db"
#endif
	                                 };
	for (int i = 0; i < _countof(l_db_name); ++i)
	{
		string l_sql = "pragma ";
		l_sql += l_db_name[i];
		l_sql += '.';
		l_sql += p_pragma;
		l_sql += ';';
		m_flySQLiteDB.executenonquery(l_sql);
	}
}
//========================================================================================================
bool CFlylinkDBManager::safeAlter(const char* p_sql, bool p_is_all_log /*= false */)
{
	try
	{
		m_flySQLiteDB.executenonquery(p_sql);
		return true;
	}
	catch (const database_error& e)
	{
		if (p_is_all_log == true || e.getError().find("duplicate column name:") == string::npos) // Логируем только неизвестные ошибки
		{
			LogManager::message("safeAlter: " + e.getError(), true);
		}
	}
	return false;
}
//========================================================================================================
string CFlylinkDBManager::get_db_size_info()
{
	auto getFileSize = [](const ::tstring & p_file_name) -> int64_t
	{
		int64_t l_size = 0;
		int64_t l_outFileTime = 0;
		bool l_is_link = false;
		File::isExist(p_file_name, l_size, l_outFileTime, l_is_link);
		return l_size;
	};
	string l_message;
	const auto l_path = Util::getConfigPath();
	dcassert(!l_path.empty());
	if (l_path.size() > 2)
	{
		g_SQLiteDBSize = 0;
		const char* l_rnrn = "\r\n";
		l_message = "Database locations:\r\n";
		for (int i = 0; i < _countof(g_db_file_names); ++i)
		{
			l_message += "  * ";
			l_message += l_path;
			l_message += g_db_file_names[i];
			const auto l_size = getFileSize(Text::toT(l_path) + Text::toT(g_db_file_names[i]));
			g_SQLiteDBSize += l_size;
			l_message += " (" + Util::formatBytes(l_size) + ")";
			l_message += l_rnrn;
		}
		
		char l_disk[3] = { 0 };
		l_disk[0] = l_path[0];
		l_disk[1] = l_path[1];
		g_SQLiteDBSizeFree = 0;
		if (GetDiskFreeSpaceExA(l_disk, (PULARGE_INTEGER)&g_SQLiteDBSizeFree, NULL, NULL))
		{
			l_message += l_rnrn;
			l_message += "(Disk: " + string(l_disk) + "  Free space:" + Util::formatBytes(g_SQLiteDBSizeFree) + " )";
			l_message += l_rnrn;
		}
		else
		{
			dcassert(0);
		}
	}
	return l_message;
}
//========================================================================================================
void CFlylinkDBManager::errorDB(const string& p_txt)
{
	string l_message = p_txt + "\r\n" + STRING(DATA_BASE_ERROR_STRING);
	const string l_error = "CID: " + ClientManager::getMyCID().toBase32() + " CFlylinkDBManager::errorDB. p_txt = " + p_txt;
	const char* l_rnrn = "\r\n";
	l_message += l_rnrn;
	l_message += l_rnrn;
	l_message += get_db_size_info();
	bool l_is_force_exit = false;
	tstring l_russian_error;
	bool l_is_db_malformed = p_txt.find("database disk image is malformed") != string::npos;
	l_is_db_malformed |= p_txt.find("disk I/O error") != string::npos;
	
	bool l_is_db_error_open  = p_txt.find("unable to open database") != string::npos;
	bool l_is_db_ro  = p_txt.find("attempt to write a readonly database") != string::npos;
	
	const tstring l_footer = _T("Если это не поможет пишите подробности на pavel.pimenov@gmail.com или ppa74@ya.ru\r\n")
	                         _T("помогу разобраться с ошибкой и исправить код флайлинка так,\r\n")
	                         _T("чтобы аналогичной проблемы не возникало у других пользователей.\r\n")
	                         _T("скопировать сообщение с этого окна можно нажатием клавишь Ctrl+C\r\n")
	                         _T("Спасибо за понимание.\r\n\r\n");
	                         
	if (l_is_db_ro)
	{
		l_message += l_rnrn;
		l_message += l_rnrn;
		l_russian_error +=
		    _T("База данных находится в каталоге защищенном от записи, или под защитой UAC\r\n")
		    _T("Варианты исправления ситуации:\r\n")
		    _T(" 1. Установите программу в каталог отличный от C:\\Programm Files*\r\n")
		    _T("    например D:\\FlylinkDC\r\n")
		    _T(" 2. Удалите каталог C:\\Programm Files\\FlylinkDC++\\Settings\r\n")
		    _T("    после перезапуска программа создаст настройки в своем профиле\r\n")
		    _T("    где UAC не будет мешать работе\r\n");
		l_russian_error += l_footer;
		l_is_force_exit = true;
	}
	if (l_is_db_malformed || l_is_db_error_open)   // TODO - пополнять эти ошибки из автоапдейта?
	{
		l_message += l_rnrn;
		l_message += l_rnrn;
		l_message += "Try backup and delete all database files! (del FlylinkDC*.sqlite)";
		l_message += l_rnrn;
		l_message += l_rnrn;
		l_russian_error +=
		    _T("База данных разрушена!\r\n")
		    _T("возможно компьютер выключили не корректно\r\n")
		    _T("или он завис с синим экраном\r\n\r\n")
		    _T("Не волнуйтесь... Для исправления ситуации выполните следующее:\r\n")
		    _T(" 1. Закройте FlylinkDC++\r\n")
		    _T(" 2. Удалите указанные выше файлы (*.sqlite)\r\n")
		    _T("    (если желаете провести детальный анализ причин разрушения\r\n")
		    _T("    предварительно сохраните файлы в другом месте\r\n")
		    _T("    упакуйте их архиватором и вышлите на почту ppa74@ya.ru\r\n")
		    _T(" 4. Запустите FlylnkDC++ повторно\r\n")
		    _T(" 5. Программа автоматически создаст новые версии файлов,\r\n")
		    _T("    и в фоновом режиме выполнит повторное хеширование шары.\r\n");
		l_russian_error += l_footer;
		l_is_force_exit = true;
	}
	if (p_txt.find("out of memory") != string::npos)
	{
		ShareManager::tryFixBadAlloc();
	}
	if (p_txt.find("database or disk is full") != string::npos)
	{
		l_message += l_rnrn;
		l_message += l_rnrn;
		l_russian_error = _T("У вас переполнился жесткий диск!\r\n")
		                  _T("удалите лишние данные и освободите место для работы приложения!\r\n");
		l_russian_error += l_footer;
		l_is_force_exit = true;
	}
	Util::setRegistryValueString(FLYLINKDC_REGISTRY_SQLITE_ERROR, Text::toT(l_error));
	LogManager::message(p_txt, true); // Всегда логируем в файл (т.к. база может быть битой)
	static int g_MessageBox = 0; // TODO - fix copy-paste
	{
		CFlyBusy l_busy(g_MessageBox);
		if (g_MessageBox <= 1)
		{
			if (p_txt.find("not an error") == string::npos)
			{
				MessageBox(NULL, (l_russian_error + Text::toT(l_message)).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
			}
		}
	}
	bool l_is_send = CFlyServerJSON::pushError(16, l_error);
	if (l_is_force_exit)
	{
		static bool g_is_first = false;
		if (g_is_first == false)
		{
			g_is_first = true;
			tstring l_body = l_russian_error + Text::toT(l_message);
			boost::replace_all(l_body, " ", "%20");
			boost::replace_all(l_body, "\r", "%0D");
			boost::replace_all(l_body, "\n", "%0A");
			tstring l_shell = _T("mailto:pavel.pimenov@gmail.com?subject=FlylinkDC++ bug-report&body=") + l_body;
			::ShellExecute(0, _T("Open"), l_shell.c_str(), _T(""), _T(""), SW_NORMAL);
			// exit(1); // exit нелья - падаем  https://drdump.com/Bug.aspx?ProblemID=261050
			//
		}
	}
	if (!l_is_send)
	{
		// TODO - скинуть ошибку в файл и не грузить crash-server логическими ошибками
		// https://www.crash-server.com/Problem.aspx?ClientID=guest&ProblemID=51924
		//throw database_error(l_error.c_str());
	}
}
//========================================================================================================
CFlylinkDBManager::CFlylinkDBManager()
{
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	m_virus_cs = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#endif
	CFlyLock(m_cs);
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
			errorDB("SQLite - CFlylinkDBManager: error GetCurrentDirectory " + Util::translateError());
		}
		else
		{
			if (SetCurrentDirectory(Text::toT(Util::getConfigPath()).c_str()))
			{
				auto l_status = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
				if (l_status != SQLITE_OK)
				{
					LogManager::message("[Error] sqlite3_config(SQLITE_CONFIG_SERIALIZED) = " + Util::toString(l_status), true);
				}
				dcassert(l_status == SQLITE_OK);
				l_status = sqlite3_initialize();
				if (l_status != SQLITE_OK)
				{
					LogManager::message("[Error] sqlite3_initialize = " + Util::toString(l_status), true);
				}
				dcassert(l_status == SQLITE_OK);
#if 0
				for (int j = 0; j < 2; ++j)
				{
					{
						sqlite3_connection l_DB;
						File::deleteFile("users.sqlite");
						File::copyFile("users-orig.sqlite", "users.sqlite");
						l_DB.open("users.sqlite");
						std::unique_ptr<sqlite3_command> l_load_all(new sqlite3_command(l_DB, "select nick from userinfo"));
						sqlite3_reader l_q = l_load_all.get()->executereader();
						std::vector<string> l_nick;
						l_nick.reserve(160000);
						while (l_q.read())
						{
							l_nick.push_back(l_q.getstring(0));
						}
						CFlySQLCommand l_sql;
						sqlite3_transaction l_trans(l_DB);
						{
							CFlyLockProfiler l_log;
							for (auto i = l_nick.cbegin(); i != l_nick.cend(); ++i)
							{
								switch (j)
								{
									case 1:
										l_sql.init(l_DB,
										           "INSERT OR REPLACE INTO userinfo (nick, last_updated, ip_address, share, description, tag, connection, email) VALUES(?,DATETIME('now'),'ip','share','description','tag','connection','email')");
										break;
									case 0:
										l_sql.init(l_DB,
										           "UPDATE userinfo set last_updated = DATETIME('now'), ip_address == 'ip', share = 'share', description = 'description', tag = 'tag', connection = 'connection', email = 'email' where nick = ?");
										break;
								}
								l_sql->bind(1, *i, SQLITE_STATIC);
								l_sql->executenonquery();
							}
							l_trans.commit();
							l_log.log("D:\\time.txt", j);
						}
					}
				}
#endif
				
				m_flySQLiteDB.open("FlylinkDC.sqlite");
				sqlite3_busy_handler(m_flySQLiteDB.get_db(), gf_busy_handler, this);
				// m_flySQLiteDB.setbusytimeout(1000);
				// TODO - sqlite3_busy_handler
				// Пример реализации обработчика -
				// https://github.com/iso9660/linux-sdk/blob/d819f98a72776fced31131b1bc22a4bcb4c492bb/SDKLinux/LFC/Data/sqlite3db.cpp
				// https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=17660
				if (BOOLSETTING(LOG_SQLITE_TRACE) || g_EnableSQLtrace)
				{
					sqlite3_trace(m_flySQLiteDB.get_db(), gf_trace_callback, NULL);
					// sqlite3_profile(m_flySQLiteDB.get_db(), profile_callback, NULL);
				}
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_log.sqlite' as log_db");
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_mediainfo.sqlite' as media_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_stat.sqlite' as stat_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_locations.sqlite' as location_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_user.sqlite' as user_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_transfers.sqlite' as transfer_db");
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_queue.sqlite' as queue_db");
				
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
				m_flySQLiteDB.executenonquery("attach database 'FlylinkDC_antivirus.sqlite' as antivirus_db");
#endif
				
#ifdef FLYLINKDC_USE_LEVELDB
				// Тут обязательно полный путь. иначе при смене рабочего каталога levelDB не сомжет открыть базу.
				const string l_full_path_level_db = Text::fromUtf8(Util::getConfigPath() + "tth-history.leveldb");
				
				bool l_is_destroy = false;
				m_TTHLevelDB.open_level_db(l_full_path_level_db, l_is_destroy);
//				g_TTHLevelDBSize = File::calcFilesSize(l_full_path_level_db, "\\*.*"); // TODO убрать сканирование второе
				/*
				#ifdef _DEBUG
				                for (int h = 0; h < 100000; ++h)
				                {
				                    unsigned char aData[39];
				                    *(int32_t*)aData = Util::rand();
				                    for (int j = 0; j < 38; ++j)
				                    {
				                        aData[j+3] = j+h;
				                    }
				                    TTHValue l_tth(aData);
				                    m_TTHLevelDB.set_value(l_tth, "x");
				                }
				#endif
				*/
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
				l_full_path_level_db = Util::getConfigPath() + "ip-history.leveldb";
				m_IPCacheLevelDB.open_level_db(l_full_path_level_db);
				g_IPCacheLevelDBSize = File::calcFilesSize(l_full_path_level_db, "\\*.*");
#endif
#endif // FLYLINKDC_USE_LEVELDB
				SetCurrentDirectory(l_dir_buffer);
				if (l_is_destroy)
				{
					convert_tth_history();
				}
			}
			else
			{
				errorDB("SQLite - CFlylinkDBManager: error SetCurrentDirectory l_db_path = " + Util::getConfigPath()
				        + " Error: " +  Util::translateError());
			}
		}
		
		LogManager::message(get_db_size_info(), true);
		const string l_thread_type = "sqlite3_threadsafe() = " + Util::toString(sqlite3_threadsafe());
		LogManager::message(l_thread_type, true);
		dcassert(sqlite3_threadsafe() >= 1);
		
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
		}
		pragma_executor("temp_store=MEMORY");
		
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
		// Log_DB
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS log_db.fly_log(id integer PRIMARY KEY AUTOINCREMENT,"
		    "sdate text not null, type integer not null,body text, hub text, nick text,"
		    "ip text, file text, source text, target text,fsize int64,fchunk int64,extra text,userCID text);");
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
		    
		// MEDIA_DB
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS media_db.fly_server_cache(tth char(24) PRIMARY KEY NOT NULL, fly_audio text,fly_audio_br text,fly_video text,fly_xy text);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS media_db.fly_media(" // id integer primary key autoincrement, TODO - id в этой табличке нам не нужна...
		    "tth_id integer not null,"
		    "stream_type integer not null,"
		    "channel integer not null,"
		    "param text not null,"
		    "value text not null);");
		// TODO - потестить как sqlite выбирает план
		// l_flySQLiteDB_Mediainfo.executenonquery("CREATE INDEX IF NOT EXISTS media_db.i_fly_media_tth_id ON fly_media(tth_id);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS media_db.iu_fly_media_param ON fly_media(tth_id,stream_type,channel,param);");
		
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_revision(rev integer NOT NULL);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_file(dic_path integer not null,"
		    "name text not null,"
		    "size int64 not null,"
		    "stamp int64 not null,"
		    "tth_id int64 not null,"
		    "hit int64 default 0,"
		    "stamp_share int64 default 0,"
		    "bitrate integer,"
		    "ftype integer default -1,"
		    "media_x integer,"
		    "media_y integer,"
		    "media_video text,"
		    "media_audio text"
		    ");");
		    
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_file_name ON fly_file(dic_path,name);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS i_fly_file_tth_id ON fly_file(tth_id);");
		//[-] TODO потестить на большой шаре
		// m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS i_fly_file_dic_path ON fly_file(dic_path);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_hash_block(tth_id integer PRIMARY KEY NOT NULL,"
		    "tiger_tree blob,file_size int64 not null,block_size int64,tth char(24) NOT NULL);");
		safeAlter("ALTER TABLE fly_hash_block add column tth char(24)");
		////////////////////////////////////////////////////////////////////////////////////////////////
		convert_fly_hash_blockL();
		////////////////////////////////////////////////////////////////////////////////////////////////
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_path("
		                              "id integer PRIMARY KEY AUTOINCREMENT NOT NULL, name text NOT NULL UNIQUE);");
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_dic("
		                              "id integer PRIMARY KEY AUTOINCREMENT NOT NULL,dic integer NOT NULL, name text NOT NULL);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_dic_name ON fly_dic(name,dic);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_ratio(id integer PRIMARY KEY AUTOINCREMENT NOT NULL,"
		    "dic_ip integer not null,dic_nick integer not null, dic_hub integer not null,"
		    "upload int64 default 0,download int64 default 0);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_ratio ON fly_ratio(dic_nick,dic_hub,dic_ip);");
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		m_flySQLiteDB.executenonquery("CREATE VIEW IF NOT EXISTS v_fly_ratio AS "
		                              "SELECT fly_ratio.id id, fly_ratio.upload upload,"
		                              "fly_ratio.download download,"
		                              "userip.name userip,"
		                              "nick.name nick,"
		                              "hub.name hub "
		                              "FROM fly_ratio "
		                              "INNER JOIN fly_dic userip ON fly_ratio.dic_ip = userip.id "
		                              "INNER JOIN fly_dic nick ON fly_ratio.dic_nick = nick.id "
		                              "INNER JOIN fly_dic hub ON fly_ratio.dic_hub = hub.id");
		                              
		m_flySQLiteDB.executenonquery("CREATE VIEW IF NOT EXISTS v_fly_ratio_all AS "
		                              "SELECT fly_ratio.id id, fly_ratio.upload upload,"
		                              "fly_ratio.download download,"
		                              "nick.name nick,"
		                              "hub.name hub,"
		                              "fly_ratio.dic_ip dic_ip,"
		                              "fly_ratio.dic_nick dic_nick,"
		                              "fly_ratio.dic_hub dic_hub "
		                              "FROM fly_ratio "
		                              "INNER JOIN fly_dic nick ON fly_ratio.dic_nick = nick.id "
		                              "INNER JOIN fly_dic hub ON fly_ratio.dic_hub = hub.id");
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		m_flySQLiteDB.executenonquery("CREATE VIEW IF NOT EXISTS v_fly_dup_file AS "
		                              "SELECT tth_id,count(*) cnt_dup,"
		                              "max((select name from fly_path where id = dic_path)) path_1,"
		                              "min((select name from fly_path where id = dic_path)) path_2,"
		                              "max(name) name_max,"
		                              "min(name) name_min "
		                              "FROM fly_file group by tth_id having count(*) > 1");
		                              
		const int l_rev = m_flySQLiteDB.executeint("select max(rev) from fly_revision");
		const int l_db_user_version = m_flySQLiteDB.executeint("PRAGMA user_version");
		if (l_rev < 322)
		{
			safeAlter("ALTER TABLE fly_file add column hit int64 default 0");
			safeAlter("ALTER TABLE fly_file add column stamp_share int64 default 0");
			safeAlter("ALTER TABLE fly_file add column bitrate integer");
			m_flySQLiteDB.executenonquery("delete from fly_file where name like '%.mp3' and (bitrate=0 or bitrate is null)");
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
			m_flySQLiteDB.executenonquery("delete from fly_file where tth_id=0");
		}
		if (l_rev < 365)
		{
			m_flySQLiteDB.executenonquery("update fly_file set ftype=6 where name like '%.mp4' or name like '%.fly'");
		}
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_queue(id integer PRIMARY KEY NOT NULL,"
		    "Target text not null,"  // убрал UNIQUE для исключения перестройки индекса (уникальность поддерживается мапой)
		    "Size int64 not null,"
		    "Priority integer not null,"
		    "Sections text,"
		    "Added int64 not null,"
		    "TTH char(24) not null,"
		    "TempTarget text,"
		    "AutoPriority integer not null,"
		    "MaxSegments integer);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_queue_source(id integer PRIMARY KEY AUTOINCREMENT NOT NULL,"
		    "fly_queue_id integer not null,CID char(24) not null,Nick text,dic_hub integer);");
		safeAlter("ALTER TABLE fly_queue_source add column dic_hub integer");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS i_fly_queue_source_id ON fly_queue_source(fly_queue_id);");
		
//		if (l_rev <= 379)
		{
			m_flySQLiteDB.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_ignore(nick text PRIMARY KEY NOT NULL);");
		}
//     if (l_rev <= 381)
		{
			m_flySQLiteDB.executenonquery(
			    "CREATE TABLE IF NOT EXISTS fly_registry(segment integer not null, key text not null,val_str text, val_number int64,tick_count int not null);");
			try
			{
				m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_registry_key ON fly_registry(segment,key);");
			}
			catch (const database_error& e)
			{
				CFlyServerJSON::pushError(3, "Error CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_registry_key ON fly_registry(segment,key); Error = " + e.getError());
				m_flySQLiteDB.executenonquery("delete from fly_registry");
				m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_registry_key ON fly_registry(segment,key);");
			}
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
		    "CREATE TABLE IF NOT EXISTS location_db.fly_p2pguard_ip(start_ip integer not null,stop_ip integer not null,note text,type integer);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_ip ON fly_p2pguard_ip(start_ip);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_p2pguard_note ON fly_p2pguard_ip(note);");
		safeAlter("ALTER TABLE location_db.fly_p2pguard_ip add column type integer");
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS location_db.fly_country_ip(start_ip integer not null,stop_ip integer not null,country text,flag_index integer);");
		safeAlter("ALTER TABLE location_db.fly_country_ip add column country text");
		
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS location_db.i_fly_country_ip ON fly_country_ip(start_ip);");
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
		                              "location_db.i_fly_location_ip ON fly_location_ip(start_ip);");
		                              
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_identity(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,hub text not null,key text not null, value text not null,"
		    "count_get integer, count_set integer,last_time_get text not null, last_time_set text not null);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS "
		                              "stat_db.iu_fly_identity ON fly_identity(hub,key,value);");
		                              
#endif // FLYLINKDC_USE_GATHER_IDENTITY_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_statistic(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,stat_value_json text not null,stat_time int64, flush_time int64, type text);");
		safeAlter("ALTER TABLE stat_db.fly_statistic add column type text");
		
		// Таблицы - мертвые
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_last_ip(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
		                              "dic_nick integer not null, dic_hub integer not null,dic_ip integer not null);");
		if (!safeAlter("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_last_ip ON fly_last_ip(dic_nick,dic_hub);"))
		{
			safeAlter("delete from fly_last_ip where rowid not in (select max(rowid) from fly_last_ip group by dic_nick,dic_hub)");
			CFlyServerJSON::pushError(7, "error CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_last_ip ON fly_last_ip(dic_nick,dic_hub)");
		}
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_last_ip_nick_hub("
		                              "nick text not null, dic_hub integer not null,ip text);");
		if (!safeAlter("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_last_ip_nick_hub ON fly_last_ip_nick_hub(nick,dic_hub);"))
		{
			safeAlter("delete from fly_last_ip_nick_hub where rowid not in (select max(rowid) from fly_last_ip_nick_hub group by nick,dic_hub)");
			CFlyServerJSON::pushError(8, "error CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_last_ip_nick_hub ON fly_last_ip_nick_hub(nick,dic_hub)");
		}
		// Она не используются в версиях r502 но для отката назад нужны
		
#ifdef FLYLINKDC_USE_COLLECT_STAT
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_dc_command_log(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL"
		    ",hub text not null,command text not null,server text,port text, sender_nick text, counter int64, last_time text not null);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS stat_db.iu_fly_dc_command_log ON fly_dc_command_log(hub,command);");
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS stat_db.fly_event(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL "
		    ",type text not null, event_key text not null, event_value text, ip text, port text, hub text, tth char(39), event_time text);");
#endif
		safeAlter("ALTER TABLE fly_queue add column Sections text");
		if (l_rev <= 500)
		{
			// [!] brain-ripper
			// have to delete follow columns, but SQLite doesn't support DROP command.
			// TODO some workaround.
			/*
			safeAlter("ALTER TABLE fly_queue drop column FreeBlocks");
			safeAlter("ALTER TABLE fly_queue drop column VerifiedParts");
			safeAlter("ALTER TABLE fly_queue drop column Downloaded");
			*/
		}
		safeAlter("ALTER TABLE fly_hash_block add column block_size int64");
		if (l_rev < 403 || l_rev == 500)
		{
			safeAlter("ALTER TABLE fly_file add column media_x integer");
			safeAlter("ALTER TABLE fly_file add column media_y integer");
			safeAlter("ALTER TABLE fly_file add column media_video text");
			if (safeAlter("ALTER TABLE fly_file add column media_audio text"))
			{
				// получилось добавить колонку - значит стартанули первый раз - удалим все записи для перехеша с помощью либы
				const string l_where = "delete from fly_file where " + g_fly_server_config.DBDelete();
				m_flySQLiteDB.executenonquery(l_where);
			}
		}
		
		if (l_rev < VERSION_NUM)
		{
			m_flySQLiteDB.executenonquery("insert into fly_revision(rev) values(" A_VERSION_NUM_STR ");");
		}
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS user_db.user_info("
		                              "nick text not null, dic_hub integer not null, last_ip integer, message_count integer);");
		m_flySQLiteDB.executenonquery("DROP INDEX IF EXISTS user_db.iu_user_info;"); //старый индекс был (nick,dic_hub)
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS user_db.iu_user_info_hub_nick ON user_info(dic_hub,nick);");
		
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
		                              "tth char(39),path text not null,nick text, hub text,size int64 not null,speed int,ip text, actual int64);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_day_type ON fly_transfer_file(day,type);");
		// TODO - сделать позже если будет тормозить
		// m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.i_fly_transfer_file_tth ON fly_transfer_file(tth);");
		
		safeAlter("ALTER TABLE transfer_db.fly_transfer_file add column actual int64");
		
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS transfer_db.fly_transfer_file_torrent("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,type int not null,day int64 not null,stamp int64 not null,"
		                              "sha1 char(20),path text,size int64 not null);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS transfer_db.fly_transfer_file_torrentday_type ON fly_transfer_file_torrent(day,type);");
		
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS queue_db.fly_queue_torrent("
		                              "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,day int64 not null,stamp int64 not null,sha1 char(20) NOT NULL,resume blob, magnet string,name string);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS queue_db.iu_fly_queue_torrent_sha1 ON fly_queue_torrent(sha1);");
		
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS antivirus_db.fly_suspect_user("
		                              "nick text not null,ip4 int not null,share int64 not null,virus_path text);");
		safeAlter("ALTER TABLE antivirus_db.fly_suspect_user add column virus_path text");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS antivirus_db.iu_suspect_user_nick ON fly_suspect_user(nick,ip4);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS antivirus_db.i_suspect_user_ip4 ON fly_suspect_user(ip4);");
		m_flySQLiteDB.executenonquery("CREATE INDEX IF NOT EXISTS antivirus_db.i_suspect_user_share ON fly_suspect_user(share);");
		
#endif
		if (l_db_user_version < 1)
		{
			// fix https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/18
			m_flySQLiteDB.executenonquery("update fly_file set ftype=1 where name like '%.wv'");
			m_flySQLiteDB.executenonquery("PRAGMA user_version=1");
		}
		if (l_db_user_version < 2)
		{
			// Удаляю уже на уровне конвертора файла.
			// m_flySQLiteDB.executenonquery("delete from location_db.fly_p2pguard_ip where note like '%VimpelCom%'");
			m_flySQLiteDB.executenonquery("PRAGMA user_version=2");
		}
		if (l_db_user_version < 3)
		{
			if (safeAlter("insert into fly_queue_source(fly_queue_id,CID,Nick) select id,CID,nick from fly_queue where nick is not null", true))
			{
				safeAlter("update fly_queue set nick=null, cid=null, CountSubSource=null where nick is not null", true);
			}
			m_flySQLiteDB.executenonquery("PRAGMA user_version=3");
		}
		if (l_db_user_version < 4)
		{
			safeAlter("delete FROM fly_queue_source where CID = X'000000000000000000000000000000000000000000000000'", true);
			m_flySQLiteDB.executenonquery("PRAGMA user_version=4");
		}
		if (l_db_user_version < 5)
		{
			cleanup_transfer_historgam();
			m_flySQLiteDB.executenonquery("PRAGMA user_version=5");
		}
		
		// TODO - грохнуть поля  fly_queue в и перетащить базу в другие файлы.
		
		/*
		{
		    // Конвертим ip в бинарный формат
		    std::unique_ptr<sqlite3_command> l_src_sql(new sqlite3_command(m_flySQLiteDB,
		                                                                    "select nick,dic_hub,ip from fly_last_ip_nick_hub"));
		    try
		    {
		        sqlite3_reader l_q = l_src_sql->executereader();
		        sqlite3_transaction l_trans(m_flySQLiteDB);
		        std::unique_ptr<sqlite3_command> l_trg_sql(new sqlite3_command(m_flySQLiteDB,
		                                                                        "insert or replace into user_db.user_info (nick,dic_hub,last_ip) values(?,?,?)"));
		        while (l_q.read())
		        {
		            boost::system::error_code ec;
		            const auto l_ip = boost::asio::ip::address_v4::from_string(l_q.getstring(2), ec);
		            dcassert(!ec);
		            if (!ec)
		            {
		                l_trg_sql->bind(1, l_q.getstring(0), SQLITE_TRANSIENT);
		                l_trg_sql->bind(2, l_q.getint64(1));
		                l_trg_sql->bind(3, sqlite_int64(l_ip.to_ulong()));
		                l_trg_sql->executenonquery();
		            }
		        }
		        l_trans.commit();
		    }
		    catch (const database_error& e)
		    {
		        // Гасим ошибки БД при конвертации
		        LogManager::message("[SQLite] Error convert user_db.user_info = " + e.getError());
		    }
		}
		*/
		load_all_hub_into_cacheL();
		//safeAlter("ALTER TABLE fly_last_ip_nick_hub add column message_count integer");
		
		/*      {
		            sqlite3_command(m_flySQLiteDB,"delete from fly_queue where size=-1"));
		            m_del_fly_queue->executenonquery();
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
	std::unique_ptr<sqlite3_command> l_load_all_dic(new sqlite3_command(m_flySQLiteDB,
	                                                                    "select id,name from fly_dic where dic=1"));
	sqlite3_reader l_q = l_load_all_dic.get()->executereader();
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
	CFlyFastLock(m_hub_dic_fcs);
#endif
	while (l_q.read())
	{
		const string l_hub_name = l_q.getstring(1);
		const auto l_id = l_q.getint(0);
		const auto& l_res = m_DIC[e_DIC_HUB - 1].insert(std::make_pair(l_hub_name, l_id));
		dcassert(l_res.second == true);
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
		m_HubNameMap[l_id] = l_hub_name;
#endif
	}
}
//========================================================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
void CFlylinkDBManager::merge_mediainfo_ext(const __int64 l_tth_id, const CFlyMediaInfo& p_media, bool p_delete_old_info)
{
	if (p_delete_old_info) // TODO - никогда не работает почему-то
	{
		m_delete_mediainfo.init(m_flySQLiteDB, "delete from media_db.fly_media where tth_id=? ");
		m_delete_mediainfo->bind(1, l_tth_id);
		m_delete_mediainfo->executenonquery();
	}
	if (p_media.isMediaAttrExists())
	{
		m_insert_mediainfo.init(m_flySQLiteDB,
		                        "insert or replace into media_db.fly_media "
		                        "(tth_id,stream_type,channel,param,value) values (?,?,?,?,?);");
		sqlite3_command* l_sql = m_insert_mediainfo.get_sql();
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
bool CFlylinkDBManager::load_media_info(const TTHValue& p_tth, CFlyMediaInfo& p_media_info, bool p_only_inform)
{
	CFlyLock(m_cs);
	try
	{
		const __int64 l_tth_id = get_tth_idL(p_tth);
		if (l_tth_id)
		{
			// Читаем базовую инфу (TODO - если есть в памяти попробовать забрать из нее)
			m_load_mediainfo_base.init(m_flySQLiteDB,
			                           "select bitrate,media_x,media_y,media_video,media_audio from fly_file where tth_id=? limit 1");
			m_load_mediainfo_base->bind(1, l_tth_id);
			sqlite3_reader l_q_base = m_load_mediainfo_base->executereader();
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
					m_load_mediainfo_ext.init(m_flySQLiteDB,
					                          "select stream_type,channel,param,value from media_db.fly_media "
					                          "where tth_id = ? order by stream_type,channel");
					m_load_mediainfo_ext->bind(1, l_tth_id);
				}
				else
				{
					m_load_mediainfo_ext_only_inform.init(m_flySQLiteDB,
					                                      "select stream_type,channel,param,value from media_db.fly_media "
					                                      "where tth_id = ? and param = 'Infrom' order by stream_type,channel");
					m_load_mediainfo_ext_only_inform->bind(1, l_tth_id);
				}
				sqlite3_reader l_q;
				if (p_only_inform)
					l_q = m_load_mediainfo_ext_only_inform->executereader();
				else
					l_q = m_load_mediainfo_ext->executereader();
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
		errorDB("SQLite - load_media_info: " + e.getError());
	}
	return false;
}

bool CFlylinkDBManager::find_fly_server_cache(const TTHValue& p_tth, CFlyServerCache& p_value)
{
	CFlyLock(m_cs);
	try
	{
		m_select_fly_server_cache.init(m_flySQLiteDB,
		                               "select fly_audio,fly_audio_br,fly_video,fly_xy from media_db.fly_server_cache where tth=?");
		m_select_fly_server_cache->bind(1, p_tth.data, 24, SQLITE_STATIC);
		sqlite3_reader l_q = m_select_fly_server_cache->executereader();
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
	CFlyLock(m_cs);
	try
	{
		m_insert_fly_server_cache.init(m_flySQLiteDB,
		                               "insert or replace into media_db.fly_server_cache (tth,fly_audio,fly_audio_br,fly_video,fly_xy) values(?,?,?,?,?)");
		auto l_sql = m_insert_fly_server_cache.get_sql();
		l_sql->bind(1, p_tth.data, 24, SQLITE_STATIC);
		l_sql->bind(2, p_value.m_audio, SQLITE_STATIC);
		l_sql->bind(3, p_value.m_audio_br, SQLITE_STATIC);
		l_sql->bind(4, p_value.m_video, SQLITE_STATIC);
		l_sql->bind(5, p_value.m_xy, SQLITE_STATIC);
		l_sql->executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_fly_server_cache: " + e.getError());
	}
}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
//========================================================================================================
void CFlylinkDBManager::convert_fly_hash_block_internalL()
{
	m_fly_hash_block_convert_loop.init(m_flySQLiteDB,
	                                   "select id,tth from fly_hash");
	m_fly_hash_block_convert_update.init(m_flySQLiteDB,
	                                     "update fly_hash_block set tth=? where tth_id=?");
	sqlite3_reader l_q = m_fly_hash_block_convert_loop->executereader();
	int l_count_convert_error = 0;
	string l_last_error;
	while (l_q.read())
	{
		vector<uint8_t> l_tth;
		l_q.getblob(1, l_tth);
		dcassert(l_tth.size() == 24);
		if (l_tth.size() == 24)
		{
			m_fly_hash_block_convert_update->bind(1, l_tth.data(), l_tth.size(), SQLITE_STATIC);
			m_fly_hash_block_convert_update->bind(2, l_q.getint64(0));
			try
			{
				m_fly_hash_block_convert_update->executenonquery();
			}
			catch (const database_error& e)
			{
				l_count_convert_error++;
				l_last_error = e.getError();
			}
		}
	}
	if (l_count_convert_error)
	{
		CFlyServerJSON::pushError(2, "Error convert fly_hash_block! count error = " + Util::toString(l_count_convert_error) + ", last Error = " + l_last_error);
	}
}
//========================================================================================================
void CFlylinkDBManager::convert_fly_hash_block_crate_unicque_tthL(CFlyLogFile& p_convert_log)
{
	const char* l_create_uindex = "CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_hash_block_tth ON fly_hash_block(tth)";
	try
	{
		p_convert_log.step(m_flySQLiteDB.executenonquery(l_create_uindex));
	}
	catch (const database_error& e)
	{
		CFlyServerJSON::pushError(3, "Error CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_hash_block_tth ON fly_hash_block(tth). Error = " + e.getError());
		// Удаляем дубли! но я не знаю откуда они могут взяться :(
		{
			p_convert_log.step(m_flySQLiteDB.executenonquery("delete from fly_hash_block where tth is not null and tth_id not in (select max(tth_id) from fly_hash_block where tth is not null group by tth)"));
		}
		p_convert_log.step(m_flySQLiteDB.executenonquery(l_create_uindex));
	}
}
//========================================================================================================
void CFlylinkDBManager::convert_fly_hash_blockL()
{
#define FLYLINKDC_USE_FAST_CONVERT
	CFlyLogFile l_convert_log("[SQLite DB convert]");
	if (is_table_exists("fly_hash"))
	{
#ifdef FLYLINKDC_USE_FAST_CONVERT
		if (!g_UseSynchronousOff)
		{
			m_flySQLiteDB.executenonquery("pragma synchronous=OFF;");
		}
#endif
		clean_fly_hash_blockL();
		{
			try
			{
				l_convert_log.step(m_flySQLiteDB.executenonquery("update fly_hash_block set tth=(select tth from fly_hash fh where fh.id=fly_hash_block.tth_id) where tth is null"));
			}
			catch (const database_error& e)
			{
				if (e.getError().find("UNIQUE ") != string::npos) // Вероятность этого очень маленькая..
				{
					CFlyServerJSON::pushError(1, e.getError()); // TODO __FILE__, __LINE__
					convert_fly_hash_block_internalL(); // Обработаем апдейт по одной записи TODO - если не возникнет ошибок - убрать
				}
				else
				{
					throw;
				}
			}
			l_convert_log.step(m_flySQLiteDB.executenonquery("delete from fly_hash_block where tth is null"));
		}
		convert_fly_hash_block_crate_unicque_tthL(l_convert_log);
		convert_tth_historyL();
		l_convert_log.step(m_flySQLiteDB.executenonquery("drop table fly_hash"));
#ifdef FLYLINKDC_USE_FAST_CONVERT
		if (!g_UseSynchronousOff)
		{
			m_flySQLiteDB.executenonquery("pragma synchronous=FULL;");
		}
#endif
	}
	else
	{
		convert_fly_hash_block_crate_unicque_tthL(l_convert_log);
	}
}

#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
//========================================================================================================
void CFlylinkDBManager::identity_initL(const string& p_hub, const string& p_key, const string& p_value)
{
	try
	{
		m_insert_identity_stat.init(m_flySQLiteDB,
		                            "insert into stat_db.fly_identity (hub,key,value,count_get,count_set,last_time_get,last_time_set) "
		                            "values(?,?,?,0,0,strftime('%d.%m.%Y %H:%M:%S','now','localtime'),strftime('%d.%m.%Y %H:%M:%S','now','localtime'))");
		m_insert_identity_stat->bind(1, p_hub, SQLITE_STATIC);
		m_insert_identity_stat->bind(2, p_key, SQLITE_STATIC);
		m_insert_identity_stat->bind(3, p_value, SQLITE_STATIC);
		m_insert_identity_stat->executenonquery();
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
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_update_identity_stat_set.init(m_flySQLiteDB,
		                                "update stat_db.fly_identity set count_set = count_set+1, last_time_set = strftime('%d.%m.%Y %H:%M:%S','now','localtime') "
		                                "where hub = ? and key=? and value =?");
		m_update_identity_stat_set->bind(1, p_hub, SQLITE_STATIC);
		m_update_identity_stat_set->bind(2, p_key, SQLITE_STATIC);
		m_update_identity_stat_set->bind(3, p_value, SQLITE_STATIC);
		m_update_identity_stat_set->executenonquery();
		if (m_update_identity_stat_set.sqlite3_changes() == 0)
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
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_update_identity_stat_get.init(m_flySQLiteDB,
		                                "update stat_db.fly_identity set count_get = count_get+1, last_time_get = strftime('%d.%m.%Y %H:%M:%S','now','localtime') "
		                                "where hub = ? and key=? and value =?"));
		m_update_identity_stat_get->bind(1, p_hub, SQLITE_STATIC);
		m_update_identity_stat_get->bind(2, p_key, SQLITE_STATIC);
		m_update_identity_stat_get->bind(3, p_value, SQLITE_STATIC);
		m_update_identity_stat_get->executenonquery();
		if (m_update_identity_stat_get.sqlite3_changes() == 0)
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
	CFlyLock(m_cs);
	try
	{
		m_insert_event_stat.init(m_flySQLiteDB,
		                         "insert into stat_db.fly_event(type,event_key,event_value,ip,port,hub,tth,event_time) values(?,?,?,?,?,?,?,strftime('%d.%m.%Y %H:%M:%S','now','localtime'))");
		m_insert_event_stat->bind(1, p_event_type, SQLITE_STATIC);
		m_insert_event_stat->bind(2, p_event_key, SQLITE_STATIC);
		m_insert_event_stat->bind(3, p_event_value, SQLITE_STATIC);
		m_insert_event_stat->bind(4, p_ip, SQLITE_STATIC);
		m_insert_event_stat->bind(5, p_port, SQLITE_STATIC);
		m_insert_event_stat->bind(6, p_hub, SQLITE_STATIC);
		m_insert_event_stat->bind(7, p_tth, SQLITE_STATIC);
		m_insert_event_stat->executenonquery();
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
		CFlyLock(m_cs);
		try
		{
			__int64 l_counter = 0;
			{
				m_select_statistic_dc_command.init(m_flySQLiteDB,
				                                   "select counter from stat_db.fly_dc_command_log where hub = ? and command = ?");
				m_select_statistic_dc_command->bind(1, p_hub, SQLITE_STATIC);
				m_select_statistic_dc_command->bind(2, p_command, SQLITE_STATIC);
				sqlite3_reader l_q = m_select_statistic_dc_command.get()->executereader();
				while (l_q.read())
				{
					l_counter = l_q.getint64(0);
				}
			}
			m_insert_statistic_dc_command.init(m_flySQLiteDB,
			                                   "insert or replace into stat_db.fly_dc_command_log(hub, command, server, port, sender_nick, counter, last_time) values(?,?,?,?,?,?,strftime('%d.%m.%Y %H:%M:%S','now','localtime'))");
			m_insert_statistic_dc_command->bind(1, p_hub, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(2, p_command, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(3, p_server, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(4, p_port, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(5, p_sender_nick, SQLITE_STATIC);
			m_insert_statistic_dc_command->bind(6, l_counter + 1);
			m_insert_statistic_dc_command->executenonquery();
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
void CFlylinkDBManager::push_json_statistic(const std::string& p_value, const string& p_type, bool p_is_stat_server)
{
	if (!p_value.empty() && BOOLSETTING(USE_FLY_SERVER_STATICTICS_SEND))
	{
		CFlyLock(m_cs);
		try
		{
			m_insert_statistic_json.init(m_flySQLiteDB, "insert into stat_db.fly_statistic(stat_value_json,stat_time,type) values(?,strftime('%s','now','localtime'), ?)");
			// TODO stat_time пока не используется, но пусть будет :)
			m_insert_statistic_json->bind(1, p_value, SQLITE_STATIC);
			m_insert_statistic_json->bind(2, p_type, SQLITE_STATIC);
			m_insert_statistic_json->executenonquery();
			++m_count_json_stat;
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - push_json_statistic: " + e.getError());
		}
	}
}
//========================================================================================================
void CFlylinkDBManager::flush_lost_json_statistic(bool& p_is_error)
{
	p_is_error = false;
	if (CFlyServerConfig::g_is_use_statistics && BOOLSETTING(USE_FLY_SERVER_STATICTICS_SEND)) // Отсылка статистики разрешена?
	{
		try
		{
			std::vector<__int64> l_json_array_id;
			if (m_count_json_stat && !ClientManager::isBeforeShutdown())
			{
				m_select_statistic_json.init(m_flySQLiteDB, "select id,stat_value_json,type from stat_db.fly_statistic limit 50");
				// where flush_time is null (пока не используем это поле и не храним локальную статистику - не придумал как подчищать данные)
				sqlite3_reader l_q = m_select_statistic_json->executereader();
				while (l_q.read() && !ClientManager::isBeforeShutdown())
				{
					bool l_is_send = false;
					const auto l_id = l_q.getint64(0);
					const std::string l_post_query = l_q.getstring(1);
					std::string l_type_query = l_q.getstring(2);
					if (l_type_query.empty())
						l_type_query = "fly-stat";
					const bool l_is_stat_server = l_type_query == "fly-stat";
					CFlyServerJSON::postQuery(true, l_is_stat_server, false, false, l_type_query.c_str(), l_post_query, l_is_send, p_is_error);
					if (p_is_error)
						break;
					if (l_is_send)
						l_json_array_id.push_back(l_id);
				}
			}
			if (l_json_array_id.size() < 50)
				m_count_json_stat = 0; // Все записи будут удалены ниже. скидываем счетчик в ноль чтобы не делать больше селекта
			else
				m_count_json_stat = 1; // Записи еще возможно есть - через минуту пробуем снова.
			if (!l_json_array_id.empty())
			{
				CFlyLock(m_cs);
				// Отметим факт пересылки статистики на сервер
				m_delete_statistic_json.init(m_flySQLiteDB, "delete from stat_db.fly_statistic where id=?");
				sqlite3_transaction l_trans(m_flySQLiteDB, l_json_array_id.size() > 1);
				// "update stat_db.fly_statistic set flush_time = strftime('%s','now','localtime') where id=?"));
				for (auto i = l_json_array_id.cbegin(); i != l_json_array_id.cend(); ++i)
				{
					m_delete_statistic_json->bind(1, *i);
					m_delete_statistic_json->executenonquery();
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
		CFlyLock(m_cs);
		try
		{
			{
				m_select_location_lost.init(m_flySQLiteDB,
				                            "select ip from location_db.fly_location_ip_lost where is_send_fly_server is null limit 100"); // За один раз больше 100 не шлем
				sqlite3_reader l_q = m_select_location_lost.get()->executereader();
				while (l_q.read())
				{
					p_lost_ip_array.push_back(l_q.getstring(0));
				}
			}
			// Отметим факт пересылки массива IP на сервер
			sqlite3_transaction l_trans(m_flySQLiteDB, p_lost_ip_array.size() > 1);
			m_update_location_lost.init(m_flySQLiteDB,
			                            "update location_db.fly_location_ip_lost set is_send_fly_server=1 where ip=?");
			for (auto i = p_lost_ip_array.cbegin(); i != p_lost_ip_array.cend(); ++i)
			{
				m_update_location_lost->bind(1, *i, SQLITE_STATIC);
				m_update_location_lost->executenonquery();
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
	CFlyLock(m_cs);
	if (m_count_fly_location_ip_record < 0)
	{
		m_select_count_location.init(m_flySQLiteDB,
		                             "select count(*) from location_db.fly_location_ip");
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
				m_insert_location_lost.init(m_flySQLiteDB,
				                            "insert into location_db.fly_location_ip_lost (ip) values(?)");
				try
				{
					m_insert_location_lost->bind(1, p_ip, SQLITE_STATIC);
					m_insert_location_lost->executenonquery();
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
void CFlylinkDBManager::save_location(const CFlyLocationIPArray& p_geo_ip)
{
	CFlyLock(m_cs);
	try
	{
		CFlyBusy l_disable_log(g_DisableSQLtrace);
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_delete_location.init(m_flySQLiteDB, "delete from location_db.fly_location_ip")->executenonquery();
		m_count_fly_location_ip_record = 0;
		m_insert_location.init(m_flySQLiteDB, "insert into location_db.fly_location_ip (start_ip,stop_ip,location,flag_index) values(?,?,?,?)");
		for (auto i = p_geo_ip.begin(); i != p_geo_ip.end(); ++i)
		{
			dcassert(i->m_start_ip  && !i->m_location.empty());
			m_insert_location->bind(1, i->m_start_ip);
			m_insert_location->bind(2, i->m_stop_ip);
			m_insert_location->bind(3, i->m_location, SQLITE_STATIC);
			m_insert_location->bind(4, i->m_flag_index);
			m_insert_location->executenonquery();
			++m_count_fly_location_ip_record;
		}
		l_trans.commit();
		{
			CFlyFastLock(m_cache_location_cs);
			m_location_cache_array.clear();
			m_ip_info_cache.clear();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_location: " + e.getError());
	}
}
#ifdef FLYLINKDC_USE_GEO_IP
//========================================================================================================
__int64 CFlylinkDBManager::get_dic_country_id(const string& p_country)
{
	CFlyLock(m_cs);
	return get_dic_idL(p_country, e_DIC_COUNTRY, true);
}
//========================================================================================================
bool CFlylinkDBManager::find_cache_country(uint32_t p_ip, uint16_t& p_index)
{
	CFlyFastLock(m_cache_location_cs);
	dcassert(p_ip);
	p_index = 0;
	const auto l_result = m_ip_info_cache.find(p_ip);
	if (m_ip_info_cache.find(p_ip) != m_ip_info_cache.end())
	{
		const auto& l_record = l_result->second;
		p_index = l_record.m_country_cache_index;
		return true;
	}
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
bool CFlylinkDBManager::find_cache_location(uint32_t p_ip, uint32_t& p_location_index, uint16_t& p_flag_location_index)
{
	CFlyFastLock(m_cache_location_cs);
	p_location_index = 0;
	p_flag_location_index = 0;
	const auto l_result = m_ip_info_cache.find(p_ip);
	if (m_ip_info_cache.find(p_ip) != m_ip_info_cache.end())
	{
		const auto& l_record = l_result->second;
		p_location_index = l_record.m_location_cache_index;
		p_flag_location_index = l_record.m_flag_location_index;
		return true;
	}
	for (auto i = m_location_cache_array.cbegin(); i != m_location_cache_array.cend(); ++i, ++p_location_index)
	{
		if (p_ip >= i->m_start_ip && p_ip < i->m_stop_ip)
		{
			++p_location_index;
			p_flag_location_index = i->m_flag_index;
			return true;
		}
	}
	return false;
}
//========================================================================================================
void CFlylinkDBManager::get_country_and_location(uint32_t p_ip, uint16_t& p_country_index, uint32_t& p_location_index, bool p_is_use_only_cache)
{
	dcassert(p_ip);
	uint16_t l_flag_location_index = 0; // TODO ?
	const bool l_is_find_country   = Util::isPrivateIp(p_ip) || find_cache_country(p_ip, p_country_index);
	const bool l_is_find_location = find_cache_location(p_ip, p_location_index, l_flag_location_index);
	if (p_is_use_only_cache == false)
	{
		if (l_is_find_country == false || l_is_find_location == false)
		{
			load_country_locations_p2p_guard_from_db(p_ip, p_location_index, p_country_index);
		}
	}
}
//========================================================================================================
string CFlylinkDBManager::load_country_locations_p2p_guard_from_db(uint32_t p_ip, uint32_t& p_location_cache_index, uint16_t& p_country_cache_index)
{
	dcassert(p_ip);
	CFlyLock(m_cs); // Без этого падает почему-то
	string l_p2p_guard_text;
	try
	{
		// http://www.sql.ru/forum/783621/faq-nahozhdenie-zapisey-gde-zadannoe-znachenie-nahoditsya-mezhdu-znacheniyami-poley
		// http://habrahabr.ru/post/138067/
		
		// TODO - optimisation if(!Util::isPrivateIp(p_ip))
		// TODO - склеить выборку в один запрос
		// для стран и p2p не запрашивать приватные адреса
		m_select_country_and_location.init(m_flySQLiteDB,
		                                   "select country,flag_index,start_ip,stop_ip,0 from "
		                                   "(select country,flag_index,start_ip,stop_ip from location_db.fly_country_ip where start_ip <=? order by start_ip desc limit 1) "
		                                   "where stop_ip >=?"
		                                   "\nunion all\n"
		                                   "select location,flag_index,start_ip,stop_ip,1 from "
		                                   "(select location,flag_index,start_ip,stop_ip from location_db.fly_location_ip where start_ip <=? order by start_ip desc limit 1) "
		                                   "where stop_ip >=?"
		                                   "\nunion all\n"
		                                   "select note,0,start_ip,stop_ip,2 from "
		                                   "(select note,start_ip,stop_ip from location_db.fly_p2pguard_ip where start_ip <=? order by start_ip desc limit 1) "
		                                   "where stop_ip >=?");
		m_select_country_and_location->bind(1, p_ip);
		m_select_country_and_location->bind(2, p_ip);
		m_select_country_and_location->bind(3, p_ip);
		m_select_country_and_location->bind(4, p_ip);
		m_select_country_and_location->bind(5, p_ip);
		m_select_country_and_location->bind(6, p_ip);
		sqlite3_reader l_q = m_select_country_and_location->executereader();
		CFlyLocationDesc l_location;
		p_location_cache_index = 0;
		p_country_cache_index = 0;
		l_location.m_flag_index = 0;
		unsigned l_count_country = 0;
		CFlyCacheIPInfo* l_ip_cahe_item = nullptr;
		{
			CFlyFastLock(m_cache_location_cs);
			l_ip_cahe_item = &m_ip_info_cache[p_ip];
		}
		while (l_q.read())
		{
			const unsigned l_id = l_q.getint(4);
			dcassert(l_id < 3)
			const string l_description = l_q.getstring(0);
			l_location.m_description = Text::toT(l_description);
			l_location.m_flag_index = l_q.getint(1);
			l_location.m_start_ip = l_q.getint(2);
			l_location.m_stop_ip = l_q.getint(3);
			switch (l_q.getint(4))
			{
				case 0:
				{
					l_count_country++;
					{
						CFlyFastLock(m_cache_location_cs);
						m_country_cache.push_back(l_location);
						p_country_cache_index = uint16_t(m_country_cache.size());
						l_ip_cahe_item->m_country_cache_index = p_country_cache_index;
						l_ip_cahe_item->m_flag_location_index = l_location.m_flag_index;
					}
					break;
				}
				case 1:
				{
					CFlyFastLock(m_cache_location_cs);
					m_location_cache_array.push_back(l_location);
					p_location_cache_index = m_location_cache_array.size();
					l_ip_cahe_item->m_location_cache_index = p_location_cache_index;
					l_ip_cahe_item->m_flag_location_index = l_location.m_flag_index;
					break;
				}
				case 2:
				{
					{
						CFlyFastLock(m_cache_location_cs);
						l_ip_cahe_item->m_description_p2p_guard = l_description;
					}
					if (!l_p2p_guard_text.empty())
					{
						l_p2p_guard_text += " + ";
					}
					l_p2p_guard_text += l_description;
					continue;
				}
				default:
					dcassert(0);
			}
		}
		dcassert(l_count_country <= 1); // Второго диапазона в GeoIPCountryWhois.csv быть не должно!
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_country_locations_p2p_guard_from_db: " + e.getError());
	}
	return l_p2p_guard_text;
}
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
//========================================================================================================
bool CFlylinkDBManager::is_avdb_guard(const string& p_nick, int64_t p_share, const uint32_t& p_ip)
{
	dcassert(Util::isPrivateIp(p_ip) == false);
	dcassert(p_ip && p_ip != INADDR_NONE);
	CFlyReadLock(*m_virus_cs);
	bool l_is_share_virus = p_share && m_virus_share.find(p_share) != m_virus_share.end();
	bool l_is_ip_virus    = m_virus_ip4.find(p_ip) != m_virus_ip4.end();
	if (l_is_ip_virus || l_is_share_virus ||
	        (m_virus_user.find(p_nick) != m_virus_user.end() && l_is_share_virus || l_is_ip_virus))
	{
		return true;
	}
	return false;
}
#endif
//========================================================================================================
string CFlylinkDBManager::is_p2p_guard(const uint32_t& p_ip)
{
	// dcassert(Util::isPrivateIp(p_ip) == false);
	dcassert(p_ip && p_ip != INADDR_NONE);
	string l_p2p_guard_text;
	if (p_ip && p_ip != INADDR_NONE)
	{
		{
			CFlyFastLock(m_cache_location_cs);
			const auto l_p2p = m_ip_info_cache.find(p_ip);
			if (l_p2p != m_ip_info_cache.end())
				return l_p2p->second.m_description_p2p_guard;
		}
		uint16_t l_country_index;
		uint32_t l_location_index;
		l_p2p_guard_text = load_country_locations_p2p_guard_from_db(p_ip, l_location_index, l_country_index);
	}
	return l_p2p_guard_text;
}
//========================================================================================================
void CFlylinkDBManager::remove_manual_p2p_guard(const string& p_ip)
{
	try
	{
		m_delete_manual_p2p_guard.init(m_flySQLiteDB,
		                               "delete from location_db.fly_p2pguard_ip where note = 'Manual block IP' and start_ip=?");
		boost::system::error_code ec;
		const auto l_ip_boost = boost::asio::ip::address_v4::from_string(p_ip, ec);
		if (!ec)
		{
		
			m_delete_manual_p2p_guard->bind(1, l_ip_boost.to_ulong());
			m_delete_manual_p2p_guard->executenonquery();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_manual_p2p_guard: " + e.getError());
	}
}
//========================================================================================================
string CFlylinkDBManager::load_manual_p2p_guard()
{
	string l_result;
	try
	{
		m_select_manual_p2p_guard.init(m_flySQLiteDB,
		                               "select distinct start_ip from location_db.fly_p2pguard_ip where note = 'Manual block IP'");
		sqlite3_reader l_q = m_select_manual_p2p_guard->executereader();
		while (l_q.read())
		{
			l_result += boost::asio::ip::address_v4(l_q.getint(0)).to_string() + "\r\n";
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_manual_p2p_guard: " + e.getError());
	}
	return l_result;
}
//========================================================================================================
void CFlylinkDBManager::save_p2p_guard(const CFlyP2PGuardArray& p_p2p_guard_ip, const string& p_manual_marker, int p_type)
{
	CFlyLock(m_cs);
	try
	{
		{
			CFlyFastLock(m_cache_location_cs);
			m_ip_info_cache.clear();
		}
		CFlyBusy l_disable_log(g_DisableSQLtrace);
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (p_manual_marker.empty())
		{
			m_delete_p2p_guard.init(m_flySQLiteDB, "delete from location_db.fly_p2pguard_ip where note <> 'Manual block IP' and (type=? or type is null)");
			m_delete_p2p_guard->bind(1, p_type);
			m_delete_p2p_guard->executenonquery();
		}
		m_insert_p2p_guard.init(m_flySQLiteDB, "insert into location_db.fly_p2pguard_ip (start_ip,stop_ip,note,type) values(?,?,?,?)");
		for (auto i = p_p2p_guard_ip.begin(); i != p_p2p_guard_ip.end(); ++i)
		{
			dcassert(!i->m_note.empty());
			m_insert_p2p_guard->bind(1, i->m_start_ip);
			m_insert_p2p_guard->bind(2, i->m_stop_ip);
			m_insert_p2p_guard->bind(3, i->m_note, SQLITE_STATIC);
			m_insert_p2p_guard->bind(4, p_type);
			m_insert_p2p_guard->executenonquery();
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_p2p_guard: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::save_geoip(const CFlyLocationIPArray& p_geo_ip)
{
	CFlyLock(m_cs);
	try
	{
		CFlyBusy l_disable_log(g_DisableSQLtrace);
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_delete_geoip.init(m_flySQLiteDB, "delete from location_db.fly_country_ip");
		m_delete_geoip->executenonquery();
		m_insert_geoip.init(m_flySQLiteDB, "insert into location_db.fly_country_ip (start_ip,stop_ip,country,flag_index) values(?,?,?,?)");
		for (auto i = p_geo_ip.begin(); i != p_geo_ip.end(); ++i)
		{
			dcassert(i->m_start_ip  && !i->m_location.empty());
			m_insert_geoip->bind(1, i->m_start_ip);
			m_insert_geoip->bind(2, i->m_stop_ip);
			m_insert_geoip->bind(3, i->m_location, SQLITE_STATIC);
			m_insert_geoip->bind(4, i->m_flag_index);
			m_insert_geoip->executenonquery();
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_geoip: " + e.getError());
	}
}
#endif // FLYLINKDC_USE_GEO_IP
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
//========================================================================================================
bool CFlylinkDBManager::is_virus_bot(const string& p_nick, int64_t p_share, const boost::asio::ip::address_v4& p_ip4)
{
	if (p_share)
	{
		CFlyReadLock(*m_virus_cs);
		const bool l_is_share = p_share && m_virus_share.find(p_share) != m_virus_share.end();
		if (l_is_share && m_virus_user.find(p_nick) != m_virus_user.end() ||
		        l_is_share && !p_ip4.is_unspecified() && m_virus_ip4.find(p_ip4.to_ulong()) != m_virus_ip4.end())
		{
			return true;
		}
	}
	return false;
}
//========================================================================================================
int CFlylinkDBManager::calc_antivirus_flag(const string& p_nick, const boost::asio::ip::address_v4& p_ip4, int64_t p_share, string& p_virus_path)
{
	CFlyLock(m_cs); // TODO без лока падаем
	int l_result = 0;
	try
	{
		CFlyReadLock(*m_virus_cs);
		//dcassert(!m_virus_user.empty());
		if (m_virus_user.find(p_nick) != m_virus_user.end() ||
		        (p_share && m_virus_share.find(p_share) != m_virus_share.end()) ||
		        (!p_ip4.is_unspecified() && m_virus_ip4.find(p_ip4.to_ulong()) != m_virus_ip4.end()))
		{
			if (!p_ip4.is_unspecified())
			{
				m_find_virus_nick_and_share_and_ip4.init(m_flySQLiteDB,
				                                         "select ip4,share,virus_path from antivirus_db.fly_suspect_user where nick=? or share=? or ip4=?"
				                                        );
			}
			if (p_ip4.is_unspecified())
			{
				m_find_virus_nick_and_share.init(m_flySQLiteDB,
				                                 "select ip4,share,virus_path from antivirus_db.fly_suspect_user where nick=? or share=?"
				                                );
			}
			
			auto l_sql = p_ip4.is_unspecified() ? m_find_virus_nick_and_share.get_sql() : m_find_virus_nick_and_share_and_ip4.get_sql();
			l_sql->bind(1, p_nick, SQLITE_STATIC);
			l_sql->bind(2, p_share);
			if (!p_ip4.is_unspecified())
			{
				l_sql->bind(3, p_ip4.to_ulong());
			}
			sqlite3_reader l_q = l_sql->executereader();
			p_virus_path.clear();
			boost::unordered_set<string> l_dup_filter;
			while (l_q.read())
			{
				if (p_ip4 == boost::asio::ip::address_v4((unsigned long)l_q.getint64(0)))
				{
					l_result |= Identity::VT_IP;
				}
				if (p_share == l_q.getint64(1))
				{
					l_result |= Identity::VT_SHARE;
				}
				const auto l_virus_path = l_q.getstring(2);
				if (!l_virus_path.empty() && l_dup_filter.find(l_virus_path) == l_dup_filter.end())
				{
					l_dup_filter.insert(l_virus_path);
					p_virus_path += " [" + l_virus_path + "]";
				}
			}
		}
		if ((l_result & Identity::VT_NICK) == 0)
		{
			if (m_virus_user.find(p_nick) != m_virus_user.end())
			{
				l_result |= Identity::VT_NICK;
			}
		}
		if ((l_result & Identity::VT_SHARE) == 0)
		{
			if (m_virus_share.find(p_share) != m_virus_share.end())
			{
				l_result |= Identity::VT_SHARE;
			}
		}
		if ((l_result & Identity::VT_IP) == 0)
		{
			if (m_virus_ip4.find(p_ip4.to_ulong()) != m_virus_ip4.end())
			{
				l_result |= Identity::VT_IP;
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - calc_antivirus_flag: " + e.getError());
	}
	return l_result;
}
//========================================================================================================
int CFlylinkDBManager::get_antivirus_record_count()
{
	try
	{
		const int l_count = m_flySQLiteDB.executeint("select count(*) from antivirus_db.fly_suspect_user");
		return l_count;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_antivirus_record_count: " + e.getError());
	}
	return 0;
}
//========================================================================================================
void CFlylinkDBManager::purge_antivirus_db(const uint64_t p_delete_counter, const uint64_t p_unixtime, bool p_is_clean_cache)
{
	CFlyLock(m_cs); // TODO
	try
	{
		unique_ptr<sqlite3_command> l_delete_antivirus_db(new sqlite3_command(m_flySQLiteDB,
		                                                                      "delete from antivirus_db.fly_suspect_user"));
		l_delete_antivirus_db->executenonquery();
		set_registry_variable_int64(e_DeleteCounterAntivirusDB, p_delete_counter);
		set_registry_variable_int64(e_TimeStampAntivirusDB, p_unixtime);
		set_registry_variable_int64(e_MergeCounterAntivirusDB, 0);
		if (p_is_clean_cache)
		{
			clear_virus_cacheL();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - purge_antivirus_db: " + e.getError());
	}
}
//========================================================================================================
int CFlylinkDBManager::sync_antivirus_db(const string& p_antivirus_db, const uint64_t p_unixtime)
{
	int l_count_new_user = 0;
	if (BOOLSETTING(AUTOUPDATE_ANTIVIRUS_DB))
	{
		CFlyBusy l_disable_log(g_DisableSQLtrace);
		CFlyLock(m_cs); // TODO - Вернул лок - не понятно почему-то иногда падает
#ifdef _DEBUG
		std::unordered_set<string> l_dup_nick_ip;
#endif
		try
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			m_update_antivirus_db.init(m_flySQLiteDB,
			                           "update antivirus_db.fly_suspect_user set share=?,virus_path=? where nick=? and ip4=?");
			int l_pos = 0;
			int l_nick_pos = 0;
			int l_nick_len = 0;
			string l_ip4;
			while (true)
			{
				l_nick_pos = l_pos;
				auto l_sep_pos = p_antivirus_db.find('|', l_pos);
				if (l_sep_pos == string::npos)
					break;
				l_nick_len = l_sep_pos - l_nick_pos;
				string l_nick = Text::toUtf8(p_antivirus_db.substr(l_nick_pos, l_nick_len));
				l_pos      = l_sep_pos + 1;
				l_sep_pos = p_antivirus_db.find('|', l_pos);
				if (l_sep_pos == string::npos)
					break;
				l_ip4 = p_antivirus_db.substr(l_pos, l_sep_pos - l_pos);
				boost::system::error_code ec;
				dcassert(!l_ip4.empty());
				const auto l_boost_ip4 = boost::asio::ip::address_v4::from_string(l_ip4, ec);
				dcassert(!ec);
				l_pos = l_sep_pos + 1;
				const int64_t l_share = _atoi64(p_antivirus_db.c_str() + l_pos);
				dcassert(l_share);
				l_pos = l_sep_pos + 1;
				l_sep_pos = p_antivirus_db.find('|', l_pos);
				if (l_sep_pos == string::npos)
					break;
				l_pos = l_sep_pos + 1;
				l_sep_pos = p_antivirus_db.find('\n', l_pos);
				if (l_sep_pos == string::npos)
					break;
				string l_virus_path;
				if (p_antivirus_db.size() > l_pos && p_antivirus_db[l_pos] != '\n')
				{
					dcassert(l_sep_pos > l_pos);
					if (l_sep_pos > l_pos)
					{
						l_virus_path = Text::toUtf8(p_antivirus_db.substr(l_pos, l_sep_pos - l_pos));
					}
				}
				l_pos = l_sep_pos + 1;
				m_update_antivirus_db->bind(1, l_share);
				m_update_antivirus_db->bind(2, l_virus_path, SQLITE_STATIC);
				m_update_antivirus_db->bind(3, l_nick, SQLITE_STATIC);
				m_update_antivirus_db->bind(4, l_boost_ip4.to_ulong());
				m_update_antivirus_db->executenonquery();
				if (m_update_antivirus_db.sqlite3_changes() == 0)
				{
					m_insert_antivirus_db.init(m_flySQLiteDB,
					                           "insert or replace into antivirus_db.fly_suspect_user(share,virus_path,nick,ip4) values(?,?,?,?)");
					m_insert_antivirus_db->bind(1, l_share);
					m_insert_antivirus_db->bind(2, l_virus_path, SQLITE_STATIC);
					m_insert_antivirus_db->bind(3, l_nick, SQLITE_STATIC);
					m_insert_antivirus_db->bind(4, l_boost_ip4.to_ulong());
					m_insert_antivirus_db->executenonquery();
				}
#ifdef _DEBUG
				const auto l_key = l_nick + " + " + l_boost_ip4.to_string();
				const auto l_res = l_dup_nick_ip.insert(l_key);
				if (l_res.second == false)
				{
					dcassert(0);
					LogManager::message("antivirus_db.fly_suspect_user duplicate user:" + l_key);
				}
#endif
				{
					CFlyWriteLock(*m_virus_cs);
					m_virus_user.insert(l_nick);
					const unsigned long l_ip = l_boost_ip4.to_ulong();
					dcassert(l_ip);
					m_virus_ip4.insert(l_ip);
					dcassert(l_share);
					m_virus_share.insert(l_share);
				}
				++l_count_new_user;
			}
			l_trans.commit();
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - sync_antivirus_db: " + e.getError());
		}
	}
	if (l_count_new_user)
	{
		set_registry_variable_int64(e_TimeStampAntivirusDB, p_unixtime);
	}
	dcassert(l_count_new_user);
	return l_count_new_user;
}
#endif // FLYLINKDC_USE_ANTIVIRUS_DB
//========================================================================================================
void CFlylinkDBManager::set_registry_variable_string(eTypeSegment p_TypeSegment, const string& p_value)
{
	CFlyRegistryMap l_store_values;
	l_store_values[Util::toString(p_TypeSegment)] = p_value;
	save_registry(l_store_values, p_TypeSegment, false);
}
//========================================================================================================
string CFlylinkDBManager::get_registry_variable_string(eTypeSegment p_TypeSegment)
{
	CFlyRegistryMap l_values;
	load_registry(l_values, p_TypeSegment);
	if (!l_values.empty())
		return l_values.begin()->second.m_val_str;
	else
		return Util::emptyString;
}
//========================================================================================================
void CFlylinkDBManager::set_registry_variable_int64(eTypeSegment p_TypeSegment, __int64 p_value)
{
	CFlyRegistryMap l_store_values;
	l_store_values[Util::toString(p_TypeSegment)] = CFlyRegistryValue(p_value);
	save_registry(l_store_values, p_TypeSegment, false);
}
//========================================================================================================
__int64 CFlylinkDBManager::get_registry_variable_int64(eTypeSegment p_TypeSegment)
{
	CFlyRegistryMap l_values;
	load_registry(l_values, p_TypeSegment);
	if (!l_values.empty())
		return l_values.begin()->second.m_val_int64;
	else
		return 0;
}
//========================================================================================================
void CFlylinkDBManager::load_registry(CFlyRegistryMap& p_values, eTypeSegment p_Segment)
{
	CFlyLock(m_cs); // Убирать нельзя - падаем  SQLite - load_registry: library routine called out of sequence
	try
	{
		m_get_registry.init(m_flySQLiteDB, "select key,val_str,val_number from fly_registry where segment=? order by rowid")->bind(1, p_Segment);
		sqlite3_reader l_q = m_get_registry->executereader();
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
void CFlylinkDBManager::clean_registry(eTypeSegment p_Segment, __int64 p_tick)
{
	CFlyLock(m_cs);
	try
	{
		clean_registryL(p_Segment, p_tick);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - clean_registry: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::clean_registryL(eTypeSegment p_Segment, __int64 p_tick)
{
	m_delete_registry.init(m_flySQLiteDB, "delete from fly_registry where segment=? and tick_count<>?");
	m_delete_registry->bind(1, p_Segment);
	m_delete_registry->bind(2, p_tick);
	m_delete_registry->executenonquery();
}
//========================================================================================================
void CFlylinkDBManager::save_registry(const CFlyRegistryMap& p_values, eTypeSegment p_Segment, bool p_is_cleanup_old_value)
{
	const __int64 l_tick = GET_TICK() + Util::rand();
	CFlyLock(m_cs);
	try
	{
		m_insert_registry.init(m_flySQLiteDB, "insert or replace into fly_registry (segment,key,val_str,val_number,tick_count) values(?,?,?,?,?)");
		m_update_registry.init(m_flySQLiteDB, "update fly_registry set val_str=?,val_number=?,tick_count=? where segment=? and key=?");
		sqlite3_transaction l_trans(m_flySQLiteDB, (p_values.size() > 1) || p_is_cleanup_old_value);
		for (auto k = p_values.cbegin(); k != p_values.cend(); ++k)
		{
			m_update_registry->bind(1, k->second.m_val_str, SQLITE_TRANSIENT);
			m_update_registry->bind(2, k->second.m_val_int64);
			m_update_registry->bind(3, l_tick);
			m_update_registry->bind(4, int(p_Segment));
			m_update_registry->bind(5, k->first, SQLITE_TRANSIENT);
			m_update_registry->executenonquery();
			if (m_update_registry.sqlite3_changes() == 0)
			{
				m_insert_registry->bind(1, int(p_Segment));
				m_insert_registry->bind(2, k->first, SQLITE_TRANSIENT);
				m_insert_registry->bind(3, k->second.m_val_str, SQLITE_TRANSIENT);
				m_insert_registry->bind(4, k->second.m_val_int64);
				m_insert_registry->bind(5, l_tick);
				m_insert_registry->executenonquery();
			}
		}
		if (p_is_cleanup_old_value)
		{
			clean_registryL(p_Segment, l_tick);
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_registry: " + e.getError());
	}
}
//========================================================================================================
bool CFlylinkDBManager::is_download_tth(const TTHValue& p_tth)
{
	try
	{
		m_is_download_tth.init(m_flySQLiteDB,
		                       "select 1 from transfer_db.fly_transfer_file where type=0 and tth=? limit 1");
		const auto l_tth = p_tth.toBase32();
		m_is_download_tth->bind(1, l_tth, SQLITE_STATIC);
		sqlite3_reader l_q = m_is_download_tth->executereader();
		while (l_q.read())
		{
			return true;
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - is_download_tth: " + e.getError());
	}
	return false;
}
//========================================================================================================
void CFlylinkDBManager::cleanup_transfer_historgam()
{
	if ((SETTING(DB_LOG_FINISHED_DOWNLOADS) || SETTING(DB_LOG_FINISHED_UPLOADS)))
	{
		CFlySQLCommand l_sql_delete_dc;
		CFlySQLCommand l_sql_delete_torrent;
		const string l_sql_dc = get_purge_transfer_history("fly_transfer_file");
		const string l_sql_torrent = get_purge_transfer_history("fly_transfer_file_torrent");
		l_sql_delete_dc.init(m_flySQLiteDB, l_sql_dc.c_str());
		l_sql_delete_dc.executenonquery();
		l_sql_delete_torrent.init(m_flySQLiteDB, l_sql_torrent.c_str());
		l_sql_delete_torrent.executenonquery();
	}
}
//========================================================================================================
void CFlylinkDBManager::load_transfer_historgam(bool p_is_torrent, eTypeTransfer p_type, CFlyTransferHistogramArray& p_array)
{
	// CFlyLock(m_cs);
	try
	{
		static bool g_is_first = false;
		if (!g_is_first)
		{
			g_is_first = true;
			cleanup_transfer_historgam();
		}
		if (!p_is_torrent)
		{
			m_select_transfer_histrogram.init(m_flySQLiteDB,
			                                  "select day,strftime('%d.%m.%Y',day*60*60*24,'unixepoch'),count(*),sum(actual) "
			                                  "from transfer_db.fly_transfer_file where type=? group by day order by day desc");
			m_select_transfer_histrogram->bind(1, p_type);
		}
		else
		{
			m_select_transfer_histrogram_torrent.init(m_flySQLiteDB,
			                                          "select day,strftime('%d.%m.%Y',day*60*60*24,'unixepoch'),count(*) "
			                                          "from transfer_db.fly_transfer_file_torrent where type=? group by day order by day desc");
			m_select_transfer_histrogram_torrent->bind(1, p_type);
		}
		sqlite3_reader l_q = p_is_torrent ? m_select_transfer_histrogram_torrent->executereader() : m_select_transfer_histrogram->executereader();
		while (l_q.read())
		{
			CFlyTransferHistogram l_item;
			l_item.m_date = l_q.getstring(1);
			l_item.m_count = l_q.getint(2);
			l_item.m_date_as_int = l_q.getint(0);
			if (!p_is_torrent)
			{
				l_item.m_actual = l_q.getint64(3);
			}
			else
			{
				l_item.m_actual = 0; // TODO
			}
			p_array.push_back(l_item);
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_transfer_historgam: " + e.getError());
	}
}
//========================================================================================================
std::string CFlylinkDBManager::get_purge_transfer_history(std::string p_table_name)
{
	const string l_sql = "delete from transfer_db." + p_table_name + " where (type=1 and day<strftime('%s','now','localtime')/60/60/24-"
	                     + Util::toString(SETTING(DB_LOG_FINISHED_UPLOADS)) + ") or "
	                     "(type=0 and day<strftime('%s','now','localtime')/60/60/24-" + Util::toString(SETTING(DB_LOG_FINISHED_DOWNLOADS)) + ")";
	return l_sql;
}
//========================================================================================================
void CFlylinkDBManager::load_transfer_history(bool p_is_torrent, eTypeTransfer p_type, int p_day)
{
	try
	{
		if (p_is_torrent)
		{
			m_select_transfer_day_torrent.init(m_flySQLiteDB,
			                                   "select path,sha1,size,stamp,id "
			                                   "from transfer_db.fly_transfer_file_torrent where type=? and day=?");
			m_select_transfer_day_torrent->bind(1, p_type);
			m_select_transfer_day_torrent->bind(2, p_day);
		}
		else
		{
			m_select_transfer_day.init(m_flySQLiteDB,
			                           "select path,nick,hub,size,speed,stamp,ip,tth,id,actual "
			                           "from transfer_db.fly_transfer_file where type=? and day=?");
			m_select_transfer_day->bind(1, p_type);
			m_select_transfer_day->bind(2, p_day);
		}
		
		sqlite3_reader l_q = p_is_torrent ? m_select_transfer_day_torrent->executereader() : m_select_transfer_day->executereader();
		while (l_q.read())
		{
			if (p_is_torrent)
			{
				libtorrent::sha1_hash l_sha1;
				l_q.getblob(1, l_sha1.data(), l_sha1.size());
				auto item = std::make_shared<FinishedItem>(l_q.getstring(0), l_sha1, l_q.getint64(2), 0, l_q.getint64(3), 0, l_q.getint64(4));
				FinishedManager::getInstance()->pushHistoryFinishedItem(item, p_type);
			}
			else
			{
				auto item = std::make_shared<FinishedItem>(l_q.getstring(0),
				                                           l_q.getstring(1),
				                                           l_q.getstring(2),
				                                           l_q.getint64(3),
				                                           l_q.getint64(4),
				                                           l_q.getint64(5),
				                                           TTHValue(l_q.getstring(7)),
				                                           l_q.getstring(6),
				                                           l_q.getint64(8),
				                                           l_q.getint64(9)
				                                          );
				FinishedManager::getInstance()->pushHistoryFinishedItem(item, p_type);
			}
		}
		FinishedManager::getInstance()->updateStatus();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_transfer_history: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::delete_transfer_history_torrent(const vector<__int64>& p_id_array)
{
	if (!p_id_array.empty())
	{
		try
		{
			CFlyLock(m_cs);
			sqlite3_transaction l_trans(m_flySQLiteDB, p_id_array.size() > 1);
			m_delete_transfer_torrent.init(m_flySQLiteDB,
			                               "delete from transfer_db.fly_transfer_file_torrent where id=?");
			for (auto i = p_id_array.cbegin(); i != p_id_array.cend(); ++i)
			{
				dcassert(*i);
				m_delete_transfer_torrent->bind(1, *i);
				m_delete_transfer_torrent->executenonquery();
			}
			l_trans.commit();
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - delete_transfer_history_torrent: " + e.getError());
		}
	}
}
//========================================================================================================
void CFlylinkDBManager::delete_transfer_history(const vector<__int64>& p_id_array)
{
	if (!p_id_array.empty())
	{
		try
		{
			CFlyLock(m_cs);
			sqlite3_transaction l_trans(m_flySQLiteDB, p_id_array.size() > 1);
			m_delete_transfer.init(m_flySQLiteDB,
			                       "delete from transfer_db.fly_transfer_file where id=?");
			for (auto i = p_id_array.cbegin(); i != p_id_array.cend(); ++i)
			{
				dcassert(*i);
				m_delete_transfer->bind(1, *i);
				m_delete_transfer->executenonquery();
			}
			l_trans.commit();
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - delete_transfer_history: " + e.getError());
		}
	}
}
//========================================================================================================
void CFlylinkDBManager::load_torrent_resume(libtorrent::session& p_session)
{
	try
	{
		m_select_resume_torrent.init(m_flySQLiteDB,
		                             "select resume,sha1 from queue_db.fly_queue_torrent");
		sqlite3_reader l_q = m_select_resume_torrent->executereader();
		while (l_q.read())
		{
			vector<uint8_t> l_resume;
			l_q.getblob(0, l_resume);
			//dcassert(!l_resume.empty());
			if (!l_resume.empty())
			{
				libtorrent::error_code ec;
				libtorrent::add_torrent_params p = libtorrent::read_resume_data({ (const char*)l_resume.data(), l_resume.size()}, ec);
				//p.save_path = SETTING(DOWNLOAD_DIRECTORY); // TODO - load from DB ?
				libtorrent::sha1_hash l_sha1;
				l_q.getblob(1, l_sha1.data(), l_sha1.size());
				p.info_hash = l_sha1;
				p.flags |= libtorrent::torrent_flags::auto_managed;
				{
					CFlyFastLock(g_resume_torrents_cs);
					g_resume_torrents.insert(l_sha1);
				}
#ifdef _DEBUG
				ec.clear();
				p_session.async_add_torrent(std::move(p)); // TODO sync for debug
				if (ec)
				{
					dcdebug("%s\n", ec.message().c_str());
					dcassert(0);
					LogManager::message("Error add_torrent_file: " + ec.message());
				}
#else
				p_session.async_add_torrent(std::move(p));
#endif
			}
			else
			{
				LogManager::torrent_message("Error add_torrent_file: resume data is empty()");
				// TODO delete_torrent_resume
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_torrent_resume: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::delete_torrent_resume(const libtorrent::sha1_hash& p_sha1)
{
	CFlyLock(m_cs);
	try
	{
		m_delete_resume_torrent.init(m_flySQLiteDB,
		                             "delete from queue_db.fly_queue_torrent where sha1=?");
		m_delete_resume_torrent->bind(1, p_sha1.data(), p_sha1.size(), SQLITE_STATIC);
		m_delete_resume_torrent->executenonquery();
		if (m_delete_resume_torrent.sqlite3_changes() == 1)
		{
			CFlyFastLock(g_delete_torrents_cs);
			g_delete_torrents.insert(p_sha1);
		}
		else
		{
			dcassert(0); // Зовем второй раз удаление - не хорошо
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - delete_torrent_resume: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::save_torrent_resume(const libtorrent::sha1_hash& p_sha1, const std::string& p_name, const std::vector<char>& p_resume)
{
	CFlyLock(m_cs);
	try
	{
		m_check_resume_torrent.init(m_flySQLiteDB,
		                            "select resume,id from queue_db.fly_queue_torrent where sha1=?");
		m_check_resume_torrent->bind(1, p_sha1.data(), p_sha1.size(), SQLITE_STATIC);
		bool l_is_need_update = true;
		__int64 l_ID = 0;
		{
			sqlite3_reader l_q_check = m_check_resume_torrent->executereader();
			while (l_q_check.read())
			{
				l_ID = l_q_check.getint64(1);
				vector<uint8_t> l_resume;
				l_q_check.getblob(0, l_resume);
				//dcassert(!l_resume.empty());
				if (!l_resume.empty() && l_resume.size() == p_resume.size())
				{
					l_is_need_update = !memcmp(&p_resume[0], &l_resume[0], l_resume.size());
				}
			}
		}
		if (l_is_need_update)
		{
			if (l_ID == 0)
			{
				m_insert_resume_torrent.init(m_flySQLiteDB,
				                             "insert or replace into queue_db.fly_queue_torrent (day,stamp,sha1,resume,name) "
				                             "values(strftime('%s','now','localtime')/60/60/24,strftime('%s','now','localtime'),?,?,?)");
				m_insert_resume_torrent->bind(1, p_sha1.data(), p_sha1.size(), SQLITE_STATIC);
				m_insert_resume_torrent->bind(2, &p_resume[0], p_resume.size(), SQLITE_STATIC);
				m_insert_resume_torrent->bind(3, p_name, SQLITE_STATIC);
				m_insert_resume_torrent->executenonquery();
			}
			else
			{
				m_update_resume_torrent.init(m_flySQLiteDB,
				                             "update queue_db.fly_queue_torrent set day = strftime('%s','now','localtime')/60/60/24,"
				                             "stamp = strftime('%s','now','localtime'),"
				                             "resume = ?,"
				                             "name = ? where id = ?");
				m_update_resume_torrent->bind(1, &p_resume[0], p_resume.size(), SQLITE_STATIC);
				m_update_resume_torrent->bind(2, p_name, SQLITE_STATIC);
				m_update_resume_torrent->bind(3, l_ID);
				m_update_resume_torrent->executenonquery();
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_torrent_resume: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::save_transfer_history(bool p_is_torrent, eTypeTransfer p_type, const FinishedItemPtr& p_item)
{
	// TODO const int l_tick = GET_TICK();
	string l_name;
	string l_path;
	if (!p_is_torrent)
	{
		l_name = Text::toLower(Util::getFileName(p_item->getTarget()));
		l_path = Text::toLower(Util::getFilePath(p_item->getTarget()));
		{
			CFlyFastLock(g_tth_cache_cs);
			g_tiger_tree_cache.erase(p_item->getTTH());
		}
	}
	CFlyLock(m_cs);
	try
	{
		if (!p_is_torrent)
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			inc_hitL(l_path, l_name);
			m_insert_transfer.init(m_flySQLiteDB,
			                       "insert into transfer_db.fly_transfer_file (type,day,stamp,path,nick,hub,size,speed,ip,tth,actual) "
			                       "values(?,strftime('%s','now','localtime')/60/60/24,strftime('%s','now','localtime'),?,?,?,?,?,?,?,?)");
			m_insert_transfer->bind(1, p_type);
			m_insert_transfer->bind(2, p_item->getTarget(), SQLITE_STATIC);
			m_insert_transfer->bind(3, p_item->getNick(), SQLITE_STATIC);
			m_insert_transfer->bind(4, p_item->getHub(), SQLITE_STATIC);
			m_insert_transfer->bind(5, p_item->getSize());
			m_insert_transfer->bind(6, p_item->getAvgSpeed());
			m_insert_transfer->bind(7, p_item->getIP(), SQLITE_STATIC);
			if (p_item->getTTH() != TTHValue())
				m_insert_transfer->bind(8, p_item->getTTH().toBase32(), SQLITE_TRANSIENT); // SQLITE_TRANSIENT!
			else
				m_insert_transfer->bind(8);
			m_insert_transfer->bind(9, p_item->getActual());
			m_insert_transfer->executenonquery();
			l_trans.commit();
		}
		else
		{
			m_insert_transfer_torrent.init(m_flySQLiteDB,
			                               "insert into transfer_db.fly_transfer_file_torrent (day,type,stamp,path,size,sha1) "
			                               "values(strftime('%s','now','localtime')/60/60/24,?,?,?,?,?)");
			m_insert_transfer_torrent->bind(1, p_type);
			m_insert_transfer_torrent->bind(2, p_item->getTime());
			m_insert_transfer_torrent->bind(3, p_item->getTarget(), SQLITE_STATIC);
			m_insert_transfer_torrent->bind(4, p_item->getSize());
			if (!p_item->m_sha1.is_all_zeros())
			{
				m_insert_transfer_torrent->bind(5, p_item->m_sha1.data(), p_item->m_sha1.size(), SQLITE_STATIC);
			}
			else
			{
				m_insert_transfer_torrent->bind(5);
			}
			m_insert_transfer_torrent->executenonquery();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_transfer_history: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::load_ignore(StringSet& p_ignores)
{
	// CFlyLock(m_cs);
	try
	{
		m_get_ignores.init(m_flySQLiteDB,
		                   "select trim(nick) from fly_ignore");
		sqlite3_reader l_q = m_get_ignores->executereader();
		string l_users;
		string l_sep;
		while (l_q.read())
		{
			const string& l_ignore_user = l_q.getstring(0);
			if (!l_ignore_user.empty())
			{
				l_users += l_sep + l_ignore_user;
				p_ignores.insert(l_ignore_user);
				l_sep = " , ";
			}
		}
		if (!p_ignores.empty())
		{
			LogManager::message(STRING(IGNORE_USER_BY_NAME) + ": " + l_users, true);
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
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_delete_ignores.init(m_flySQLiteDB,
		                      "delete from fly_ignore");
		m_delete_ignores->executenonquery();
		m_insert_ignores.init(m_flySQLiteDB,
		                      "insert or replace into fly_ignore (nick) values(?)");
		for (auto k = p_ignores.cbegin(); k != p_ignores.cend(); ++k)
		{
			string l_ignore_user = (*k);
			boost::algorithm::trim(l_ignore_user);
			if (!l_ignore_user.empty())
			{
				m_insert_ignores->bind(1, l_ignore_user, SQLITE_TRANSIENT);
				m_insert_ignores->executenonquery();
			}
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_ignore: " + e.getError());
	}
}
//========================================================================================================
class CFlySourcesItem
{
	public:
		CID m_CID;
		string m_nick;
		int m_hub_id;
		CFlySourcesItem(const CID& p_CID, const string& p_nick, int p_hub_id):
			m_CID(p_CID), m_nick(p_nick), m_hub_id(p_hub_id)
		{
		}
};
//========================================================================================================
int32_t CFlylinkDBManager::load_queue()
{
	vector<QueueItemPtr> l_qitem;
	{
		g_count_queue_source = 0;
		g_count_queue_files = 0;
#ifdef _DEBUG
		std::unordered_set<string> l_cache_duplicate_tth;
		
// #define FLYLINKDC_USE_DEBUG_10_RECORD
#endif
		try
		{
			boost::unordered_map<int, std::vector< CFlySourcesItem > > l_sources_map;
			{
				CFlyLog l_src_log("[Load queue source]");
#ifdef FLYLINKDC_USE_DEBUG_10_RECORD
				m_get_fly_queue_all_source.init(m_flySQLiteDB, "select fly_queue_id,cid,nick,dic_hub from fly_queue_source where fly_queue_id < 10");
#else
				m_get_fly_queue_all_source.init(m_flySQLiteDB, "select fly_queue_id,cid,nick,dic_hub from fly_queue_source");
#endif
				sqlite3_reader l_q = m_get_fly_queue_all_source->executereader();
				unsigned l_count_users = 0;
				while (l_q.read() && !ClientManager::isBeforeShutdown())
				{
					CID l_cid;
					if (l_q.getblob(1, l_cid.get_data_for_write(), 24))
					{
						dcassert(!l_cid.isZero());
						//if (!l_cid.isZero())
						{
							auto& l_source_items = l_sources_map[l_q.getint(0)];
							dcassert(!l_q.getstring(2).empty());
							l_source_items.push_back(CFlySourcesItem(l_cid, l_q.getstring(2), l_q.getint(3)));
							l_count_users++;
						}
					}
				}
				if (!l_sources_map.empty())
				{
					l_src_log.step("Files: " + Util::toString(l_sources_map.size()) + " Users: " + Util::toString(l_count_users));
				}
			}
			const char* l_sql = "select "
			                    "id,"
			                    "Target,"
			                    "Size,"
			                    "Priority,"
			                    "Sections,"
			                    "Added,"
			                    "TTH,"
			                    "TempTarget,"
			                    "AutoPriority,"
			                    "MaxSegments"
			                    " from fly_queue"
#ifdef FLYLINKDC_USE_DEBUG_10_RECORD
			                    " where id < 10"
#endif
			                    ;
			m_get_fly_queue.init(m_flySQLiteDB, l_sql);
			vector<__int64> l_bad_targets;
			{
				CFlyLog l_q_files_log("[Load queue files]");
				sqlite3_reader l_q = m_get_fly_queue->executereader();
				while (l_q.read() && !ClientManager::isBeforeShutdown())
				{
					const int64_t l_size = l_q.getint64(2);
					if (l_size < 0)
						continue;
					//[-] brain-ripper
					// int l_flags = QueueItem::FLAG_RESUME;
					int l_flags = 0;
					g_count_queue_files++;
					const string l_target = l_q.getstring(1);
#if 0
					const string l_tgt = l_q.getstring(1);
					string l_target;
					try
					{
						// TODO отложить валидацию на потом
						l_target = QueueManager::checkTarget(l_tgt, l_size, false); // Валидация не проводить - в базе уже храниться хорошо
						if (l_target.empty())
						{
							dcassert(0);
							CFlyServerJSON::pushError(28, "Error CFlylinkDBManager::load_queue l_tgt = " + l_tgt);
							continue;
						}
					}
					catch (const Exception& e)
					{
						l_bad_targets.push_back(l_q.getint64(0));
						LogManager::message("SQLite - load_queue[1]: " + l_tgt + e.getError(), true);
						continue;
					}
#endif
					const QueueItem::Priority l_p = QueueItem::Priority(l_q.getint(3));
					time_t l_added = static_cast<time_t>(l_q.getint64(5));
					if (l_added == 0)
						l_added = GET_TIME();
					TTHValue l_tthRoot;
					if (!l_q.getblob(6, l_tthRoot.data, 24))
					{
						dcassert(0);
						continue;
					}
#ifdef _DEBUG
					if (0)
					{
						const string l_tth_check = l_tthRoot.toBase32();
						if (l_cache_duplicate_tth.find(l_tth_check) == l_cache_duplicate_tth.end())
						{
							m_select_transfer_tth.init(m_flySQLiteDB,
							                           "select distinct path,size from transfer_db.fly_transfer_file where TTH=? and path<>? limit 1");
							m_select_transfer_tth->bind(1, l_tth_check, SQLITE_STATIC);
							m_select_transfer_tth->bind(2, l_target, SQLITE_STATIC);
							sqlite3_reader l_q_tth = m_select_transfer_tth->executereader();
							int l_count_download_tth = 0;
							while (l_q_tth.read())
							{
								l_cache_duplicate_tth.insert(l_tthRoot.toBase32());
								l_count_download_tth++;
								LogManager::message("Already download [" + Util::toString(l_count_download_tth) +
								                    "] TTH = " + l_tthRoot.toBase32() + " Path = " + l_q_tth.getstring(0) + " size = " + Util::toString(l_q_tth.getint64(1)));
								//l_q_tth.getstring(0),
								//l_q.getint64(1),
							}
						}
					}
#endif
					//if(tthRoot.empty()) ?
					//  continue;
					string l_tempTarget = l_q.getstring(7);
					if (l_tempTarget.length() >= MAX_PATH)
					{
						const auto i = l_tempTarget.rfind(PATH_SEPARATOR);
						if (l_tempTarget.length() - i >= MAX_PATH || i == string::npos) // Имя файла больше MAX_PATH - обрежем
						{
							string l_file_name = Util::getFileName(l_tempTarget);
							dcassert(l_file_name.length() >= MAX_PATH);
							if (l_file_name.length() >= MAX_PATH)
							{
								const string l_file_path = Util::getFilePath(l_tempTarget);
								Util::fixFileNameMaxPathLimit(l_file_name);
								l_tempTarget = l_file_path + l_file_name;
							}
						}
					}
					const uint8_t l_maxSegments = uint8_t(l_q.getint(9));
					const __int64 l_ID = l_q.getint64(0);
					m_queue_id = std::max(m_queue_id, l_ID);
					QueueItemPtr qi = QueueManager::FileQueue::find_target(l_target); //TODO после отказа от конвертации XML варианта очереди можно удалить
					if (!qi)
					{
						qi = QueueManager::g_fileQueue.add(l_ID, l_target, l_size, Flags::MaskType(l_flags), l_p,
						                                   l_q.getint(8) != 0,
						                                   l_tempTarget,
						                                   l_added, l_tthRoot, max((uint8_t)1, l_maxSegments));
						if (qi) // Возможны дубли
						{
							dcassert(qi->isDirtyAll() == false);
							qi->setDirty(false);
							l_qitem.push_back(qi);
						}
						else
						{
#ifdef  _DEBUG
							LogManager::message("Skip QueueManager::g_fileQueue.add - l_target = " + l_target);
#endif //  _DEBUG
						}
					}
					if (qi)
					{
						// [+] brain-ripper
						qi->setSectionString(l_q.getstring(4), true);
						const auto l_source_items = l_sources_map.find(l_ID);
						if (l_source_items != l_sources_map.end())
						{
							// TODO - возможно появление дублей
							for (auto i = l_source_items->second.cbegin(); i != l_source_items->second.cend(); ++i)
							{
								add_sourceL(qi, i->m_CID, i->m_nick, i->m_hub_id); //
							}
						}
						else
						{
							//dcassert(0);
						}
						qi->calcAverageSpeedAndCalcAndGetDownloadedBytesL();
						qi->resetDirtyAll();
					}
				}
				if (g_count_queue_source)
				{
					l_q_files_log.step("Items:" + Util::toString(g_count_queue_source));
				}
			}
			// if (!l_sources_map.empty())
			{
				// dcassert(l_sources_map.empty());
				// Удаление пока не делаем
				//for (auto i = l_sources_map.cbegin(); i != l_sources_map.cend(); ++i)
				//{
				//  delete_queue_sourcesL(i->first);
				//}
			}
			{
				// TODO - есть отдельный метод
				CFlyLock(m_cs);
				sqlite3_transaction l_trans(m_flySQLiteDB, l_bad_targets.size() > 1);
				for (auto i = l_bad_targets.cbegin(); i != l_bad_targets.cend(); ++i)
				{
					remove_queue_itemL(*i);
				}
				l_trans.commit();
			}
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - load_queue: " + e.getError());
		}
	}
	if (!l_qitem.empty())
	{
		QueueManager::getInstance()->fly_fire1(QueueManagerListener::AddedArray(), l_qitem);
	}
	return g_count_queue_source;
}
//========================================================================================================
void CFlylinkDBManager::add_sourceL(const QueueItemPtr& p_QueueItem, const CID& p_cid, const string& p_nick, int p_hub_id)
{
	dcassert(!p_nick.empty());
	dcassert(!p_cid.isZero());
	if (!p_cid.isZero())
	{
		UserPtr l_user = ClientManager::createUser(p_cid, p_nick, p_hub_id); // Создаем юзера в любом случае - http://www.flylinkdc.ru/2012/09/flylinkdc-r502-beta59.html
		bool wantConnection = false;
		try
		{
			CFlyLock(*QueueItem::g_cs); // [+] IRainman fix.
			//TODO- LOCK ??      QueueManager::LockFileQueueShared l_fileQueue; //[+]PPA
			wantConnection = QueueManager::addSourceL(p_QueueItem, l_user, 0, true) && l_user->isOnline(); // Добавить флаг ускоренной загрузки первый раз.
			g_count_queue_source++;
		}
		catch (const Exception&)
		{
			dcassert(0);
			//LogManager::message("CFlylinkDBManager::add_sourceL, Error = " + e.getError(), true);
		}
		if (wantConnection)
		{
			QueueManager::get_download_connection(l_user);
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
void CFlylinkDBManager::delete_queue_sourcesL(const __int64 p_id)
{
	dcassert(p_id);
	m_del_fly_queue_source.init(m_flySQLiteDB, "delete from fly_queue_source where fly_queue_id=?");
	// TODO - сделать апдейт поля-флажка + фильтр + фоновая зачистка
	m_del_fly_queue_source->bind(1, p_id);
	m_del_fly_queue_source->executenonquery();
}
//========================================================================================================
void CFlylinkDBManager::remove_queue_all_items()
{
	CFlyLock(m_cs);
	{
		unique_ptr<sqlite3_command> l_delete_all_fly_queue_src(new sqlite3_command(m_flySQLiteDB,
		                                                                           "delete from fly_queue_source"));
		l_delete_all_fly_queue_src->executenonquery();
	}
	{
		unique_ptr<sqlite3_command> l_delete_all_fly_queue(new sqlite3_command(m_flySQLiteDB,
		                                                                       "delete from fly_queue"));
		l_delete_all_fly_queue->executenonquery();
	}
}
//========================================================================================================
void CFlylinkDBManager::remove_queue_item_array(const std::vector<int64_t>& p_id_array)
{
	CFlyLock(m_cs);
	sqlite3_transaction l_trans(m_flySQLiteDB, p_id_array.size() > 1);
	for (auto i = p_id_array.cbegin(); i != p_id_array.cend(); ++i)
	{
		remove_queue_itemL(*i);
	}
	l_trans.commit();
}
//========================================================================================================
void CFlylinkDBManager::remove_queue_item_sourcesL(const __int64 p_id, const CID& p_cid)
{
	m_del_fly_queue_source_cid.init(m_flySQLiteDB, "delete from fly_queue_source where fly_queue_id=? and cid=?");
	m_del_fly_queue_source_cid->bind(1, p_id);
	m_del_fly_queue_source_cid->bind(2, p_cid.data(), 24, SQLITE_TRANSIENT);
	m_del_fly_queue_source_cid->executenonquery();
}
//========================================================================================================
void CFlylinkDBManager::remove_queue_itemL(const __int64 p_id)
{
	dcassert(p_id);
	try
	{
		delete_queue_sourcesL(p_id);
		m_del_fly_queue.init(m_flySQLiteDB, "delete from fly_queue where id=?");
		m_del_fly_queue->bind(1, p_id);
		m_del_fly_queue->executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - remove_queue_itemL: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::merge_queue_all_segments(const CFlySegmentArray& p_QueueSegmentArray)
{
	dcassert(!p_QueueSegmentArray.empty());
	try
	{
		CFlyLock(m_cs);
		sqlite3_transaction l_trans(m_flySQLiteDB, p_QueueSegmentArray.size() > 1);
		for (auto i = p_QueueSegmentArray.cbegin(); i != p_QueueSegmentArray.cend(); ++i)
		{
			merge_queue_segmentL(*i);
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_queue_all_segments: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::merge_queue_segmentL(const CFlySegment& p_QueueSegment)
{
	try
	{
		m_update_segments_fly_queue.init(m_flySQLiteDB, "update fly_queue set Priority=?, Sections=? where id=?");
		m_update_segments_fly_queue->bind(1, p_QueueSegment.m_priority);
		m_update_segments_fly_queue->bind(2, p_QueueSegment.m_segment, SQLITE_STATIC);
		m_update_segments_fly_queue->bind(3, p_QueueSegment.m_id);
		m_update_segments_fly_queue->executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_queue_segmentL: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::merge_queue_all_items(std::vector<QueueItemPtr>& p_QueueItemArray)
{
	dcassert(!p_QueueItemArray.empty());
	try
	{
		CFlyLock(m_cs);
		sqlite3_transaction l_trans(m_flySQLiteDB, p_QueueItemArray.size() > 1);
		for (auto i = p_QueueItemArray.begin(); i != p_QueueItemArray.end(); ++i)
		{
			if (merge_queue_itemL(*i))
			{
				(*i)->resetDirtyAll();
			}
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_queue_all_items: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::merge_queue_sub_itemsL(QueueItemPtr& p_QueueItem, __int64 p_id)
{
	try
	{
		const auto &l_sources = p_QueueItem->getSourcesL();
		if (l_sources.size())
		{
			m_insert_fly_queue_source.init(m_flySQLiteDB, "insert into fly_queue_source(fly_queue_id,CID,Nick,dic_hub) values(?,?,?,?)");
			sqlite3_command* l_sql_source = m_insert_fly_queue_source.get_sql();
			for (auto j = l_sources.cbegin(); j != l_sources.cend(); ++j)
			{
				const auto &l_user = j->first;
				if (j->second.isSet(QueueItem::Source::FLAG_PARTIAL))
					continue;
				const auto &l_cid = l_user->getCID();
				dcassert(!l_cid.isZero());
				if (l_cid.isZero())
				{
#ifdef _DEBUG
					//                      LogManager::message("[CFlylinkDBManager::merge_queue_sub_itemsL] l_cid.isZero() - skip! insert into fly_queue_source CID = "
					//                                                         + l_cid.toBase32() + " nick = " + l_user->getLastNick(),true
					//                                                        );
					//
#endif
					continue;
				}
				l_sql_source->bind(1, p_id);
				dcassert(!l_user->getLastNick().empty());
				//dcassert(!j->getUser().hint.empty());
				l_sql_source->bind(2, l_cid.data(), 24, SQLITE_TRANSIENT);
				l_sql_source->bind(3, l_user->getLastNick(), SQLITE_TRANSIENT);
				l_sql_source->bind(4, l_user->getHubID());
				l_sql_source->executenonquery(); // TODO - копипаст
#ifdef _DEBUG
				//                  LogManager::message("[CFlylinkDBManager::merge_queue_itemL] insert into fly_queue_source CID = "
				//                                                     + l_cid.toBase32() + " nick = " + l_user->getLastNick(),true
				//                                                    );
				//
#endif
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_queue_sub_itemsL: " + e.getError());
	}
}
//========================================================================================================
bool CFlylinkDBManager::merge_queue_itemL(QueueItemPtr& p_QueueItem)
{
	static unsigned g_count_sync_db = 0;
	
	try
	{
		bool l_is_new_id = false;
		__int64 l_id = p_QueueItem->getFlyQueueID();
		if (!l_id)
		{
			l_id = ++m_queue_id;
			l_is_new_id = true;
		}
		else
		{
			if (p_QueueItem->isDirtySource())
			{
				// Источники писали в базу - есть что удалять?
#ifdef _DEBUG
//				LogManager::message("delete_queue_sourcesL(l_id) l_id = " + Util::toString(l_id),true);
#endif
				delete_queue_sourcesL(l_id);
			}
		}
		if (
		    p_QueueItem->getFlyQueueID() &&
		    p_QueueItem->isDirtySegment() == true &&
		    p_QueueItem->isDirtyBase() == false)
		{
			const CFlySegment l_QueueSegment(p_QueueItem);
			merge_queue_segmentL(l_QueueSegment);
			if (p_QueueItem->isDirtySource())
			{
				merge_queue_sub_itemsL(p_QueueItem, l_id);
				p_QueueItem->setDirtySource(false);
			}
		}
		else
		{
			bool l_is_need_update = true;
#ifdef FLYLINKDC_BETA
			
			if (l_is_new_id == false && p_QueueItem->isDirtyBase() == true)
			{
				m_select_for_update_fly_queue.init(m_flySQLiteDB, "select "
				                                   "id,"
				                                   "Target,"
				                                   "Size,"
				                                   "Priority,"
				                                   "Sections,"
				                                   "Added,"
				                                   "TTH,"
				                                   "TempTarget,"
				                                   "AutoPriority,"
				                                   "MaxSegments"
				                                   " from fly_queue where id=?");
				m_select_for_update_fly_queue->bind(1, l_id);
				sqlite3_reader l_q = m_select_for_update_fly_queue->executereader();
				if (l_q.read())
				{
					const int64_t l_size = l_q.getint64(2);
					const string l_target = l_q.getstring(1);
					const QueueItem::Priority l_p = QueueItem::Priority(l_q.getint(3));
					const time_t l_added = static_cast<time_t>(l_q.getint64(5));
					TTHValue l_tthRoot;
					if (!l_q.getblob(6, l_tthRoot.data, 24))
					{
						dcassert(0);
					}
					string l_tempTarget = l_q.getstring(7);
					const uint8_t l_maxSegments = uint8_t(l_q.getint(9));
					const int l_auto_priority = l_q.getint(8);
					const string l_sections = l_q.getstring(4);
					//if ()
					{
						string l_log_base = "Update queue_item id = " + Util::toString(l_id) + " count = " + Util::toString(++g_count_sync_db) + " Target = " + p_QueueItem->getTarget();
						string l_item;
						if (l_target != p_QueueItem->getTarget())
							l_item += "[ target old = " + l_target + " new = " + p_QueueItem->getTarget() + "]";
						if (l_size != p_QueueItem->getSize())
							l_item += "[ size old = " + Util::toString(l_size) + " new = " + Util::toString(p_QueueItem->getSize()) + "]";
						if (bool(l_auto_priority) != p_QueueItem->getAutoPriority())
							l_item += "[ auto_priority old = " + Util::toString(l_auto_priority) + " new = " + Util::toString(p_QueueItem->getAutoPriority()) + "]";
						if (l_p != p_QueueItem->getPriority())
							l_item += "[ priority old = " + Util::toString(l_p) + " new = " + Util::toString(p_QueueItem->getPriority()) + "]";
						if (l_added != p_QueueItem->getAdded())
							l_item += "[ added old = " + Util::toString(l_added) + " new = " + Util::toString(p_QueueItem->getAdded()) + "]";
						if (l_maxSegments != p_QueueItem->getMaxSegments())
							l_item += "[ maxSegments old = " + Util::toString(l_maxSegments) + " new = " + Util::toString(p_QueueItem->getMaxSegments()) + "]";
						if (l_sections != p_QueueItem->getSectionString())
							l_item += "[ sections old = " + l_sections + " new = " + p_QueueItem->getSectionString() + "]";
						const string l_new_tempTarget = p_QueueItem->getDownloadedBytes() > 0 ? p_QueueItem->getTempTargetConst() : Util::emptyString;
						if (l_tempTarget != l_new_tempTarget)
							l_item += "[ tempTarget old = " + l_tempTarget + " new = " + l_new_tempTarget + "]";
						const string l_new_tth = p_QueueItem->getTTH().toBase32();
						if (l_tthRoot.toBase32() != l_new_tth)
							l_item += "[ TTH old = " + l_tthRoot.toBase32() + " new = " + l_new_tth + "]";
						if (!l_item.empty())
						{
							LogManager::message(l_log_base + l_item);
						}
						else
						{
							//dcassert(0);
							LogManager::message("[!] Skip duplicate " + l_log_base);
							l_is_need_update = false;
						}
					}
				}
				else
				{
					//dcassert(0);
				}
			}
#endif // FLYLINKDC_BETA
			if (l_is_need_update)
			{
				auto bind_values = [](sqlite3_command * p_sql, const QueueItemPtr & p_qitem, __int64 p_id)
				{
					p_sql->bind(1, p_qitem->getTarget(), SQLITE_TRANSIENT);
					p_sql->bind(2, p_qitem->getSize());
					p_sql->bind(3, int(p_qitem->getPriority()));
					p_sql->bind(4, p_qitem->getSectionString(), SQLITE_TRANSIENT);
					p_sql->bind(5, p_qitem->getAdded());
					p_sql->bind(6, p_qitem->getTTH().data, 24, SQLITE_TRANSIENT);
					p_sql->bind(7, p_qitem->getDownloadedBytes() > 0 ? p_qitem->getTempTargetConst() : Util::emptyString, SQLITE_TRANSIENT);
					p_sql->bind(8, p_qitem->getAutoPriority());
					p_sql->bind(9, p_qitem->getMaxSegments());
					p_sql->bind(10, p_id);
				};
				
				if (l_is_new_id == false)
				{
					m_update_and_full_update_fly_queue.init(m_flySQLiteDB,
					                                        "update fly_queue set "
					                                        "Target=?,"
					                                        "Size=?,"
					                                        "Priority=?,"
					                                        "Sections=?,"
					                                        "Added=?,"
					                                        "TTH=?,"
					                                        "TempTarget=?,"
					                                        "AutoPriority=?,"
					                                        "MaxSegments=? where id=?");
					bind_values(m_update_and_full_update_fly_queue.get_sql(), p_QueueItem, l_id);
					m_update_and_full_update_fly_queue->executenonquery();
				}
				if (l_is_new_id == true || m_update_and_full_update_fly_queue.sqlite3_changes() == 0)
				{
					m_insert_and_full_update_fly_queue.init(m_flySQLiteDB,
					                                        "insert or replace into fly_queue ("
					                                        "Target,"
					                                        "Size,"
					                                        "Priority,"
					                                        "Sections,"
					                                        "Added,"
					                                        "TTH,"
					                                        "TempTarget,"
					                                        "AutoPriority,"
					                                        "MaxSegments,"
					                                        "id"
					                                        ") values(?,?,?,?,?,?,?,?,?,?)");
					bind_values(m_insert_and_full_update_fly_queue.get_sql(), p_QueueItem, l_id);
					m_insert_and_full_update_fly_queue->executenonquery();
				}
#ifdef _DEBUG
				//          LogManager::message("insert or replace into fly_queue! l_id = "  + Util::toString(l_id)
				//                                             + " l_count_total_source = " + Util::toString(l_count_total_source),true);
#endif
			}
			if (p_QueueItem->isDirtySource())
			{
				merge_queue_sub_itemsL(p_QueueItem, l_id);
				p_QueueItem->setDirtySource(false);
			}
			p_QueueItem->setFlyQueueID(l_id);
		}
		return true;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_queue_item: " + e.getError());
	}
	return false;
}
//========================================================================================================
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
void CFlylinkDBManager::load_global_ratio()
{
	try
	{
		CFlyLock(m_cs);
		std::unique_ptr<sqlite3_command> l_select_global_ratio_load(new sqlite3_command(m_flySQLiteDB, "select total(upload),total(download) from fly_ratio"));
		// http://www.sqlite.org/lang_aggfunc.html
		// Sum() will throw an "integer overflow" exception if all inputs are integers or NULL and an integer overflow occurs at any point during the computation.
		// Total() never throws an integer overflow.
		sqlite3_reader l_q = l_select_global_ratio_load.get()->executereader();
		if (l_q.read())
		{
			m_global_ratio.set_upload(l_q.getdouble(0));
			m_global_ratio.set_download(l_q.getdouble(1));
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
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
//========================================================================================================
void CFlylinkDBManager::clear_virus_cacheL()
{
	CFlyWriteLock(*m_virus_cs);
	m_virus_user.clear();
	m_virus_share.clear();
	m_virus_ip4.clear();
}
//========================================================================================================
void CFlylinkDBManager::load_avdb()
{
	if (BOOLSETTING(AUTOUPDATE_ANTIVIRUS_DB) && !CFlyServerConfig::g_antivirus_db_url.empty())
	{
		try
		{
			CFlyLock(m_cs);
			clear_virus_cacheL();
			{
				CFlyWriteLock(*m_virus_cs);
				std::unique_ptr<sqlite3_command> l_select_antivirus_db(new sqlite3_command(m_flySQLiteDB,
				                                                                           "select nick,ip4,share from antivirus_db.fly_suspect_user"));
				sqlite3_reader l_q = l_select_antivirus_db.get()->executereader();
				while (l_q.read())
				{
					m_virus_user.insert(l_q.getstring(0));
					const unsigned long l_ip = l_q.getint64(1);
					dcassert(l_ip);
					m_virus_ip4.insert(l_ip);
					dcassert(l_q.getint64(2));
					m_virus_share.insert(l_q.getint64(2));
				}
			}
			dcassert(!m_virus_user.empty());
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - load_avdb: " + e.getError());
		}
	}
}
#endif
//========================================================================================================
bool CFlylinkDBManager::load_last_ip_and_user_stat(uint32_t p_hub_id, const string& p_nick, uint32_t& p_message_count, boost::asio::ip::address_v4& p_last_ip)
{
	dcassert(BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER));
	//CFlyLock(m_cs);
	try
	{
		p_message_count = 0;
#ifdef FLYLINKDC_USE_LASTIP_CACHE
		CFlyFastLock(m_last_ip_cs);
		auto l_find_cache_item = m_last_ip_cache.find(p_hub_id);
		if (l_find_cache_item == m_last_ip_cache.end()) // Хаб первый раз? (TODO - добавить задержку на кол-во запросов. если больше N - выполнить пакетную загрузку)
		{
			auto& l_cache_item = m_last_ip_cache[p_hub_id];
			m_select_all_last_ip_and_message_count.init(m_flySQLiteDB,
			                                            "select last_ip,message_count,nick"
			                                            //"(select flag_index ||'~'|| location from location_db.fly_location_ip where start_ip <= last_ip and stop_ip > last_ip limit 1) location_id,\n"
			                                            //"(select flag_index ||'~'|| country from location_db.fly_country_ip where start_ip <= last_ip and stop_ip > last_ip) country_id\n"
			                                            " from user_db.user_info where dic_hub=?");
			m_select_all_last_ip_and_message_count->bind(1, p_hub_id);
			sqlite3_reader l_q = m_select_all_last_ip_and_message_count->executereader();
			while (l_q.read())
			{
				CFlyLastIPCacheItem l_item;
				l_item.m_last_ip = boost::asio::ip::address_v4((unsigned long)l_q.getint64(0));
				l_item.m_message_count = l_q.getint64(1);
				l_cache_item.insert(std::make_pair(l_q.getstring(2), l_item));
			}
		}
		auto& l_hub_cache = m_last_ip_cache[p_hub_id]; // TODO - убрать лишний поиск. его нашли уже выше
		const auto& l_cache_nick_item = l_hub_cache.find(p_nick);
		if (l_cache_nick_item != l_hub_cache.end())
		{
			p_message_count = l_cache_nick_item->second.m_message_count;
			p_last_ip = l_cache_nick_item->second.m_last_ip;
			return true;
		}
#else
		m_select_last_ip_and_message_count.init(m_flySQLiteDB,
		                                        "select last_ip,message_count from user_db.user_info where nick=? and dic_hub=?");
		sqlite3_command* l_sql_command = m_select_last_ip_and_message_count.get_sql();
		l_sql_command->bind(1, p_nick, SQLITE_STATIC);
		l_sql_command->bind(2, p_hub_id);
		sqlite3_reader l_q = l_sql_command->executereader();
		if (l_q.read())
		{
			p_last_ip = boost::asio::ip::address_v4((unsigned long)l_q.getint64(0));
			p_message_count = l_q.getint64(1);
			return true;
		}
		
#endif // FLYLINKDC_USE_LASTIP_CACHE
	}
	catch (const database_error& e)
	{
		// errorDB("SQLite - load_last_ip_and_user_stat: " + e.getError());
		dcassert(0);
		LogManager::message("SQLite - load_last_ip_and_user_stat: " + e.getError());
	}
	return false;
}
//========================================================================================================
bool CFlylinkDBManager::load_ratio(uint32_t p_hub_id, const string& p_nick, CFlyUserRatioInfo& p_ratio_info, const boost::asio::ip::address_v4& p_last_ip)
{
	/*
	sqlite> select dic_hub,upload,download,(select name from fly_dic where id = dic_ip) from fly_ratio where dic_nick=(select id from fly_dic where name='FlylinkDC-dev' and dic=2) and dic_hub=789612;
	--EQP-- 0,0,0,SEARCH TABLE fly_ratio USING INDEX iu_fly_ratio (dic_nick=? AND dic_hub=?)
	--EQP-- 0,0,0,EXECUTE SCALAR SUBQUERY 1
	--EQP-- 1,0,0,SEARCH TABLE fly_dic USING COVERING INDEX iu_fly_dic_name (name=? AND dic=?)
	--EQP-- 0,0,0,EXECUTE CORRELATED SCALAR SUBQUERY 2
	--EQP-- 2,0,0,SEARCH TABLE fly_dic USING INTEGER PRIMARY KEY (rowid=?)
	789612|1740114051|0|185.90.227.251
	Memory Used:                         512608 (max 518096) bytes
	Number of Outstanding Allocations:   339 (max 350)
	Number of Pcache Overflow Bytes:     4096 (max 4096) bytes
	Number of Scratch Overflow Bytes:    0 (max 0) bytes
	Largest Allocation:                  425600 bytes
	Largest Pcache Allocation:           4096 bytes
	Largest Scratch Allocation:          0 bytes
	Lookaside Slots Used:                17 (max 78)
	Successful lookaside attempts:       335
	Lookaside failures due to size:      92
	Lookaside failures due to OOM:       0
	Pager Heap Usage:                    72624 bytes
	Page cache hits:                     16
	Page cache misses:                   16
	Page cache writes:                   0
	Schema Heap Usage:                   13392 bytes
	Statement Heap/Lookaside Usage:      4008 bytes
	Fullscan Steps:                      0
	Sort Operations:                     0
	Autoindex Inserts:                   0
	Virtual Machine Steps:               42
	
	
	After vacuum
	
	Memory Used:                         512608 (max 518592) bytes
	Number of Outstanding Allocations:   339 (max 390)
	Number of Pcache Overflow Bytes:     4096 (max 4096) bytes
	Number of Scratch Overflow Bytes:    0 (max 0) bytes
	Largest Allocation:                  425600 bytes
	Largest Pcache Allocation:           4096 bytes
	Largest Scratch Allocation:          0 bytes
	Lookaside Slots Used:                15 (max 71)
	Successful lookaside attempts:       107
	Lookaside failures due to size:      53
	Lookaside failures due to OOM:       0
	Pager Heap Usage:                    68380 bytes
	Page cache hits:                     2
	Page cache misses:                   15
	Page cache writes:                   0
	Schema Heap Usage:                   13392 bytes
	Statement Heap/Lookaside Usage:      4008 bytes
	Fullscan Steps:                      0
	Sort Operations:                     0
	Autoindex Inserts:                   0
	Virtual Machine Steps:               42
	
	*/
	dcassert(BOOLSETTING(ENABLE_RATIO_USER_LIST));
	dcassert(p_hub_id != 0);
	dcassert(!p_nick.empty());
	bool l_res = false;
	try
	{
		if (!p_last_ip.is_unspecified()) // Если нет в таблице user_db.user_info, в fly_ratio можно не ходить - там ничего нет
		{
			CFlyLock(m_cs); // TODO - убирать нельзя падаем https://drdump.com/Problem.aspx?ProblemID=118720
			m_select_ratio_load.init(m_flySQLiteDB, "select upload,download,(select name from fly_dic where id = dic_ip) " // TODO перевести на хранение IP как числа?
			                         "from fly_ratio where dic_nick=(select id from fly_dic where name=? and dic=2) and dic_hub=? ");
			m_select_ratio_load->bind(1, p_nick, SQLITE_STATIC);
			m_select_ratio_load->bind(2, p_hub_id);
			sqlite3_reader l_q = m_select_ratio_load->executereader();
			string l_ip_from_ratio;
			while (l_q.read())
			{
				l_ip_from_ratio = l_q.getstring(2);
				// dcassert(!l_ip_from_ratio.empty()); // TODO - сделать зачистку таких
				if (!l_ip_from_ratio.empty())
				{
					boost::system::error_code ec;
					const auto l_ip = boost::asio::ip::address_v4::from_string(l_ip_from_ratio, ec);
					dcassert(!ec);
					if (!l_ip_from_ratio.empty())
					{
						const auto l_u = l_q.getint64(0);
						const auto l_d = l_q.getint64(1);
						dcassert(l_d || l_u);
						p_ratio_info.addDownload(l_ip, l_d);
						p_ratio_info.addUpload(l_ip, l_u);
						l_res = true;
					}
				}
			}
			p_ratio_info.reset_dirty();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_ratio: " + e.getError());
	}
	return l_res;
}
//========================================================================================================
uint32_t CFlylinkDBManager::get_dic_hub_id(const string& p_hub)
{
	CFlyLock(m_cs);
	return get_dic_idL(p_hub, e_DIC_HUB, true);
}
//========================================================================================================
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
__int64 CFlylinkDBManager::get_dic_location_id(const string& p_location)
{
	CFlyLock(m_cs);
	return get_dic_idL(p_location, e_DIC_LOCATION, true);
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
	CFlyLock(m_cs);
	m_DIC[p_DIC - 1].clear();
}
#endif
//========================================================================================================
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
//========================================================================================================
void CFlylinkDBManager::store_all_ratio_internal(uint32_t p_hub_id, const __int64& p_dic_nick,
                                                 const __int64& p_ip,
                                                 const uint64_t& p_upload,
                                                 const uint64_t& p_download
                                                )
{
	m_update_ratio->bind(3, p_ip);
	m_update_ratio->bind(1, p_upload);
	m_update_ratio->bind(2, p_download);
	m_update_ratio->executenonquery();
	if (m_update_ratio.sqlite3_changes() == 0)
	{
		m_insert_ratio->bind(4, p_dic_nick);
		m_insert_ratio->bind(5, p_hub_id);
		m_insert_ratio->bind(3, p_ip);
		m_insert_ratio->bind(1, p_upload);
		m_insert_ratio->bind(2, p_download);
		m_insert_ratio->executenonquery();
	}
}
//========================================================================================================
void CFlylinkDBManager::store_all_ratio_and_last_ip(uint32_t p_hub_id,
                                                    const string& p_nick,
                                                    CFlyUserRatioInfo& p_user_ratio,
                                                    const uint32_t p_message_count,
                                                    const boost::asio::ip::address_v4& p_last_ip,
                                                    bool p_is_last_ip_dirty,
                                                    bool p_is_message_count_dirty,
                                                    bool& p_is_sql_not_found)
{
	dcassert(BOOLSETTING(ENABLE_RATIO_USER_LIST));
	CFlyLock(m_cs);
	try
	{
		dcassert(p_hub_id);
		dcassert(!p_nick.empty());
		const bool l_is_exist_map = p_user_ratio.getUploadDownloadMap() && !p_user_ratio.getUploadDownloadMap()->empty();
		const __int64 l_dic_nick = get_dic_idL(p_nick, e_DIC_NICK, true);
		// Транзакции делать нельзя
		// sqlite3_transaction l_trans_insert(m_flySQLiteDB, p_user_ratio.getUploadDownloadMap()->size() > 1);
		m_update_ratio.init(m_flySQLiteDB, "update fly_ratio set upload=?,download=? where dic_ip=? and dic_nick=? and dic_hub=?");
		m_insert_ratio.init(m_flySQLiteDB, "insert or replace into fly_ratio(upload,download,dic_ip,dic_nick,dic_hub) values(?,?,?,?,?)");
		// TODO провести конвертацию в другой формат и файл БД + отказаться от DIC
		m_update_ratio->bind(4, l_dic_nick);
		m_update_ratio->bind(5, p_hub_id);
		__int64 l_last_ip_id = 0;
		if (l_is_exist_map)
		{
			for (auto i = p_user_ratio.getUploadDownloadMap()->begin(); i != p_user_ratio.getUploadDownloadMap()->end(); ++i)
			{
				l_last_ip_id = get_dic_idL(boost::asio::ip::address_v4(i->first).to_string(), e_DIC_IP, true);
				dcassert(i->second.get_upload() != 0 || i->second.get_download() != 0);
				if (l_last_ip_id &&  // Коннект еще не наступил - не пишем в базу 0
				        i->second.is_dirty() &&
				        (i->second.get_upload() != 0 || i->second.get_download() != 0)) // Если все по нулям - тоже странно
				{
					store_all_ratio_internal(p_hub_id, l_dic_nick, l_last_ip_id, i->second.get_upload(), i->second.get_download());
					i->second.reset_dirty();
				}
			}
		}
		if (p_user_ratio.is_dirty() && !p_user_ratio.m_ip.is_unspecified())
		{
			l_last_ip_id = get_dic_idL(p_user_ratio.m_ip.to_string(), e_DIC_IP, true);
			store_all_ratio_internal(p_hub_id, l_dic_nick, l_last_ip_id, p_user_ratio.get_upload(), p_user_ratio.get_download());
			p_user_ratio.reset_dirty();
		}
		// Иначе фиксируем только последний IP и cчетчик мессаг
		if (p_is_last_ip_dirty || p_is_message_count_dirty)
		{
			update_last_ip_deferredL(p_hub_id, p_nick, p_message_count, p_last_ip, p_is_sql_not_found,
			                         p_is_last_ip_dirty,
			                         p_is_message_count_dirty
			                        );
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - store_all_ratio_and_last_ip: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::update_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick,
                                                         const boost::asio::ip::address_v4& p_last_ip,
                                                         const uint32_t p_message_count,
                                                         bool& p_is_sql_not_found,
                                                         const bool p_is_last_ip_dirty,
                                                         const bool p_is_message_count_dirty
                                                        )
{
#ifndef FLYLINKDC_USE_LASTIP_CACHE
	CFlyLock(m_cs);
#endif
	try
	{
		update_last_ip_deferredL(p_hub_id, p_nick, p_message_count, p_last_ip, p_is_sql_not_found,
		                         p_is_last_ip_dirty,
		                         p_is_message_count_dirty);
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - update_last_ip_and_message_count: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::flush()
{
	flush_all_last_ip_and_message_count();
}
//========================================================================================================
void CFlylinkDBManager::flush_all_last_ip_and_message_count()
{
#ifdef FLYLINKDC_USE_LASTIP_CACHE
	CFlyLock(m_cs);
	try
	{
		CFlyFastLock(m_last_ip_cs);
		CFlyLogFile l_log("[sqlite - flush-user-info]");
		int l_count = 0;
		{
			sqlite3_transaction l_trans_insert(m_flySQLiteDB, m_last_ip_cache.size() > 1);
			for (auto h = m_last_ip_cache.begin(); h != m_last_ip_cache.end(); ++h)
			{
				for (auto i = h->second.begin(); i != h->second.end(); ++i)
				{
					if (i->second.m_is_item_dirty)
					{
#ifdef _DEBUG
						{
							// Проверим что данные не затираются
							m_check_message_count.init(m_flySQLiteDB,
							"select message_count from user_db.user_info where nick=? and dic_hub=?");
							sqlite3_command* l_sql_command = m_check_message_count.get();
							l_sql_command->bind(1, i->first, SQLITE_STATIC);
							l_sql_command->bind(2, h->first);
							sqlite3_reader l_q = l_sql_command->executereader();
							if (l_q.read())
							{
								const auto l_message_count = l_q.getint64(0);
								if (l_message_count > i->second.m_message_count)
								{
									dcassert(0);
									l_log.log("Error update message_count for user = " + i->first +
									" new_message_count = " + Util::toString(i->second.m_message_count) +
									" sqlite_message_count = " + Util::toString(l_message_count)
									         );
									// В базе оказалось знаение больше чем пишется - не затираем его нужно разбираться когда так получается
									i->second.m_is_item_dirty = false;
									continue;
								}
							}
						}
#endif
						++l_count;
						m_insert_store_all_ip_and_message_count.init(m_flySQLiteDB,
						                                             "insert or replace into user_db.user_info (nick,dic_hub,last_ip,message_count) values(?,?,?,?)");
						sqlite3_command* l_sql = m_insert_store_all_ip_and_message_count.get_sql();
						l_sql->bind(1, i->first, SQLITE_STATIC);
						l_sql->bind(2, h->first));
						l_sql->bind(3, i->second.m_last_ip.to_ulong());
						l_sql->bind(4, i->second.m_message_count);
						l_sql->executenonquery();
						i->second.m_is_item_dirty = false;
					}
				}
			}
			l_trans_insert.commit();
		}
		if (l_count)
		{
			l_log.log("Save dirty record user_db.user_info:" + Util::toString(l_count));
		}
		
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - flush_all_last_ip_and_message_count: " + e.getError());
	}
#endif // FLYLINKDC_USE_LASTIP_CACHE
}
//========================================================================================================
void CFlylinkDBManager::update_last_ip_deferredL(uint32_t p_hub_id, const string& p_nick,
                                                 uint32_t p_message_count,
                                                 boost::asio::ip::address_v4 p_last_ip,
                                                 bool& p_is_sql_not_found,
                                                 const bool p_is_last_ip_dirty,
                                                 const bool p_is_message_count_dirty
                                                )
{
	dcassert(BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER));
	
	dcassert(p_hub_id);
	dcassert(!p_nick.empty());
#ifdef FLYLINKDC_USE_LASTIP_CACHE
	CFlyFastLock(m_last_ip_cs);
#ifdef _DEBUG
	{
#if 0
		m_select_store_ip.init(m_flySQLiteDB,
		                       "select last_ip,message_count from user_db.user_info where nick=? and dic_hub=?");
		m_select_store_ip->bind(1, p_nick, SQLITE_STATIC);
		m_select_store_ip->bind(2, p_hub_id);
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
		m_insert_store_ip_and_message_count.init(m_flySQLiteDB,
		                                         "insert or replace into user_db.user_info (nick,dic_hub,last_ip,message_count) values(?,?,?,?)");
		sqlite3_command* l_sql = m_insert_store_ip_and_message_count.get();
		l_sql->bind(1, p_nick, SQLITE_STATIC);
		l_sql->bind(2, p_hub_id);
		l_sql->bind(3, p_last_ip.to_ulong());
		l_sql->bind(4, p_message_count);
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
			m_insert_store_ip.init(m_flySQLiteDB,
			                       "insert or replace into user_db.user_info (nick,dic_hub,last_ip) values(?,?,?)");
			sqlite3_command* l_sql = m_insert_store_ip.get();
			l_sql->bind(1, p_nick, SQLITE_STATIC);
			l_sql->bind(2, p_hub_id);
			l_sql->bind(3, p_last_ip.to_ulong());
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
			m_insert_store_message_count.init(m_flySQLiteDB,
			                                  "insert or replace into user_db.user_info (nick,dic_hub,message_count) values(?,?,?)");
			sqlite3_command* l_sql = m_insert_store_message_count.get();
			l_sql->bind(1, p_nick, SQLITE_STATIC);
			l_sql->bind(2, p_hub_id);
			l_sql->bind(3, p_message_count);
			l_sql->executenonquery();
#endif
			if (l_hub_cache_item.m_message_count != p_message_count)
			{
				l_hub_cache_item.m_is_item_dirty = true;
			}
			l_hub_cache_item.m_message_count = p_message_count;
		}
	}
#else
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
	if (p_message_count == 0 || p_last_ip.is_unspecified())
	{
		CFlyIPMessageCache l_old = m_IPCacheLevelDB.get_last_ip_and_message_count(p_hub_id, p_nick);
		if (p_message_count == 0)
			p_message_count = l_old.m_message_count;
		if (p_last_ip.is_unspecified())
			p_last_ip = boost::asio::ip::address_v4(l_old.m_ip);
	}
	m_IPCacheLevelDB.set_last_ip_and_message_count(p_hub_id, p_nick, p_message_count, p_last_ip);
#else
	
	if (!p_last_ip.is_unspecified() && p_message_count)
	{
		if (p_is_sql_not_found == false)
		{
			m_update_last_ip_and_message_count.init(m_flySQLiteDB,
			                                        "update user_db.user_info set last_ip=?,message_count=? where dic_hub=? and nick=?");
			m_update_last_ip_and_message_count->bind(1, p_last_ip.to_ulong());
			m_update_last_ip_and_message_count->bind(2, p_message_count);
			m_update_last_ip_and_message_count->bind(3, p_hub_id);
			m_update_last_ip_and_message_count->bind(4, p_nick, SQLITE_STATIC);
			m_update_last_ip_and_message_count->executenonquery();
		}
		if (p_is_sql_not_found == true || m_update_last_ip_and_message_count.sqlite3_changes() == 0)
		{
			m_insert_last_ip_and_message_count.init(m_flySQLiteDB,
			                                        "insert or replace into user_db.user_info(nick,dic_hub,last_ip,message_count) values(?,?,?,?)");
			m_insert_last_ip_and_message_count->bind(1, p_nick, SQLITE_STATIC);
			m_insert_last_ip_and_message_count->bind(2, p_hub_id);
			m_insert_last_ip_and_message_count->bind(3, p_last_ip.to_ulong());
			m_insert_last_ip_and_message_count->bind(4, p_message_count);
			m_insert_last_ip_and_message_count->executenonquery();
		}
		p_is_sql_not_found = false;
	}
	else
	{
		if (!p_last_ip.is_unspecified())
		{
			//LogManager::message("Update lastip p_nick = " + p_nick);
			if (p_is_sql_not_found == false)
			{
				m_update_last_ip.init(m_flySQLiteDB,
				                      "update user_db.user_info set last_ip=? where dic_hub=? and nick=?");
				m_update_last_ip->bind(1, p_last_ip.to_ulong());
				m_update_last_ip->bind(2, p_hub_id);
				m_update_last_ip->bind(3, p_nick, SQLITE_STATIC);
				m_update_last_ip->executenonquery();
			}
			if (p_is_sql_not_found == true || m_update_last_ip.sqlite3_changes() == 0)
			{
				boost::asio::ip::address_v4 l_ip_from_db;
				// Проверим наличие в базе записи
				{
					m_select_last_ip.init(m_flySQLiteDB,
					                      "select last_ip from user_db.user_info where nick=? and dic_hub=?");
					m_select_last_ip->bind(1, p_nick, SQLITE_STATIC);
					m_select_last_ip->bind(2, p_hub_id);
					sqlite3_reader l_q = m_select_last_ip->executereader();
					if (l_q.read())
					{
						l_ip_from_db = boost::asio::ip::address_v4((unsigned long)l_q.getint64(0));
						p_is_sql_not_found = false;
					}
					else
					{
					}
				}
				if (p_last_ip != l_ip_from_db)
				{
					m_insert_last_ip.init(m_flySQLiteDB,
					                      "insert or replace into user_db.user_info(nick,dic_hub,last_ip) values(?,?,?)");
					m_insert_last_ip->bind(1, p_nick, SQLITE_STATIC);
					m_insert_last_ip->bind(2, p_hub_id);
					m_insert_last_ip->bind(3, p_last_ip.to_ulong());
					m_insert_last_ip->executenonquery();
				}
				p_is_sql_not_found = false;
			}
		}
		else if (p_message_count)
		{
			if (p_is_sql_not_found == false)
			{
				m_update_message_count.init(m_flySQLiteDB,
				                            "update user_db.user_info set message_count=? where dic_hub=? and nick=?");
				m_update_message_count->bind(1, p_message_count);
				m_update_message_count->bind(2, p_hub_id);
				m_update_message_count->bind(3, p_nick, SQLITE_STATIC);
				m_update_message_count->executenonquery();
			}
			if (p_is_sql_not_found == true || m_update_message_count.sqlite3_changes() == 0)
			{
				m_insert_message_count.init(m_flySQLiteDB,
				                            "insert or replace into user_db.user_info(nick,dic_hub,message_count) values(?,?,?)");
				m_insert_message_count->bind(1, p_nick, SQLITE_STATIC);
				m_insert_message_count->bind(2, p_hub_id);
				m_insert_message_count->bind(3, p_message_count);
				m_insert_message_count->executenonquery();
			}
			p_is_sql_not_found = false;
		}
		else
		{
			dcassert(0);
		}
	}
#endif // FLYLINKDC_USE_IPCACHE_LEVELDB
	
#endif // FLYLINKDC_USE_LASTIP_CACHE
	
}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
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
	m_select_fly_dic.init(m_flySQLiteDB,
	                      "select id from fly_dic where name=? and dic=?");
	sqlite3_command* l_sql = m_select_fly_dic.get_sql();
	l_sql->bind(1, p_name, SQLITE_STATIC);
	l_sql->bind(2, p_DIC);
	__int64 l_dic_id = l_sql->executeint64_no_throw();
	if (l_dic_id && p_cache_result)
	{
		m_DIC[p_DIC - 1][p_name] = l_dic_id;
	}
	return l_dic_id;
}
//========================================================================================================
__int64 CFlylinkDBManager::get_dic_idL(const string& p_name, const eTypeDIC p_DIC, bool p_create)
{
#ifdef _DEBUG
	static int g_count = 0;
	LogManager::message("[" + Util::toString(++g_count) + "] get_dic_idL " + p_name + " type = " + Util::toString(p_DIC) + " is_create = " + Util::toString(p_create));
#endif
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
			m_insert_fly_dic.init(m_flySQLiteDB,
			                      "insert into fly_dic (dic,name) values(?,?)");
			sqlite3_command* l_sql = m_insert_fly_dic.get_sql();
			l_sql->bind(1, p_DIC);
			l_sql->bind(2, p_name, SQLITE_STATIC);
			l_sql->executenonquery();
			l_Cache_ID = m_flySQLiteDB.insertid();
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
			if (p_DIC == e_DIC_HUB)
			{
				CFlyFastLock(m_hub_dic_fcs);
				m_HubNameMap[l_Cache_ID] = p_name;
			}
#endif
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
#ifdef FLYLINKDC_USE_CACHE_HUB_URLS
string CFlylinkDBManager::get_hub_name(unsigned p_hub_id)
{
	CFlyFastLock(m_hub_dic_fcs);
	const string l_name = m_HubNameMap[p_hub_id];
	dcassert(!l_name.empty())
	return l_name;
}
#endif
//========================================================================================================
/*
void CFlylinkDBManager::IPLog(const string& p_hub_ip,
               const string& p_ip,
               const string& p_nick)
{
 CFlyLock(m_cs);
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
void CFlylinkDBManager::clean_fly_hash_blockL()
{
	CFlyLogFile l_log("HashTable cleanup");
	l_log.log(m_flySQLiteDB.executenonquery("delete from fly_hash_block where tth_id not in(select tth_id from fly_file)"));
	// TODO - Версия 2 delete from fly_hash_block where not exists (select 1 from fly_file ff where ff.tth_id = fly_hash_block.tth_id)
}
//========================================================================================================
size_t CFlylinkDBManager::get_count_folders()
{
	CFlyFastLock(m_path_cache_cs);
	return m_path_cache.size();
}
//========================================================================================================
void CFlylinkDBManager::sweep_db()
{
	CFlyLock(m_cs);
	try
	{
		{
			/*
			CFlyFastLock(m_path_cache_cs);
			sqlite3_transaction l_trans(m_flySQLiteDB, m_path_cache.size() > 1);
			for (auto i = m_path_cache.cbegin(); i != m_path_cache.cend(); ++i)
			{
			if (i->second.m_is_found == false)
			    {
			    m_sweep_path_file.init(m_flySQLiteDB,
			                           "delete from fly_file where dic_path=?");
			    m_sweep_path_file->bind(1, i->second.m_path_id);
			    m_sweep_path_file->executenonquery();
			    }
			}
			l_trans.commit();
			}
			*/
			// Зачищаем мусорок, который остался в файлах.
			{
				const char* l_clean_file = "delete from fly_file where not exists (select * from fly_hash_block fhb where fly_file.tth_id=fhb.tth_id)";
				CFlyLogFile l_log(l_clean_file);
				m_flySQLiteDB.executenonquery(l_clean_file);
			}
		}
		{
			const char* l_clean_path = "delete from fly_path where not exists (select * from fly_file where dic_path=fly_path.id)";
			CFlyLogFile l_log(l_clean_path);
			m_flySQLiteDB.executenonquery(l_clean_path);
			{
				CFlyFastLock(m_path_cache_cs);
				m_path_cache.clear();
			}
		}
		load_path_cache();
		{
			clean_fly_hash_blockL();
			clearTTHCache();
		}
		{
			const char* l_clean_sql_media = "delete from media_db.fly_media where tth_id not in(select tth_id from fly_file)";
			CFlyLogFile l_log(l_clean_sql_media);
			m_flySQLiteDB.executenonquery(l_clean_sql_media);
		}
#ifdef FLYLINKDC_USE_VACUUM
		LogManager::message("start vacuum", true);
		m_flySQLiteDB.executenonquery("VACUUM;");
		LogManager::message("stop vacuum", true);
#endif
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - sweep_db: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::prepare_scan_folder(const tstring& p_path)
{
	if (!ClientManager::isBeforeShutdown())
	{
		auto fData = std::make_unique<WIN32_FIND_DATA >();
		dcassert(p_path[p_path.size() - 1] == L'\\');
		HANDLE hFind = FindFirstFileEx(File::formatPath((p_path + _T('*'))).c_str(),
		                               CompatibilityManager::g_find_file_level,
		                               &(*fData),
		                               FindExSearchLimitToDirectories, // Only Folder
		                               NULL,
		                               CompatibilityManager::g_find_file_flags);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				const tstring l_folder_name = fData->cFileName;
				if ((fData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				        (l_folder_name != Util::m_dotT) &&
				        (l_folder_name != Util::m_dot_dotT))
				{
					if (fData->dwFileAttributes & (FILE_ATTRIBUTE_VIRTUAL | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_REPARSE_POINT))
						continue; // fix https://drdump.com/Problem.aspx?ProblemID=261004
					const tstring l_full_pathT = p_path + l_folder_name + _T("\\");
					const string l_full_path = Text::fromT(l_full_pathT);
					if (Util::checkForbidenFolders(l_full_path) == false)
					{
						const string l_lower_full_path = Text::toLower(l_full_path);
						bool l_is_not_exists = false;
						{
							CFlyFastLock(m_path_cache_cs);
							l_is_not_exists = m_path_cache.find(l_lower_full_path) == m_path_cache.end();
						}
						if (l_is_not_exists)
						{
							CFlyLock(m_cs);
							create_path_idL(l_lower_full_path, true);
						}
						prepare_scan_folder(l_full_pathT);
					}
					else
					{
						dcassert(0);
					}
				}
			}
			while (!ClientManager::isBeforeShutdown() && FindNextFile(hFind, &(*fData)));
			FindClose(hFind);
		}
	}
}
//========================================================================================================
void CFlylinkDBManager::scan_path(CFlyDirItemArray& p_directories)
{
	load_path_cache();
	if (!p_directories.empty())
	{
		CFlyLogFile log(STRING(SCAN_DIR));
		try
		{
			// Тут транзакцию нелья - работаем без блокировки
			// sqlite3_transaction l_trans(m_flySQLiteDB, p_directories.size() > 1);
			for (auto j = p_directories.begin(); j != p_directories.end(); ++j)
			{
				prepare_scan_folder(Text::toT(j->m_path)); // TODO - хранить сразу в tstring и отдавать туда ID каталога?
			}
			//l_trans.commit();
		}
		catch (const database_error& e)
		{
			errorDB("SQLite - scan_path: " + e.getError());
		}
	}
}
//========================================================================================================
void CFlylinkDBManager::load_path_cache()
{
	m_convert_ftype_stop_key = 0;
	
	/* Отрубаем загрузку всех каталогов пачкой - тупит на грязной базе
	    CFlyLogFile log(STRING(RELOAD_DIR));
	    // CFlyLock(m_cs); // попробуем еще раз отключить лок - хотя раньше тут падали https://drdump.com/Problem.aspx?ProblemID=118720
	    {
	        CFlyFastLock(m_path_cache_cs);
	        m_path_cache.clear();
	    }
	    try
	    {
	        m_load_path_cache.init(m_flySQLiteDB,
	                               "select id,name,(select count(*) from fly_file where dic_path = fly_path.id and (media_audio is not null or media_video is not null)) cnt_mediainfo from fly_path");
	        // Версия 2
	        // select id,name,(select 1 from fly_file where dic_path = fly_path.id and (media_audio is not null or media_video is not null) limit 1) cnt_mediainfo from fly_path
	        sqlite3_reader l_q = m_load_path_cache->executereader();
	        CFlyFastLock(m_path_cache_cs);
	        while (l_q.read())
	        {
	            if (ClientManager::isBeforeShutdown())
	                break;
	            m_path_cache.insert(std::make_pair(l_q.getstring(1), CFlyPathItem(l_q.getint64(0), false, l_q.getint(2) == 0)));
	        }
	    }
	    catch (const database_error& e)
	    {
	        errorDB("SQLite - load_path_cache: " + e.getError());
	        return;
	    }
	*/
	
}
//========================================================================================================
__int64 CFlylinkDBManager::get_path_id(string p_path, bool p_create, bool p_case_convet, bool& p_is_no_mediainfo, bool p_sweep_path)
{
	CFlyLock(m_cs);
	return get_path_idL(p_path, p_create, p_case_convet, p_is_no_mediainfo, p_sweep_path);
}
//========================================================================================================
__int64 CFlylinkDBManager::create_path_idL(const string& p_path, bool p_is_skip_dup_val_index)
{
	try
	{
		__int64 l_last_path_id = 0;
		{
			m_load_path_cache_one_dir.init(m_flySQLiteDB,
			                               "select id,name,(select count(*) from fly_file where dic_path = fly_path.id and (media_audio is not null or media_video is not null)) cnt_mediainfo from fly_path where name=?");
			m_load_path_cache_one_dir->bind(1, p_path, SQLITE_STATIC);
			sqlite3_reader l_q = m_load_path_cache_one_dir->executereader();
			if (l_q.read())
			{
				l_last_path_id = l_q.getint64(0);
				CFlyFastLock(m_path_cache_cs);
				m_path_cache.insert(std::make_pair(l_q.getstring(1), CFlyPathItem(l_q.getint64(0), false, l_q.getint(2) == 0)));
				return l_last_path_id;
			}
		}
		m_insert_fly_path.init(m_flySQLiteDB, "insert into fly_path (name) values(?)");
		m_insert_fly_path->bind(1, p_path, SQLITE_STATIC);
		m_insert_fly_path->executenonquery();
		l_last_path_id = m_flySQLiteDB.insertid();
		{
			CFlyFastLock(m_path_cache_cs);
			m_path_cache.insert(std::make_pair(p_path, CFlyPathItem(l_last_path_id, true, false)));
		}
		return l_last_path_id;
	}
	catch (const database_error& e)
	{
		if (p_is_skip_dup_val_index == false) // && e.getError() != "UNIQUE constraint failed: fly_path.name"
		{
			errorDB("SQLite - create_path_idL: " + e.getError());
			throw;
		}
	}
	return 0;
}
//========================================================================================================
__int64 CFlylinkDBManager::find_path_id(const string& p_path)
{
	// Этот запрос должен всегда возвращать пустоту
	m_get_path_id.init(m_flySQLiteDB, "select id from fly_path where name=?;");
	m_get_path_id->bind(1, p_path, SQLITE_STATIC);
	const __int64 l_last_path_id = m_get_path_id->executeint64_no_throw();
	return l_last_path_id;
}
//========================================================================================================
__int64 CFlylinkDBManager::get_path_idL(string p_path, bool p_create, bool p_case_convet, bool& p_is_no_mediainfo, bool p_sweep_path)
{
	p_is_no_mediainfo = false;
	if (m_last_path_id != 0 && m_last_path_id != -1 && p_path == m_last_path && p_sweep_path == false)
	{
		return m_last_path_id;
	}
	
	m_last_path = p_path;
	if (p_case_convet)
	{
		p_path = Text::toLower(p_path);
	}
	try
	{
		{
			CFlyFastLock(m_path_cache_cs);
			auto l_item_iter = m_path_cache.find(p_path);
			if (l_item_iter != m_path_cache.end())
			{
				l_item_iter->second.m_is_found = true;
				m_last_path_id = l_item_iter->second.m_path_id;
				p_is_no_mediainfo = l_item_iter->second.m_is_no_mediainfo;
				return m_last_path_id;
			}
		}
		m_last_path_id = find_path_id(p_path);
		if (m_last_path_id)
		{
			return m_last_path_id;
		}
		else if (p_create)
		{
			m_last_path_id = create_path_idL(p_path, false);
			return m_last_path_id;
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_path_idL: " + e.getError());
	}
	return 0;
}
//========================================================================================================
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
void CFlylinkDBManager::sweep_files(__int64 p_path_id, const CFlyDirMap& p_sweep_files)
{
	CFlyLock(m_cs);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB, p_sweep_files.size() > 1);
		for (auto i = p_sweep_files.cbegin(); i != p_sweep_files.cend(); ++i)
		{
			if (i->second.m_is_found == false)
			{
				m_sweep_dir_sql.init(m_flySQLiteDB,
				                     "delete from fly_file where dic_path=? and name=?");
				m_sweep_dir_sql->bind(1, p_path_id);
				m_sweep_dir_sql->bind(2, i->first);
				m_sweep_dir_sql->executenonquery();
			}
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - sweep_files: " + e.getError());
	}
}
#endif
//========================================================================================================
void CFlylinkDBManager::load_dir(__int64 p_path_id, CFlyDirMap& p_dir_map, bool p_is_no_mediainfo)
{
	try
	{
		sqlite3_command* l_sql;
		if (!p_is_no_mediainfo)
		{
			m_load_dir_sql.init(m_flySQLiteDB, "select size,stamp,tth,name,hit,stamp_share,ftype,bitrate,media_x,media_y,media_video,media_audio from fly_file ff,fly_hash_block fhb where "
			                    "ff.dic_path=? and ff.tth_id=fhb.tth_id");
			l_sql = m_load_dir_sql.get_sql();
		}
		else
		{
			m_load_dir_sql_without_mediainfo.init(m_flySQLiteDB, "select size,stamp,tth,name,hit,stamp_share,ftype from fly_file ff,fly_hash_block fhb where "
			                                      "ff.dic_path=? and ff.tth_id=fhb.tth_id");
			                                      
			l_sql = m_load_dir_sql_without_mediainfo.get_sql();
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
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
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
			const auto l_is_tth_ok = l_q.getblob(2, &l_info.m_tth, 24);
			dcassert(l_is_tth_ok);
			dcassert(l_info.m_tth != TTHValue());
		}
		// тормозит...
		if (l_calc_ftype && m_convert_ftype_stop_key < 200)
		{
			// TODO - вынести в отедльный метод
			CFlyLock(m_cs);
			m_set_ftype.init(m_flySQLiteDB, "update fly_file set ftype=? where name=? and dic_path=? and ftype=-1");
			sqlite3_transaction l_trans(m_flySQLiteDB, p_dir_map.size() > 1);
			const auto &l_set_ftype_get = m_set_ftype.get_sql(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_set_ftype.get()' expression repeatedly. cflylinkdbmanager.cpp 1992
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
		errorDB("SQLite - load_dir: " + e.getError());
	}
}
//========================================================================================================
void CFlylinkDBManager::update_file_infoL(const string& p_fname, __int64 p_path_id,
                                          int64_t p_Size, int64_t p_TimeStamp, __int64 p_tth_id)
{
	m_update_file.init(m_flySQLiteDB,
	                   "update fly_file set size=?,stamp=?,tth_id=?,stamp_share=? where name=? and dic_path=?;");
	const auto &l_update_file = m_update_file.get_sql(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_update_file.get()' expression repeatedly. cflylinkdbmanager.cpp 2021
	l_update_file->bind(1, p_Size);
	l_update_file->bind(2, p_TimeStamp);
	l_update_file->bind(3, p_tth_id);
	l_update_file->bind(4, int64_t(File::currentTime()));
	l_update_file->bind(5, p_fname, SQLITE_STATIC);
	l_update_file->bind(6, p_path_id);
	l_update_file->executenonquery();
}
//========================================================================================================
bool CFlylinkDBManager::check_tth(const string& p_fname, __int64 p_path_id,
                                  int64_t p_Size, int64_t p_TimeStamp, TTHValue& p_out_tth)
{
	CFlyLock(m_cs);
	try
	{
		m_check_tth_sql.init(m_flySQLiteDB,
		                     "select size,stamp,tth from fly_file ff,fly_hash_block fhb where "
		                     "fhb.tth_id=ff.tth_id and ff.name=? and ff.dic_path=?");
		// TODO - тут индекс по tth_id не мешается?
		// Протестировать и забрать tth из fly_hash_block подзапросом
		//
		dcassert(p_fname == Text::toLower(p_fname));
		m_check_tth_sql->bind(1, p_fname, SQLITE_STATIC);
		m_check_tth_sql->bind(2, p_path_id);
		sqlite3_reader l_q = m_check_tth_sql->executereader();
		if (l_q.read())
		{
			const int64_t l_size   = l_q.getint64(0);
			const int64_t l_stamp  = l_q.getint64(1);
			l_q.getblob(2, p_out_tth.data, 24);
			if (l_stamp != p_TimeStamp || l_size != p_Size)
			{
				l_q.close();
				update_file_infoL(p_fname, p_path_id, -1, -1, -1);
				return false;
			}
			return true; //-V612
		}
		return false;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - check_tth: " + e.getError());
	}
	return false;
}
//========================================================================================================
// [+] brain-ripper
unsigned __int64 CFlylinkDBManager::get_block_size_sql(const TTHValue& p_root, __int64 p_size)
{
	unsigned __int64 l_blocksize = 0;
	CFlyLock(m_cs);
	try
	{
		m_get_blocksize.init(m_flySQLiteDB, "select file_size,block_size from fly_hash_block where tth=?");
		m_get_blocksize->bind(1, p_root.data, 24, SQLITE_STATIC);
		sqlite3_reader l_q = m_get_blocksize->executereader();
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
		errorDB("SQLite - get_block_size_sql: " + e.getError());
	}
	l_blocksize = TigerTree::getMaxBlockSize(p_size);
	dcassert(l_blocksize);
	return l_blocksize;
}
//========================================================================================================
bool CFlylinkDBManager::get_tree(const TTHValue& p_root, TigerTree& p_tt, __int64& p_block_size)
{
	dcassert(p_root != TTHValue());
	p_block_size = 0;
	try
	{
		{
			CFlyFastLock(g_tth_cache_cs);
			const auto& l_cache_tt = g_tiger_tree_cache.find(p_root);
			if (l_cache_tt != g_tiger_tree_cache.end())
			{
#ifdef _DEBUG
				// LogManager::message("[!] Cache! bingo! CFlylinkDBManager::getTree TTH Root = " + p_root.toBase32());
#endif
				p_tt = l_cache_tt->second;
				return true;
			}
		}
		CFlyLock(m_cs); // TODO - ниже идут только селекты - может он не нужен
		m_get_tree.init(m_flySQLiteDB, "select tiger_tree,file_size,block_size from fly_hash_block where tth=?");
		m_get_tree->bind(1, p_root.data, 24, SQLITE_STATIC);
		sqlite3_reader l_q = m_get_tree->executereader();
		if (l_q.read())
		{
			const __int64 l_file_size = l_q.getint64(1);
			p_block_size = l_q.getint64(2);
			if (p_block_size == 0)
				p_block_size = TigerTree::getMaxBlockSize(l_file_size);
				
			if (l_file_size <= MIN_BLOCK_SIZE) // TODO - тут возможно этого делать нельзя.
			{
				CFlyFastLock(g_tth_cache_cs);
				p_tt = TigerTree(l_file_size, p_block_size, p_root);
				g_tiger_tree_cache.insert(make_pair(p_root, p_tt));
				dcassert(p_tt.getRoot() == p_root);
				const auto l_result = p_tt.getRoot() == p_root;
				return l_result;
			}
			vector<uint8_t> l_buf;
			l_q.getblob(0, l_buf);
			if (!l_buf.empty())
			{
				p_tt = TigerTree(l_file_size, p_block_size, &l_buf[0], l_buf.size());
				dcassert(p_tt.getRoot() == p_root);
				const auto l_result = p_tt.getRoot() == p_root;
				if (l_result)
				{
					CFlyFastLock(g_tth_cache_cs);
					if (g_tiger_tree_cache.size() > g_tth_cache_limit)
					{
						clear_and_reset_capacity(g_tiger_tree_cache);
					}
					g_tiger_tree_cache.insert(make_pair(p_root, p_tt));
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
		errorDB("SQLite - get_tree: " + e.getError());
	}
	return false;
}
//========================================================================================================
__int64 CFlylinkDBManager::get_tth_idL(const TTHValue& p_tth)
{
	try
	{
		m_get_tth_id.init(m_flySQLiteDB, "select tth_id from fly_hash_block where tth=?");
		m_get_tth_id->bind(1, p_tth.data, 24, SQLITE_STATIC);
		const __int64 l_ID = m_get_tth_id->executeint64_no_throw();
		return l_ID;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - get_tth_idL: " + e.getError());
	}
	return 0;
}
//========================================================================================================
void CFlylinkDBManager::inc_hitL(const string& p_Path, const string& p_FileName)
{
	try
	{
		bool p_is_no_mediainfo;
		const __int64 l_path_id = get_path_idL(p_Path, false, true, p_is_no_mediainfo, false);
		if (!l_path_id)
			return;
		m_upload_file.init(m_flySQLiteDB, "update fly_file set hit=hit+1 where name=? and dic_path=?");
		m_upload_file->bind(1, p_FileName, SQLITE_STATIC);
		m_upload_file->bind(2, l_path_id);
		m_upload_file->executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - Hit: " + e.getError());
	}
}
//========================================================================================================
#ifdef USE_REBUILD_MEDIAINFO
bool CFlylinkDBManager::rebuild_mediainfo(const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media, const TTHValue& p_tth)
{
	CFlyLock(m_cs);
	try
	{
		dcassert(p_path_id);
		if (!p_path_id)
			return false;
		const __int64 l_tth_id = get_tth_idL(p_tth);
		if (!l_tth_id)
			return false;
		dcassert(l_tth_id);
		merge_mediainfo(l_tth_id, p_path_id, p_file_name, p_media);
		return true;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - rebuild_mediainfo: " + e.getError());
	}
	return false;
}
#endif // USE_REBUILD_MEDIAINFO
//========================================================================================================
void CFlylinkDBManager::add_file(__int64 p_path_id, const string& p_file_name, int64_t p_time_stamp, const TigerTree& p_tth, int64_t p_size, CFlyMediaInfo& p_out_media)
{
	dcassert(!p_file_name.empty());
	dcassert(!Util::getFileName(p_file_name).empty());
	//dcassert(p_path_id);
	size_t l_size_cache = 0;
	{
		CFlyFastLock(m_cache_hash_files_cs);
		m_cache_hash_files.insert(make_pair(p_file_name, CFlyHashCacheItem(p_path_id, p_time_stamp, p_tth, p_size, p_out_media)));
		l_size_cache = m_cache_hash_files.size();
	}
	if (l_size_cache > 100)
	{
		flush_hash();
	}
}
//========================================================================================================
void CFlylinkDBManager::flush_hash()
{
	CFlyHashCacheMap l_local_map;
	{
		CFlyFastLock(m_cache_hash_files_cs);
		if (m_cache_hash_files.empty())
		{
			return;
		}
		else
		{
			l_local_map.swap(m_cache_hash_files);
		}
	}
	try
	{
		if (!l_local_map.empty())
		{
			CFlyLock(m_cs);
			sqlite3_transaction l_trans(m_flySQLiteDB, l_local_map.size() > 1);
			for (auto i = l_local_map.begin(); i != l_local_map.end(); ++i)
			{
				const string l_name = Text::toLower(Util::getFileName(i->first));
				dcassert(!l_name.empty());
				string l_path;
				if (i->second.m_path_id == 0)
				{
					l_path = Text::toLower(Util::getFilePath(i->first));
					dcassert(!l_path.empty());
				}
				const int64_t l_tth_id = merge_fileL(l_path, l_name, i->second.m_time_stamp, i->second.m_tth, false, i->second.m_path_id);
				if (i->second.m_out_media.isMedia())
				{
					merge_mediainfoL(l_tth_id, i->second.m_path_id, l_name, i->second.m_out_media); // Если медиаинфу расчитали - скинем ее в базу
				}
			}
			l_trans.commit();
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - flush_hash: " + e.getError());
	}
}
//========================================================================================================
bool CFlylinkDBManager::merge_mediainfo(const __int64 p_tth_id, const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media)
{
	CFlyLock(m_cs);
	sqlite3_transaction l_trans(m_flySQLiteDB, p_media.isMediaAttrExists());
	const auto l_res = merge_mediainfoL(p_tth_id, p_path_id, p_file_name, p_media);
	l_trans.commit();
	return l_res;
}
//========================================================================================================
bool CFlylinkDBManager::merge_mediainfoL(const __int64 p_tth_id, const __int64 p_path_id, const string& p_file_name, const CFlyMediaInfo& p_media)
{
	try
	{
		m_update_base_mediainfo.init(m_flySQLiteDB,
		                             "update fly_file set "
		                             "bitrate=?, media_x=?, media_y=?, media_video=?, media_audio=? "
		                             "where dic_path=? and name=?");
		sqlite3_command* l_sql = m_update_base_mediainfo.get_sql();
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
		return true;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_mediainfoL: " + e.getError());
	}
	return false;
}
//========================================================================================================
__int64 CFlylinkDBManager::merge_fileL(const string& p_Path, const string& p_file_name,
                                       const int64_t p_time_stamp, const TigerTree& p_tt,
                                       bool p_case_convet, __int64& p_path_id)
{
	__int64 l_tth_id = 0;
	try
	{
		bool p_is_no_mediainfo;
		if (p_path_id == 0)
		{
			p_path_id = get_path_idL(p_Path, true, p_case_convet, p_is_no_mediainfo, false);
		}
		dcassert(p_path_id);
		if (!p_path_id)
			return 0;
		l_tth_id = add_treeL(p_tt);
		dcassert(l_tth_id);
		if (!l_tth_id)
			return 0;
		m_insert_file.init(m_flySQLiteDB,
		                   "insert or replace into fly_file(tth_id,dic_path,name,size,stamp,stamp_share,ftype) "
		                   "values(?,?,?,?,?,?,?);");
		sqlite3_command* l_sql = m_insert_file.get_sql();
		l_sql->bind(1, l_tth_id);
		l_sql->bind(2, p_path_id);
		dcassert(p_file_name == Text::toLower(p_file_name));
		l_sql->bind(3, p_file_name, SQLITE_STATIC);
		l_sql->bind(4, p_tt.getFileSize());
		l_sql->bind(5, p_time_stamp);
		l_sql->bind(6, int64_t(File::currentTime()));
		l_sql->bind(7, ShareManager::getFType(p_file_name));
		l_sql->executenonquery();
		return l_tth_id;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - merge_fileL: " + e.getError());
	}
	return l_tth_id;
}
//========================================================================================================
void CFlylinkDBManager::add_tree(const TigerTree& p_tt)
{
	CFlyLock(m_cs);
	add_treeL(p_tt);
}
//========================================================================================================
void CFlylinkDBManager::add_tree_internal_bind_and_executeL(sqlite3_command* p_sql, const TigerTree& p_tt)
{
	p_sql->bind(1, p_tt.getFileSize());
	if (p_tt.getFileSize() > MIN_BLOCK_SIZE)
	{
		const int l_size = p_tt.getLeaves().size() * TTHValue::BYTES;
		p_sql->bind(2, p_tt.getLeaves()[0].data,  l_size, SQLITE_STATIC);
	}
	else
	{
		p_sql->bind(2);
	}
	p_sql->bind(3, p_tt.getBlockSize());
	dcassert(p_tt.getRoot() != TTHValue());
	p_sql->bind(4, p_tt.getRoot().data, 24, SQLITE_STATIC);
	p_sql->executenonquery();
}
//========================================================================================================
__int64 CFlylinkDBManager::add_treeL(const TigerTree& p_tt)
{
	{
		CFlyFastLock(g_tth_cache_cs);
		g_tiger_tree_cache.erase(p_tt.getRoot()); // Сбросим кэш, чтобы случайно не достать старую карту.
	}
	try
	{
		sqlite3_command* l_sql = nullptr;
		__int64 l_tth_id = 0;
		try
		{
			m_insert_fly_hash_block.init(m_flySQLiteDB,
			                             "insert into fly_hash_block(file_size,tiger_tree,block_size,tth) values(?,?,?,?)");
			l_sql = m_insert_fly_hash_block.get_sql();
			add_tree_internal_bind_and_executeL(l_sql, p_tt);
			l_tth_id = m_flySQLiteDB.insertid();
		}
		catch (const database_error& e)
		{
			if (e.getError().find("UNIQUE ") != string::npos) // Вероятность этого очень маленькая..
			{
				//dcassert(0);
				/*
				    m_update_fly_hash_block.init(m_flySQLiteDB,
				                                                                            "update fly_hash_block set file_size=?,tiger_tree=?,block_size=? where tth=?");
				l_sql = m_update_fly_hash_block.get();
				add_tree_internal_bind_and_executeL(l_sql, p_tt);
				*/
				l_tth_id = get_tth_idL(p_tt.getRoot());
				dcassert(l_tth_id);
			}
			else
			{
				throw;
			}
		}
		dcassert(l_tth_id);
		return l_tth_id;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - add_treeL: " + e.getError());
	}
	dcassert(0);
	return 0;
}
//========================================================================================================
void CFlylinkDBManager::clearTTHCache()
{
	CFlyFastLock(g_tth_cache_cs);
	clear_and_reset_capacity(g_tiger_tree_cache);
}
//========================================================================================================
void CFlylinkDBManager::tryFixBadAlloc()
{
	g_tth_cache_limit /= 2;
	if (g_tth_cache_limit < 10)
		g_tth_cache_limit = 10;
	clearTTHCache();
}
//========================================================================================================
/*
__int64 CFlylinkDBManager::add_treeL(const TigerTree& p_tt)
{
    {
        CFlyFastLock(g_tth_cache_cs);
        g_tiger_tree_cache.erase(p_tt.getRoot()); // Сбросим кэш, чтобы случайно не достать старую карту.
    }
    try
    {
        const __int64 l_file_size = p_tt.getFileSize();
        __int64 l_tth_id = 0;
                m_insert_fly_hash_block.init(m_flySQLiteDB,
                                                                                        "insert or replace into fly_hash_block(file_size,tiger_tree,block_size,tth) values(?,?,?,?)");
        sqlite3_command* l_sql = m_insert_fly_hash_block.get();
        l_sql->bind(1, l_file_size);
        if (l_file_size > MIN_BLOCK_SIZE)
        {
            const int l_size = p_tt.getLeaves().size() * TTHValue::BYTES;
            l_sql->bind(2, p_tt.getLeaves()[0].data,  l_size, SQLITE_STATIC);
        }
        else
        {
            l_sql->bind(2);
        }
        l_sql->bind(3, p_tt.getBlockSize());
        l_sql->bind(4, p_tt.getRoot().data, 24, SQLITE_STATIC);
        l_sql->executenonquery();
        l_tth_id = m_flySQLiteDB.insertid();
        dcassert(l_tth_id);
        return l_tth_id;
    }
    catch (const database_error& e)
    {
        errorDB("SQLite - add_treeL: " + e.getError());
    }
    return 0;
}
*/

//========================================================================================================
void CFlylinkDBManager::shutdown_engine()
{
	auto l_status = sqlite3_shutdown();
	if (l_status != SQLITE_OK)
	{
		LogManager::message("[Error] sqlite3_shutdown = " + Util::toString(l_status), true);
	}
	dcassert(l_status == SQLITE_OK);
}
//========================================================================================================
CFlylinkDBManager::~CFlylinkDBManager()
{
	if (!ClientManager::isBeforeShutdown())
	{
		CFlyFastLock(g_tth_cache_cs);
		dcassert(g_tiger_tree_cache.empty());
	}
	dcassert(m_cache_hash_files.empty());
	flush();
#ifdef _DEBUG
	{
#ifdef FLYLINKDC_USE_LASTIP_CACHE
		CFlyFastLock(m_last_ip_cs);
		for (auto h = m_last_ip_cache.cbegin(); h != m_last_ip_cache.cend(); ++h)
		{
			for (auto i = h->second.begin(); i != h->second.end(); ++i)
			{
				dcassert(i->second.m_is_item_dirty == false);
			}
		}
#endif //FLYLINKDC_USE_LASTIP_CACHE
	}
#ifdef FLYLINKDC_USE_GEO_IP
	{
		dcdebug("CFlylinkDBManager::m_country_cache size = %d\n", m_country_cache.size());
	}
#endif
	{
		dcdebug("CFlylinkDBManager::m_location_cache_array size = %d\n", m_location_cache_array.size());
	}
#endif // _DEBUG
}
//========================================================================================================
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
double CFlylinkDBManager::get_ratio() const
{
	dcassert(m_is_load_global_ratio);
	return m_global_ratio.get_ratio();
}
//========================================================================================================
tstring CFlylinkDBManager::get_ratioW() const
{
	dcassert(m_is_load_global_ratio);
	if (m_global_ratio.get_download() > 0)
	{
		LocalArray<TCHAR, 32> buf;
		_snwprintf(buf.data(), buf.size(), _T("%.2f"), get_ratio());
		return buf.data();
	}
	return Util::emptyStringT;
}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO

//========================================================================================================
void CFlylinkDBManager::push_add_virus_database_tth(const TTHValue& p_tth)
{
#ifdef FLYLINKDC_USE_LEVELDB
	m_TTHLevelDB.set_bit(p_tth, VIRUS_FILE_KNOWN);
#endif // FLYLINKDC_USE_LEVELDB
}
//========================================================================================================
void CFlylinkDBManager::push_add_share_tth(const TTHValue& p_tth)
{
#ifdef FLYLINKDC_USE_LEVELDB
	m_TTHLevelDB.set_bit(p_tth, PREVIOUSLY_BEEN_IN_SHARE);
#endif // FLYLINKDC_USE_LEVELDB
}
//========================================================================================================
void CFlylinkDBManager::push_download_tth(const TTHValue& p_tth)
{
#ifdef FLYLINKDC_USE_LEVELDB
	m_TTHLevelDB.set_bit(p_tth, PREVIOUSLY_DOWNLOADED);
#endif // FLYLINKDC_USE_LEVELDB
}
//========================================================================================================
CFlylinkDBManager::FileStatus CFlylinkDBManager::get_status_file(const TTHValue& p_tth)
{
#ifdef FLYLINKDC_USE_LEVELDB
	if (m_TTHLevelDB.is_open())
	{
	
		string l_status;
		m_TTHLevelDB.get_value(p_tth, l_status);
		int l_result = Util::toInt(l_status);
		dcassert(l_result >= 0 && l_result <= 7);
		return static_cast<FileStatus>(l_result); // 1 - скачивал, 2 - был в шаре, 3 - 1+2 и то и то, 4- вирусня
	}
	else
	{
		CFlyLock(m_cs);
		try
		{
			m_get_status_file.init(m_flySQLiteDB,
			                       "select 2 from fly_hash_block where tth=?");
			m_get_status_file->bind(1, p_tth.data, 24, SQLITE_STATIC);
			sqlite3_reader l_q = m_get_status_file->executereader();
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
			errorDB("SQLite - get_status_file: " + e.getError());
		}
	}
	return UNKNOWN;
#endif // FLYLINKDC_USE_LEVELDB
}
//========================================================================================================
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
void CFlylinkDBManager::log(const int p_area, const StringMap& p_params)
{
	CFlyLock(m_cs); // [2] https://www.box.net/shared/9e63916273d37e5b2932
	try
	{
		m_insert_fly_message.init(m_flySQLiteDB,
		                          "insert into log_db.fly_log(sdate,type,body,hub,nick,ip,file,source,target,fsize,fchunk,extra,userCID)"
		                          " values(datetime('now','localtime'),?,?,?,?,?,?,?,?,?,?,?,?);");
		const auto &l_insert_fly_message_get = m_insert_fly_message.get_sql(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'm_insert_fly_message.get()' expression repeatedly. cflylinkdbmanager.cpp 2482
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
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - log: " + e.getError() + " parrent message: " + getString(p_params, "message"));
	}
}
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
//========================================================================================================
__int64 CFlylinkDBManager::convert_tth_historyL()
{
	__int64 l_count = 0;
	try
	{
		m_flySQLiteDB.executenonquery("create table IF NOT EXISTS fly_tth(tth char(24) PRIMARY KEY NOT NULL);");
		{
			std::unique_ptr<sqlite3_command> l_sql(new sqlite3_command(m_flySQLiteDB,
			                                                           "select tth, 2 as val from fly_hash_block group by tth "
			                                                           "union all "
			                                                           "select tth, 1 as val from fly_tth"
			                                                          ));
			sqlite3_reader l_q = l_sql->executereader();
			while (l_q.read())
			{
				vector<uint8_t> l_tth;
				l_q.getblob(0, l_tth);
				dcassert(l_tth.size() == 24);
				if (l_tth.size() == 24)
				{
					m_TTHLevelDB.set_bit(TTHValue(&l_tth[0]), l_q.getint(1));
					++l_count;
				}
			}
		}
		m_flySQLiteDB.executenonquery("DROP TABLE fly_tth");
		{
			try
			{
				m_select_transfer_convert_leveldb.init(m_flySQLiteDB,
				                                       "select distinct tth,type from transfer_db.fly_transfer_file");
				sqlite3_reader l_q = m_select_transfer_convert_leveldb->executereader();
				while (l_q.read())
				{
					const TTHValue l_tth(l_q.getstring(0));
					int l_type = l_q.getint(1); // 0 - download, 1 - upload
					if (l_type == 0)
					{
						push_download_tth(l_tth);
					}
					else if (l_type == 1)
					{
						push_add_share_tth(l_tth);
					}
				}
			}
			catch (const database_error&)
			{
				dcassert(0);
				//errorDB("SQLite - convert_tth_historyL: " + e.getError());
			}
		}
		return l_count;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - convert_tth_historyL: " + e.getError());
	}
	return l_count;
}
//========================================================================================================
bool CFlylinkDBManager::is_resume_torrent(const libtorrent::sha1_hash& p_sha1)
{
	CFlyFastLock(g_resume_torrents_cs);
	return g_resume_torrents.find(p_sha1) != g_resume_torrents.end();
}
//========================================================================================================
bool CFlylinkDBManager::is_delete_torrent(const libtorrent::sha1_hash& p_sha1)
{
	CFlyFastLock(g_delete_torrents_cs);
	return g_delete_torrents.find(p_sha1) != g_delete_torrents.end();
}
//========================================================================================================
__int64 CFlylinkDBManager::convert_tth_history()
{
#ifdef FLYLINKDC_USE_LEVELDB
	__int64 l_count = 0;
	//if (get_registry_variable_int64(e_IsTTHLevelDBConvert) == 0)
	{
		//CFlyLock(m_cs);
		l_count = convert_tth_historyL();
		//set_registry_variable_int64(e_IsTTHLevelDBConvert, 1);
	}
	return l_count;
#endif // FLYLINKDC_USE_LEVELDB
}
#ifdef FLYLINKDC_USE_LEVELDB
//========================================================================================================
CFlyLevelDB::CFlyLevelDB(): m_level_db(nullptr)
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
	safe_delete(m_level_db);
	safe_delete(m_options.filter_policy);
	safe_delete(m_options.block_cache);
	// нельзя удалять - падаем. safe_delete(m_options.env);
	// TODO - leak delete m_options.comparator;
}
//========================================================================================================
bool CFlyLevelDB::open_level_db(const string& p_db_name, bool& p_is_destroy)
{
	p_is_destroy = false;
	int64_t l_count_files = 0;
	int64_t l_size_files = 0;
	dcassert(m_level_db == nullptr);
	auto l_status = leveldb::DB::Open(m_options, p_db_name, &m_level_db, l_count_files, l_size_files);
	if (l_count_files > 1000)
	{
		safe_delete(m_level_db);
		const string l_new_name = p_db_name + ".old";
		const bool l_rename_result = File::renameFile(p_db_name, l_new_name);
		if (l_rename_result)
		{
			CFlyServerJSON::pushError(90, "open_level_db rename : " + p_db_name + " - > " + l_new_name);
		}
		else
		{
			CFlyServerJSON::pushError(91, "open_level_db error rename : " + p_db_name + " - > " + l_new_name);
		}
		l_status = leveldb::DB::Open(m_options, p_db_name, &m_level_db, l_count_files, l_size_files);
	}
	dcassert(l_count_files != 0);
	if (!l_status.ok())
	{
		const auto l_result_error = l_status.ToString();
		Util::setRegistryValueString(FLYLINKDC_REGISTRY_LEVELDB_ERROR, Text::toT(l_result_error));
		if (l_status.IsIOError() || l_status.IsCorruption())
		{
			LogManager::message("[CFlyLevelDB::open_level_db] l_status.IsIOError() || l_status.IsCorruption() = " + l_result_error, true);
			//dcassert(0);
			const StringList l_delete_file = File::findFiles(p_db_name + '\\', "*.*");
			unsigned l_count_delete_error = 0;
			for (auto i = l_delete_file.cbegin(); i != l_delete_file.cend(); ++i)
			{
				if (i->size())
					if ((*i)[i->size() - 1] != '\\')
					{
						if (!File::deleteFile(*i))
						{
							++l_count_delete_error;
							LogManager::message("[CFlyLevelDB::open_level_db] error delete corrupt leveldb file  = " + *i, true);
						}
						else
						{
							LogManager::message("[CFlyLevelDB::open_level_db] OK delete corrupt leveldb file  = " + *i, true);
						}
					}
			}
			if (l_count_delete_error == 0)
			{
				l_count_files = 0;
				l_size_files = 0;
				// Create new leveldb-database
				l_status = leveldb::DB::Open(m_options, p_db_name, &m_level_db, l_count_files, l_size_files);
				if (l_status.ok())
				{
					LogManager::message("[CFlyLevelDB::open_level_db] OK Create new leveldb database: " + p_db_name, true);
					p_is_destroy = true;
				}
			}
			// most likely there's another instance running or the permissions are wrong
//			messageF(STRING_F(DB_OPEN_FAILED_IO, getNameLower() % Text::toUtf8(ret.ToString()) % APPNAME % dbPath % APPNAME), false, true);
//			exit(0);
		}
		else
		{
			LogManager::message("[CFlyLevelDB::open_level_db] !l_status.IsIOError() the database is corrupted? = " + l_result_error, true);
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
bool CFlyLevelDB::get_value(const void* p_key, size_t p_key_len, string& p_result)
{
	dcassert(m_level_db);
	if (m_level_db)
	{
		const leveldb::Slice l_key((const char*)p_key, p_key_len);
		const auto l_status = m_level_db->Get(m_readoptions, l_key, &p_result);
		if (!(l_status.ok() || l_status.IsNotFound()))
		{
			const auto l_message = l_status.ToString();
			LogManager::message(l_message, true);
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
	dcassert(m_level_db);
	if (m_level_db)
	{
		const leveldb::Slice l_key((const char*)p_key, p_key_len);
		const leveldb::Slice l_val((const char*)p_val, p_val_len);
		const auto l_status = m_level_db->Put(m_writeoptions, l_key, l_val);
		if (!l_status.ok())
		{
			const auto l_message = l_status.ToString();
			LogManager::message(l_message, true);
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
	if (get_value(p_tth, l_value))
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
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
//========================================================================================================
CFlyIPMessageCache CFlyLevelDBCacheIP::get_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick)
{
	CFlyIPMessageCache l_res;
	std::vector<char> l_key;
	create_key(p_hub_id, p_nick, l_key);
	string l_result;
	if (get_value(l_key.data(), l_key.size(), l_result))
	{
		if (!l_result.empty())
		{
			dcassert(l_result.size() == sizeof(CFlyIPMessageCache))
			if (l_result.size() == sizeof(CFlyIPMessageCache))
			{
				memcpy(&l_res, l_result.c_str(), sizeof(CFlyIPMessageCache));
			}
		}
	}
	return l_res;
}
//========================================================================================================
void CFlyLevelDBCacheIP::set_last_ip_and_message_count(uint32_t p_hub_id, const string& p_nick, uint32_t p_message_count, const boost::asio::ip::address_v4& p_last_ip)
{

	std::vector<char> l_key;
	create_key(p_hub_id, p_nick, l_key);
	CFlyIPMessageCache l_value(p_message_count, p_last_ip.to_ulong());
	set_value(l_key.data(), l_key.size(), &l_value, sizeof(l_value));
}
#endif // FLYLINKDC_USE_IPCACHE_LEVELDB
//========================================================================================================
#endif // FLYLINKDC_USE_LEVELDB
