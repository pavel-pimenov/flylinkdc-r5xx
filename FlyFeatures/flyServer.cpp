/*
 * Copyright (C) 2011-2013 FlylinkDC++ Team http://flylinkdc.com/
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

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/trim_all.hpp>

#include "flyServer.h"
#include "../client/Socket.h"
#include "../client/ClientManager.h"
#include "../client/ShareManager.h"
#include "../client/LogManager.h"
#include "../client/SimpleXML.h"
#include "../client/StringTokenizer.h"
#include "../client/Wildcards.h"
#ifdef PPA_INCLUDE_IPGUARD
#include "../client/IpGuard.h"
#endif
#include "../jsoncpp/include/json/value.h"
#include "../jsoncpp/include/json/reader.h"
#include "../jsoncpp/include/json/writer.h"
#include "../MediaInfoLib/Source/MediaInfo/MediaInfo_Const.h"
#include "../windows/resource.h"
#include "../GdiOle/GDIImage.h"
#include "../client/FavoriteManager.h"
#include "../windows/ChatBot.h"

#ifdef FLYLINKDC_USE_GATHER_STATISTICS
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
  #define PSAPI_VERSION 1
#endif // FLYLINKDC_SUPPORT_WIN_VISTA
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif // FLYLINKDC_USE_GATHER_STATISTICS

const static string g_full_user_agent = APPNAME 
#ifdef FLYLINKDC_HE
	"HE"
#endif
	" " A_VERSIONSTRING;

string g_debug_fly_server_url;
CFlyServerConfig g_fly_server_config;
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
CFlyServerStatistics g_fly_server_stat;
CServerItem CFlyServerConfig::g_stat_server;
#define FLY_SHUTDOWN_FILE_MARKER_NAME "FlylinkDCShutdownMarker.txt"
#endif // FLYLINKDC_USE_GATHER_STATISTICS
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
CServerItem CFlyServerConfig::g_test_port_server;
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
#ifdef STRONG_USE_DHT
std::vector<DHTServer>	  CFlyServerConfig::g_dht_servers;
#endif // STRONG_USE_DHT
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
static volatile long g_running;
std::vector<CServerItem> CFlyServerConfig::g_mirror_read_only_servers;
CServerItem CFlyServerConfig::g_main_server;
uint16_t CFlyServerAdapter::CFlyServerQueryThread::g_minimal_interval_in_ms  = 2000; 
DWORD CFlyServerConfig::g_winet_connect_timeout = 2000;
uint16_t CFlyServerConfig::g_winet_min_response_time_for_log = 200;
DWORD CFlyServerConfig::g_winet_receive_timeout = 1000; 
DWORD CFlyServerConfig::g_winet_send_timeout    = 1000; 
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
uint16_t CFlyServerConfig::g_max_ddos_connect_to_me = 10; // Не более 10 коннектов на один IP в течении минуты
uint16_t CFlyServerConfig::g_ban_ddos_connect_to_me = 10; // Блокируем подключения к этом IP в течении 10 минут
uint16_t CFlyServerConfig::g_max_unique_tth_search  = 10; // Не принимаем в течении 10 секунд одинаковых поисков по TTH для одного и того-же целевого IP:PORT (UDP)
#ifdef USE_SUPPORT_HUB
string CFlyServerConfig::g_support_hub = "dchub://dc.fly-server.ru";
#endif // USE_SUPPORT_HUB
string CFlyServerConfig::g_faq_search_does_not_work = "http://www.flylinkdc.ru/2014/01/flylinkdc.html";
StringSet CFlyServerConfig::g_parasitic_files;
StringSet CFlyServerConfig::g_mediainfo_ext;
StringSet CFlyServerConfig::g_virus_ext;
StringSet CFlyServerConfig::g_block_share_ext;
StringSet CFlyServerConfig::g_custom_compress_ext;
StringSet CFlyServerConfig::g_block_share_name;
StringList CFlyServerConfig::g_block_share_mask;

//======================================================================================================
bool CFlyServerConfig::isSupportTag(const string& p_tag) const
{
	const auto l_string_pos = p_tag.find("/String");
	if(l_string_pos != string::npos)
	{
		return false;
	}
	if(m_include_tag.empty())
	{
		return m_exclude_tag.find(p_tag) == m_exclude_tag.end();
	}
	else
	{
		return m_include_tag.find(p_tag) != m_include_tag.end();
	}
}
//======================================================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
void CFlyServerStatistics::saveShutdownMarkers()
{
	try
	{
	 File l_file (Util::getConfigPath() + FLY_SHUTDOWN_FILE_MARKER_NAME, File::WRITE, File::CREATE | File::TRUNCATE);
	 l_file.write(Util::toString(m_time_mark[TIME_SHUTDOWN_CORE]) + ',' + Util::toString(m_time_mark[TIME_SHUTDOWN_GUI]));
	}
	catch (const FileException&)
	{
	}
}
//======================================================================================================
bool CFlyServerConfig::isSupportFile(const string& p_file_ext, uint64_t p_size) const
{
	dcassert(!m_scan.empty()); // TODO: fix CFlyServerConfig::loadConfig() in debug.
	return p_size > m_min_file_size && m_scan.find(p_file_ext) != m_scan.end(); // [!] IRainman opt.
}
//======================================================================================================

CServerItem& CFlyServerConfig::getRandomMirrorServer(bool p_is_set)
{
	if(p_is_set == false && !g_mirror_read_only_servers.empty())
	{
	 const int l_id = Util::rand(g_mirror_read_only_servers.size());
	 return g_mirror_read_only_servers[l_id];
	}
	else // Вернем пишущий сервер
	{
		return g_main_server;
	}
}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
//======================================================================================================
bool CFlyServerConfig::isParasitFile(const string& p_file)
{
	return isCheckName(g_parasitic_files, p_file); // [!] IRainman opt.
}
//======================================================================================================
#ifdef STRONG_USE_DHT
const DHTServer& CFlyServerConfig::getRandomDHTServer()
{
	dcassert(!g_dht_servers.empty());
	if(!g_dht_servers.empty())
	{
	 const int l_id = Util::rand(g_dht_servers.size());
	 return g_dht_servers[l_id];
	}
	else
	{
		// fix https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=113332
		// TODO - Попытаться повторить.
		g_dht_servers.push_back(DHTServer("http://ssa.in.ua/dcDHT.php", "")); 
		return g_dht_servers[0];
	}
}
#endif // STRONG_USE_DHT
//======================================================================================================
inline static void checkStrKey(const string& p_str) // TODO: move to Util.
{
#ifdef _DEBUG
	dcassert(Text::toLower(p_str) == p_str);
	string l_str_copy = p_str;
	boost::algorithm::trim(l_str_copy);
	dcassert(l_str_copy == p_str);
#endif
}
//======================================================================================================
void CFlyServerConfig::ConvertInform(string& p_inform) const
{
	// TODO - убрать лишние строки из результат
	if(!m_exclude_tag_inform.empty())
	{
	string l_result_line;
	int  l_start_line = 0;
	auto l_end_line = string::npos;
	do
	{
	l_end_line = p_inform.find("\r\n",l_start_line);
	if (l_end_line != string::npos)
	 {
		string l_cur_line = p_inform.substr(l_start_line,l_end_line - l_start_line);
		// Фильтруем строчку через маски
		bool l_ignore_tag = false;
		for(auto i = m_exclude_tag_inform.cbegin(); i != m_exclude_tag_inform.cend(); ++i)
		{
			const auto l_tag_begin = l_cur_line.find(*i);
			const auto l_end_index  = i->size() + 1;
			if(l_tag_begin != string::npos 
				&& l_tag_begin == 0  // Тэг в начале строки?
				&& l_cur_line.size() > l_end_index // После него есть место?
				&& (l_cur_line[l_end_index] == ':' || l_cur_line[l_end_index] == ' ') // После тэга пробел или ':'
				)
			{ 
				l_ignore_tag = true;
				break;
			}
		}
		if(l_ignore_tag == false)
		{
			  boost::algorithm::trim_all(l_cur_line);
			  l_result_line += l_cur_line + "\r\n";
		}
		l_start_line = l_end_line + 2;
	 }
	}
	while (l_end_line != string::npos);
    p_inform = l_result_line;	
  }
}
//======================================================================================================
void CFlyServerConfig::loadConfig()
{
	const auto l_cur_tick = GET_TICK();
	if(m_time_load_config == 0 || (l_cur_tick - m_time_load_config) > m_time_reload_config ) 
	{
		m_time_load_config = l_cur_tick + 1;
		CFlyLog l_fly_server_log("[fly-server]");
		LPCSTR l_res_data;
		std::string l_data;
#ifdef _DEBUG
  // #define USE_FLYSERVER_LOCAL_FILE
#endif
#ifdef USE_FLYSERVER_LOCAL_FILE
		const string l_url_config_file = "file://C:/vc10/etc/flylinkdc-config-r5xx.xml"; 
		g_debug_fly_server_url = "127.0.0.1";
#else
		const string l_url_config_file = "http://www.fly-server.ru/etc/flylinkdc-config-r5xx.xml";
#endif
		l_fly_server_log.step("Download:" + l_url_config_file);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		if (Util::getDataFromInet(Text::toT(g_full_user_agent).c_str(), 4096, l_url_config_file, l_data, 0) == 0)
		{
			l_fly_server_log.step("Error download! Config will be loaded from internal resources");
#endif //FLYLINKDC_USE_MEDIAINFO_SERVER
			if (Util::GetTextResource(IDR_FLY_SERVER_CONFIG, l_res_data)) //
				l_data = l_res_data;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		}
#endif //FLYLINKDC_USE_MEDIAINFO_SERVER
		try
		{
			SimpleXML l_xml;
			l_xml.fromXML(l_data);
			if (l_xml.findChild("Bootstraps"))
			{
#ifdef STRONG_USE_DHT
				if(g_dht_servers.empty()) // DHT забираем один раз
				{
					l_xml.stepIn();
					while (l_xml.findChild("server"))
					{
						const string& l_server = l_xml.getChildAttrib("url");
						if (!l_server.empty())
						{
							const string& l_agent = l_xml.getChildAttrib("agent");
							g_dht_servers.push_back(DHTServer(l_server, l_agent));
						}
					}
					l_xml.stepOut();
				}
#endif // STRONG_USE_DHT
				l_xml.stepIn();
				if (l_xml.findChild("fly-server"))
				{
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
					g_main_server.setIp(l_xml.getChildAttrib("ip"));
					g_main_server.setPort(Util::toInt(l_xml.getChildAttrib("port")));
					dcassert(g_main_server.getPort());
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
					// Предусматриваем альтернативный сервер статистики
					g_stat_server.setIp(l_xml.getChildAttrib("stat_server_ip")); 
					if(!g_stat_server.getIp().empty())
					{
						g_stat_server.setPort(Util::toInt(l_xml.getChildAttrib("stat_server_port")));
						dcassert(g_stat_server.getPort());
						if(g_stat_server.getPort() == 0)
						{
							g_stat_server.setPort(g_main_server.getPort());
						}
					}
					else
					{
						g_stat_server = g_main_server;
					}
#endif
					// Предусматриваем альтернативный сервер тестирования портов
					g_test_port_server.setIp(l_xml.getChildAttrib("test_server_ip")); 
					if(!g_test_port_server.getIp().empty())
					{
						g_test_port_server.setPort(Util::toInt(l_xml.getChildAttrib("test_server_port")));
						dcassert(g_test_port_server.getPort());
						if(g_test_port_server.getPort() == 0)
						{
							g_test_port_server.setPort(g_main_server.getPort());
						}
					}
					else
					{
						g_test_port_server = g_main_server;
					}	
					auto initUINT16 = [&](const string& p_name, uint16_t& p_value, uint16_t p_min) -> void
					{
						const string l_value = l_xml.getChildAttrib(p_name);
						if(!l_value.empty())
						{
							p_value = Util::toInt(l_value);
							if(p_value < p_min)
							  p_value = p_min;
						}
					};
					auto initDWORD = [&](const string& p_name, DWORD& p_value) -> void
					{
						const string l_value = l_xml.getChildAttrib(p_name);
						if(!l_value.empty())
						{
							p_value = Util::toInt(l_value);
						}
					};
					auto initString = [&](const string& p_name, string& p_value) -> void
					{
						const string l_value = l_xml.getChildAttrib(p_name);
						if(!l_value.empty())
					{
							p_value = l_value;
					}					
					};
					initUINT16("max_unique_tth_search",g_max_unique_tth_search,3);
					initUINT16("max_ddos_connect_to_me",g_max_ddos_connect_to_me,3);
					initUINT16("ban_ddos_connect_to_me",g_ban_ddos_connect_to_me,3);
					initDWORD("winet_connect_timeout",g_winet_connect_timeout);
					initDWORD("winet_receive_timeout",g_winet_receive_timeout);
					initDWORD("winet_send_timeout",g_winet_send_timeout);
					initUINT16("winet_min_response_time_for_log",g_winet_min_response_time_for_log,50);

					m_min_file_size = Util::toInt64(l_xml.getChildAttrib("min_file_size")); // В конфиге min_size - переименовать
					dcassert(m_min_file_size);
					//m_max_size_value = Util::toInt(l_xml.getChildAttrib("max_size_value")); // Не используется но есть в конфиге - убрать везде
					//dcassert(m_max_size_value);
					m_zlib_compress_level = Util::toInt(l_xml.getChildAttrib("zlib_compress_level"));
					dcassert(m_zlib_compress_level >= Z_NO_COMPRESSION && m_zlib_compress_level <= Z_BEST_COMPRESSION);
					if(m_zlib_compress_level <= Z_NO_COMPRESSION || m_zlib_compress_level > Z_BEST_COMPRESSION)
					{
						m_zlib_compress_level = Z_BEST_COMPRESSION;
					}
					m_send_full_mediainfo = Util::toInt(l_xml.getChildAttrib("send_full_mediainfo")) == 1;
#ifdef USE_SUPPORT_HUB
					initString("support_hub",g_support_hub);
#endif // USE_SUPPORT_HUB
					initString("faq_search",g_faq_search_does_not_work);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
					m_collect_lost_location = Util::toInt(l_xml.getChildAttrib("collect_lost_location")) == 1;
#endif
					m_type = l_xml.getChildAttrib("type") == "http" ? TYPE_FLYSERVER_HTTP : TYPE_FLYSERVER_TCP ;
					CFlyServerAdapter::CFlyServerQueryThread::setMinimalIntervalInMilliSecond(Util::toInt(l_xml.getChildAttrib("minimal_interval")));
					l_xml.getChildAttribSplit("scan", m_scan, [this](const string& n)
					{
						checkStrKey(n);
						m_scan.insert(n);
			        });

					l_xml.getChildAttribSplit("exclude_tag", m_exclude_tag, [this](const string& n)
					{
						m_exclude_tag.insert(n);
					});

					l_xml.getChildAttribSplit("include_tag", m_include_tag, [this](const string& n)
					{
						m_include_tag.insert(n);
					});

					// Достанем RO-зеркала
					l_xml.getChildAttribSplit("mirror_read_only_server", g_mirror_read_only_servers, [this](const string& n)
					{
						const auto l_port_pos = n.find(':');
						if(l_port_pos != string::npos)
							g_mirror_read_only_servers.push_back(CServerItem(n.substr(0, l_port_pos), atoi(n.c_str() + l_port_pos + 1)));
					});
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
					l_xml.getChildAttribSplit("exclude_tag_inform", m_exclude_tag_inform, [this](const string& n)
					{
						m_exclude_tag_inform.push_back(n);
					});

					l_xml.getChildAttribSplit("parasitic_file", g_parasitic_files, [this](const string& n)
					{
						g_parasitic_files.insert(n);
					});

					l_xml.getChildAttribSplit("mediainfo_ext", g_mediainfo_ext, [this](const string& n)
					{
						checkStrKey(n);
						g_mediainfo_ext.insert(n);
					});
					l_xml.getChildAttribSplit("virus_ext", g_virus_ext, [this](const string& n)
					{
						checkStrKey(n);
						g_virus_ext.insert(n);
					});
					l_xml.getChildAttribSplit("block_share_ext", g_block_share_ext, [this](const string& n)
					{
						checkStrKey(n);
						g_block_share_ext.insert(n);
					});
					g_block_share_ext.insert(TEMP_EXTENSION);
					l_xml.getChildAttribSplit("custom_compress_ext", g_custom_compress_ext, [this](const string& n)
					{
						checkStrKey(n);
						g_custom_compress_ext.insert(n);
					});
					l_xml.getChildAttribSplit("block_share_name", g_block_share_name, [this](const string& n)
					{
						checkStrKey(n);
						g_block_share_name.insert(n);
					});
					l_xml.getChildAttribSplit("block_share_mask", g_block_share_mask, [this](const string& n)
					{
						checkStrKey(n);
						g_block_share_mask.push_back(n);
					});
				}
				l_xml.stepOut();
			}
			l_fly_server_log.step("Download and parse - Ok!");
			m_time_reload_config = TIME_TO_RELOAD_CONFIG_IF_SUCCESFUL;
			m_time_load_config = l_cur_tick;
		}
		catch (const Exception& e)
		{
			dcassert(0);
			dcdebug("CFlyServerConfig::loadConfig parseXML ::Problem: %s\n", e.what());
			l_fly_server_log.step("parseXML Problem:" + e.getError());
		}
	}
}
bool CFlyServerConfig::isVirusExt(const string& p_ext)
{
	return isCheckName(g_virus_ext, p_ext);
}
bool CFlyServerConfig::isMediainfoExt(const string& p_ext)
{
	return isCheckName(g_mediainfo_ext, p_ext);
}
bool CFlyServerConfig::isCompressExt(const string& p_ext)
{
	return isCheckName(g_custom_compress_ext, p_ext);
}
bool CFlyServerConfig::isBlockShare(const string& p_name)
{
	dcassert(Text::toLower(p_name) == p_name);
	
	if(isCheckName(g_block_share_ext, Util::getFileExtWithoutDot(p_name))) // Контроль по расширениям
	{
	   return true;
	}
	else if(isCheckName(g_block_share_name, p_name)) // Контроль по файлам на точное соотвествие
	{
	   return true;
	}
	else if (Wildcard::patternMatchLowerCase(p_name, g_block_share_mask)) // Попал под маску плохую
	{ 
		return true;
	}
	return false; // Файл хороший
}

string CFlyServerConfig::DBDelete()
{
	string l_where;
	string l_or;
	for (auto i = g_mediainfo_ext.cbegin(); i !=  g_mediainfo_ext.cend(); ++i)
	{
		l_where = l_where + l_or + string("lower(name) like '%.") + *i + string("'");
		if (l_or.empty())
			l_or = " or ";
	}
	return l_where;
}
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
//======================================================================================================
//string sendTTH(const CFlyServerKey& p_fileInfo, bool p_send_mediainfo)
//{
//	CFlyServerKeyArray l_array;
//	l_array.push_back(p_fileInfo);
//	return sendTTH(l_array,p_send_mediainfo);
//}
//======================================================================================================
string CFlyServerAdapter::CFlyServerJSON::g_fly_server_id;
//===================================================================================================================================
void CFlyServerAdapter::push_mediainfo_to_fly_server()
{
	CFlyServerKeyArray l_copy_map;
	{
		Lock l(m_cs_fly_server);
		l_copy_map.swap(m_SetFlyServerArray);
	}
	if (!l_copy_map.empty())
	{
		CFlyServerJSON::connect(l_copy_map, true); // Передать медиаинформацию на сервер (TODO - можно отложить и предать позже)
	}
}
//======================================================================================================
void CFlyServerAdapter::prepare_mediainfo_to_fly_serverL()
{
	// Обойдем кандидатов для предачи на сервер.
	// Массив - есть у нас в базе, но нет на fly-server
	for (auto i = m_tth_media_file_map.begin(); i != m_tth_media_file_map.end(); ++i)
	{
		CFlyMediaInfo l_media_info;
		if (CFlylinkDBManager::getInstance()->load_media_info(i->first, l_media_info, false))
		{
			bool l_is_send_info = l_media_info.isMedia() && g_fly_server_config.isFullMediainfo() == false; // Есть медиаинфа и сервер не ждет полный комплект?
			if (g_fly_server_config.isFullMediainfo()) // Если сервер ждет от нас только полный комплект - проверим наличие атрибутной составялющей
				l_is_send_info = l_media_info.isMediaAttrExists();
			if (l_is_send_info)
			{
				CFlyServerKey l_info(i->first, i->second);
				l_info.m_media = l_media_info; // Получили медиаинформацию с локальной базы
				m_SetFlyServerArray.push_back(l_info);
			}
		}
	}
	m_tth_media_file_map.clear();
}
//======================================================================================================
void CFlyServerAdapter::CFlyServerJSON::login()
{
	if(g_fly_server_id.empty())
	{
		CFlyLog l_log("[fly-login]");
		Json::Value  l_root;   
		Json::Value& l_info = l_root["login"];
		l_info["CID"] = ClientManager::getMyCID().toBase32();
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		std::vector<std::string> l_lost_ip_array;
		CFlylinkDBManager::getInstance()->get_lost_location(l_lost_ip_array);
		if(!l_lost_ip_array.empty())
		{
			Json::Value& l_lost_ip = l_info["customlocation_lost_ip"];
			int l_count_lost = 0;
			for (auto i = l_lost_ip_array.cbegin(); i!= l_lost_ip_array.cend(); ++i)
			{
				l_lost_ip[l_count_lost++] = *i;
			}
		}
#endif
		const std::string l_post_query = l_root.toStyledString();
		bool l_is_send;
		string l_result_query = postQuery(true,false,false,false,false,"fly-login",l_post_query,l_is_send);
		Json::Value l_result_root;
		Json::Reader l_reader;
		const bool l_parsingSuccessful = l_reader.parse(l_result_query, l_result_root);
		if (!l_parsingSuccessful && !l_result_query.empty())
		{
			l_log.step("Failed to parse json configuration: l_result_query = " + l_result_query);
		}
		else
		{
			g_fly_server_id = l_result_root["ID"].asString();
			if(!g_fly_server_id.empty())
				l_log.step("Register OK!");
//			else
//				g_fly_server_id
		}
	}
}
static void getDiskAndMemoryStat(Json::Value& p_info)
{
		if(ClientManager::isValidInstance())
		{
			  p_info["CID"] = ClientManager::getMyCID().toBase32(); 
		}
		p_info["Client"] = g_full_user_agent;
		p_info["OS"] = CompatibilityManager::getFormatedOsVersion();
		p_info["CPUCount"] = CompatibilityManager::getProcessorsCount();
		{
		Json::Value& l_disk_info = p_info["Disk"];
				auto getFileSize = [](const tstring& p_file_name) -> int64_t
				{
					int64_t l_size = 0;
					int64_t l_outFileTime = 0;
					File::isExist(p_file_name, l_size, l_outFileTime);
					return l_size;
				};
				const tstring l_path = Text::toT(Util::getConfigPath());
				l_disk_info["DBMain"] = getFileSize(l_path + _T("\\FlylinkDC.sqlite"));
				l_disk_info["DBDHT"] = getFileSize(l_path + _T("\\FlylinkDC_dht.sqlite"));
				l_disk_info["DBMediainfo"] = getFileSize(l_path + _T("\\FlylinkDC_mediainfo.sqlite"));
				l_disk_info["DBLog"] = getFileSize(l_path + _T("\\FlylinkDC_log.sqlite"));
				l_disk_info["DBStat"] = getFileSize(l_path + _T("\\FlylinkDC_stat.sqlite"));
				l_disk_info["DBUser"] = getFileSize(l_path + _T("\\FlylinkDC_user.sqlite")); // TODO - сделать обод общего массива				

				DWORD l_cluster, l_sector_size, l_freeclustor;
				int64_t l_space;
				if(l_path.size() >= 3)
				{
				if (GetDiskFreeSpace(l_path.substr(0,3).c_str(), &l_cluster, &l_sector_size, &l_freeclustor, NULL))
				{
				 l_space  = l_cluster * l_sector_size;
				 l_space *= l_freeclustor;
				 l_disk_info["DBFreeSpace"] = int64_t(l_space/1024/1024);
				}
				}
		}
		PROCESS_MEMORY_COUNTERS l_pmc = {0};
		const auto l_mem_ok = GetProcessMemoryInfo(GetCurrentProcess(), &l_pmc, sizeof(l_pmc));
		dcassert(l_mem_ok);
		if(l_mem_ok) // Под Wine может не работать
		{
		 Json::Value& l_mem_info = p_info["Memory"];
		l_mem_info["WorkingSetSize"]     = int64_t(l_pmc.WorkingSetSize);
		l_mem_info["PeakWorkingSetSize"] = int64_t(l_pmc.PeakWorkingSetSize);
		l_mem_info["TotalPhys"] = CompatibilityManager::getTotalPhysMemory();
		}
		Json::Value& l_handle_info = p_info["Handle"];
		{
		DWORD l_handle_count = 0;
		const auto l_hc_ok = GetProcessHandleCount(GetCurrentProcess(), &l_handle_count);
		dcassert(l_hc_ok);
		l_handle_info["Handle"] = int(l_handle_count); // TODO научить jsoncpp понимать DWORD
		auto getResourceCounter = [](int p_type_object) -> unsigned
		 {
			const unsigned l_res_count = GetGuiResources(GetCurrentProcess(), p_type_object);
			return l_res_count;
        };

		l_handle_info["GDI"]     = getResourceCounter(GR_GDIOBJECTS);
		l_handle_info["UserObj"] = getResourceCounter(GR_USEROBJECTS);

#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
# define GR_GDIOBJECTS_PEAK  2       /* Peak count of GDI objects */
# define GR_USEROBJECTS_PEAK 4       /* Peak count of USER objects */
#endif 
		 // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683192%28v=vs.85%29.aspx
		 // This value is not supported until Windows 7 and Windows Server 2008 R2.
		l_handle_info["GDIPeak"]      = getResourceCounter(GR_GDIOBJECTS_PEAK);
		l_handle_info["UserObjPeak"]  = getResourceCounter(GR_USEROBJECTS_PEAK);
		}
		// TODO - Свободное место на диске системном и том, где стоит флай-база
		{
			//Json::Value& l_disk_info = l_info["Disk"];
			//l_disk_info["SysFree"] = 
			//l_disk_info["sqliteFree"] = 
		}
}
//======================================================================================================
bool CFlyServerAdapter::CFlyServerJSON::pushTestPort(const string& p_magic,
		const std::vector<unsigned short>& p_udp_port,
		const std::vector<unsigned short>& p_tcp_port,
		string& p_external_ip,
		int p_timer_value)
{
		CFlyLog l_log("[fly-test-port]");
		Json::Value  l_info;   
		l_info["CID"] = p_magic;
		if(p_timer_value)
		{
			l_info["Interval"] = p_timer_value;
		}
		auto initPort = [&](const std::vector<unsigned short>& p_port, const char* p_key) -> void
		{
			if(!p_port.empty())
			{
				auto& l_ports = l_info[p_key];
				for(int i = 0;i < int(p_port.size()); ++i)
		{
					l_ports[i]["port"] = p_port[i];
			}
		}
		};
		initPort(p_udp_port,"udp");
		initPort(p_tcp_port,"tcp");
		if(SETTING(BIND_ADDRESS) != "0.0.0.0")
		{
		  l_info["ip"] = SETTING(BIND_ADDRESS);
		}
		const std::string l_post_query = l_info.toStyledString();
		bool l_is_send = false;
		p_external_ip.clear();
	    const auto l_result = postQuery(false,false,true,true,true,"fly-test-port",l_post_query,l_is_send); // Без компрессии
		// TODO - приделать счетчик таймаута и передавать его в статистику или в след пакет?
		if(!l_is_send)
		{
					l_log.step("Error POST query");
		}
		else
		{
			Json::Value l_root;
			Json::Reader reader;
			const bool parsingSuccessful = reader.parse(l_result, l_root);
			if (!parsingSuccessful)
			{
				l_log.step("Error parse JSON: " + l_result);
			}
		else
		{
					p_external_ip = l_root["ip"].asString();
		}
		}
		return l_is_send;
}
//======================================================================================================
bool CFlyServerAdapter::CFlyServerJSON::pushError(const string& p_error)
{
		CFlyLog l_log("[fly-error-sql]");
		Json::Value  l_info;   
		l_info["error"] = p_error;
		l_info["ID"]  = g_fly_server_id;
		l_info["Threads"]  =  Thread::getThreadsCount();
		l_info["Current"]  = Util::formatDigitalClock(time(nullptr));
		getDiskAndMemoryStat(l_info);
		const std::string l_post_query = l_info.toStyledString();
		bool l_is_send = false;
	    postQuery(true,true,false,false,false,"fly-error-sql",l_post_query,l_is_send);
		if(!l_is_send)
		{
			 // TODO Передача не удалась - скинем данные в файлы
		}
		return l_is_send;
}
//======================================================================================================
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
void CFlyServerAdapter::CFlyServerJSON::pushStatistic(const bool p_is_sync_run)
{
	Thread::ConditionLockerWithSpin l(g_running);
	login();
	CFlyLog l_log("[fly-stat]");
	Json::Value  l_info;   
	if(p_is_sync_run == false ) // При останове не делаем этого
		{
		// Сбросим 10 записей отложенной статистики если накопилась
		 CFlylinkDBManager::getInstance()->flush_lost_json_statistic();
		}
		else
		{
		l_info["IsShutdown"] = "1"; // Поставим маркер останова флая
		}
		dcassert(!g_fly_server_id.empty());
		
		getDiskAndMemoryStat(l_info);

		l_info["ID"]  = g_fly_server_id;
		const string l_VID_Array = Util::getRegistryCommaSubkey(_T("VID"));
		if(!l_VID_Array.empty())
		{
			l_info["VID"] = l_VID_Array;
		}
		if(ChatBot::isLoaded())
		{
			l_info["is_chat_bot"] = 1;
		}
		if(SETTING(ENABLE_AUTO_BAN))
		{
			l_info["is_autoban"] = 1;
		}
		extern bool g_DisableSQLJournal;
		if (g_DisableSQLJournal || BOOLSETTING(SQLITE_USE_JOURNAL_MEMORY))
		{
			l_info["is_journal_memory"] = 1;
		}
		extern bool g_UseWALJournal;
		if (g_UseWALJournal)
		{
			l_info["is_journal_wal"] = 1;
		}
		extern bool g_UseSynchronousOff;
		if (g_UseSynchronousOff)
		{
			l_info["is_synchronous_off"] = 1;
		}
		//
		const auto l_ISP_URL = SETTING(ISP_RESOURCE_ROOT_URL);
		if(!l_ISP_URL.empty())
		{
			l_info["ISP_URL"] = l_ISP_URL;
		}
		// Агрегационные параметры
		{
		    Json::Value& l_stat_info = l_info["Stat"];
			l_stat_info["Files"] = Util::toString(ShareManager::getInstance()->getSharedFiles());
			l_stat_info["Size"]  = ShareManager::getInstance()->getShareSizeString();
			l_stat_info["Users"] = Util::toString(ClientManager::getTotalUsers());
			// TODO l_stat_info["MaxUsers"] = 
			l_stat_info["Hubs"]  = Util::toString(Client::getTotalCounts());
			l_stat_info["DBQueueSources"] =  CFlylinkDBManager::getCountQueueSources();        
			l_stat_info["FavUsers"] = FavoriteManager::getInstance()->getCountFavsUsers();
			l_stat_info["Threads"]  =  Thread::getThreadsCount();
		}
		// Статистика по временым меткам
		{
			static string g_first_time = Util::formatDigitalClock(time(nullptr));
			Json::Value& l_time_info = l_info["Time"];
			l_time_info["Start"]    = g_first_time;
			l_time_info["Current"]  = Util::formatDigitalClock(time(nullptr));
			static bool g_is_first = false;
#ifndef _DEBUG
			if(!g_is_first)
#endif
			{
				g_is_first = true;
				Json::Value l_error_info;
				{
				// Сохраним ошибки
				auto appendError = [&l_error_info](const wchar_t* p_reg_value, const char* p_json_key)
				{
					const string l_reg_value = Util::getRegistryValueString(p_reg_value);
					if(!l_reg_value.empty())
					{
						Util::deleteRegistryValue(p_reg_value); // TODO - удалять сразу после чтения пока открыт ключик
						l_error_info[p_json_key] = l_reg_value;
				}
				};
				appendError(FLYLINKDC_REGISTRY_MEDIAINFO_CRASH_KEY,"MediaCrash");
				appendError(FLYLINKDC_REGISTRY_MEDIAINFO_FREEZE_KEY,"MediaFreeze");
				appendError(FLYLINKDC_REGISTRY_SQLITE_ERROR,"SQLite");
				appendError(FLYLINKDC_REGISTRY_LEVELDB_ERROR,"LevelDB");
				}
				if(!l_error_info.isNull())
				{
					l_info["Error"] = l_error_info;
				}
				// Статистику
				l_time_info["StartGUI"]   = Util::toString(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_GUI]);
				l_time_info["StartCore"]  = Util::toString(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE]);
				// Заберем предыдущие маркеры завершения
				try
				{
				const string l_marker_file_name = Util::getConfigPath() + FLY_SHUTDOWN_FILE_MARKER_NAME;
					{
						File l_file (l_marker_file_name, File::READ, File::OPEN);
						const StringTokenizer<string> l_markers(l_file.read(), ',');
						dcassert(l_markers.getTokens().size() == 2);
						const auto& l_token = l_markers.getTokens();
						if(l_token.size() == 2)
						{
							l_time_info["PrevShutdownCore"] = l_token[0];
							l_time_info["PrevShutdownGUI"]  = l_token[1];
						}
					}
				File::deleteFile(l_marker_file_name);
				}
				catch (const FileException&)
				{
				}
			}
		}
#ifdef IRAINMAN_INCLUDE_GDI_OLE
		if(CGDIImage::g_AnimationDeathDetectCount || CGDIImage::g_AnimationCount || CGDIImage::g_AnimationCountMax)
		{
		 Json::Value& l_debug_info = l_info["Debug"];
		 if(CGDIImage::g_AnimationDeathDetectCount)
	     {
			  l_debug_info["AnimationDeathDetectCount"] = Util::toString(CGDIImage::g_AnimationDeathDetectCount);
		 }
		 if(CGDIImage::g_AnimationCount || CGDIImage::g_AnimationCountMax)
		 {
			  l_debug_info["AnimationCount"] = Util::toString(CGDIImage::g_AnimationCount);
			  l_debug_info["AnimationCountMax"] = Util::toString(CGDIImage::g_AnimationCountMax);
		 }
		}
#endif // IRAINMAN_INCLUDE_GDI_OLE
		// Сетевые настройки
		{
			Json::Value& l_net_info = l_info["Net"];
			if(BOOLSETTING(AUTO_DETECT_CONNECTION))
			{
			 l_net_info["AutoDetect"] = "1";
			}
			l_net_info["TypeConnect"] = SETTING(INCOMING_CONNECTIONS);
			if(!g_fly_server_stat.m_upnp_status.empty())
			{
			 l_net_info["UPNPStatus"]  = g_fly_server_stat.m_upnp_status;
			}
			if(!g_fly_server_stat.m_upnp_router_name.empty())
			{
			 l_net_info["Router"]      = g_fly_server_stat.m_upnp_router_name;
			}
		}
		const std::string l_post_query = l_info.toStyledString();
		bool l_is_send = false;
		if(BOOLSETTING(USE_STATICTICS_SEND) && p_is_sync_run == false)
		{
		  postQuery(true,true,false,false,false,"fly-stat",l_post_query,l_is_send);
		}
		if(!l_is_send || p_is_sync_run)
			           // Если не удалось отправить или отключено/отложено. 
			           // собираем стату локально (чтобы в будущем построить аналитику на клиенте)
		{
			 // Передача не удалась - скинем данные в локальную базу
			CFlylinkDBManager::getInstance()->push_json_statistic(l_post_query);
		}
}
#endif // FLYLINKDC_USE_GATHER_STATISTICS
//======================================================================================================
string CFlyServerAdapter::CFlyServerJSON::postQuery(bool p_is_set, 
													bool p_is_stat_server,
													bool p_is_test_port_server,
													bool p_is_disable_zlib_in,
													bool p_is_disable_zlib_out,
													const char* p_query, 
													const string& p_body,
													bool& p_is_send)
{
	p_is_send = false;
	dcassert(!p_body.empty());
	CServerItem& l_Server =	p_is_test_port_server ? CFlyServerConfig::getTestPortServer() :
		                       p_is_stat_server ? CFlyServerConfig::getStatServer() : 
	                        CFlyServerConfig::getRandomMirrorServer(p_is_set);
	if(!g_debug_fly_server_url.empty())
	{
		l_Server.setIp(g_debug_fly_server_url); // Перекрываем адрес флай-сервера для всех сервисов на отладочный
	}
	const string l_log_marker = "[fly-server][" + l_Server.getIp() + ":" + Util::toString(l_Server.getPort()) + "]";
	CFlyLog l_fly_server_log(l_log_marker);
#ifdef PPA_INCLUDE_IPGUARD
	if (BOOLSETTING(ENABLE_IPGUARD) && IpGuard::isValidInstance()) 
	{
		string l_reason;
		if (IpGuard::getInstance()->check(l_Server.getIp(), l_reason))
		{
			l_fly_server_log.step(" (" + l_Server.getIp() + "): IPGuard: " + l_reason);
			return Util::emptyString;
		}
	}
#endif
	string l_result_query;
	//static const char g_hdrs[]		= "Content-Type: application/x-www-form-urlencoded"; // TODO - оно нужно?
	//static const size_t g_hdrs_len   = strlen(g_hdrs); // Можно заменить на (sizeof(g_hdrs)-1) ...
	// static LPCSTR g_accept[2]	= {"*/*", NULL};
	std::vector<uint8_t> l_post_compress_query;
	string l_log_string;
	if(g_fly_server_config.getZlibCompressLevel() > Z_NO_COMPRESSION) // Выполняем сжатие запроса?
	{
		if(p_is_disable_zlib_in == false)
		{
		unsigned long l_dest_length = compressBound(p_body.length()) + 2;
		l_post_compress_query.resize(l_dest_length);
		const int l_zlib_result = compress2(l_post_compress_query.data(), &l_dest_length,
	                         (uint8_t*)p_body.data(), 
							 p_body.length(), g_fly_server_config.getZlibCompressLevel()); 
                             // На клиенте пока жмем по максимуму - нагрузка мелкая. 
                             // если декомпрессия будет сажать CPU сервера можем отключить через конфиг (но маловероятно)
		if (l_zlib_result == Z_OK)
		{
			if(l_dest_length < p_body.length()) // мелкие пакеты могут стать больше - отказываемся от сжатия.
			{
				l_post_compress_query.resize(l_dest_length); // TODO - нужен ли этот релокейт?
				l_log_string = "Request:  " + Util::toString(p_body.length()) + " / " + Util::toString(l_dest_length);				
			}
			else
			{
				l_post_compress_query.clear();
			}
		}
		else
		{
			l_post_compress_query.clear();
			l_fly_server_log.step("Error zlib: =  " + Util::toString(l_zlib_result));
		}
	}
	}
	const bool l_is_zlib = !l_post_compress_query.empty();
// Передача
	CInternetHandle hSession(InternetOpenA(g_full_user_agent.c_str(),INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
	DWORD l_timeOut = CFlyServerConfig::g_winet_connect_timeout;
	if(l_timeOut <= 500)
	   l_timeOut = 1000;
	if(!InternetSetOption(hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &l_timeOut, sizeof(l_timeOut)))
	{
		l_fly_server_log.step("Error InternetSetOption INTERNET_OPTION_CONNECT_TIMEOUT: " + Util::translateError(GetLastError()));
	}
	InternetSetOption(hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &CFlyServerConfig::g_winet_receive_timeout, sizeof(CFlyServerConfig::g_winet_receive_timeout));
	InternetSetOption(hSession, INTERNET_OPTION_SEND_TIMEOUT, &CFlyServerConfig::g_winet_send_timeout, sizeof(CFlyServerConfig::g_winet_send_timeout));
	if(hSession)
	{
		DWORD dwFlags = 0; //INTERNET_FLAG_NO_COOKIES|INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE|INTERNET_FLAG_PRAGMA_NOCACHE;
		CInternetHandle hConnect(InternetConnectA(hSession, l_Server.getIp().c_str(),l_Server.getPort(), NULL, NULL, INTERNET_SERVICE_HTTP, dwFlags, NULL));
		if(hConnect)
		{
			CInternetHandle hRequest(HttpOpenRequestA(hConnect, "POST", p_query , NULL, NULL, NULL /*g_accept*/, 0, NULL));
			if(hRequest)
			{
				string l_fly_header;
				if(l_Server.getTimeResponse() > CFlyServerConfig::g_winet_min_response_time_for_log)
				{
					l_fly_header = "X-fly-response: " + Util::toString(l_Server.getTimeResponse());
				}
				if(HttpSendRequestA(hRequest, 
					    l_fly_header.length() ? l_fly_header.c_str() : nullptr,
					    l_fly_header.length(),  
						l_is_zlib ? reinterpret_cast<LPVOID>(l_post_compress_query.data()) : LPVOID(p_body.data()), 
						l_is_zlib ? l_post_compress_query.size() : p_body.size()))
				{
					DWORD l_dwBytesAvailable = 0;
					std::vector<char> l_zlib_blob;
					std::vector<unsigned char> l_MessageBody;
					while (InternetQueryDataAvailable(hRequest, &l_dwBytesAvailable, 0, 0))
					{
						if(l_dwBytesAvailable == 0)
							break;
#ifdef _DEBUG
						l_fly_server_log.step(" InternetQueryDataAvailable dwBytesAvailable = " + Util::toString(l_dwBytesAvailable));
#endif
						l_MessageBody.resize(l_dwBytesAvailable + 1);
						DWORD dwBytesRead = 0;
						const BOOL bResult = InternetReadFile(hRequest, l_MessageBody.data(),l_dwBytesAvailable, &dwBytesRead);
						if (!bResult)
						{
							l_fly_server_log.step(" InternetReadFile error " + Util::translateError(GetLastError()));
							break;
						}
						if (dwBytesRead == 0)
							break;
						l_MessageBody[dwBytesRead] = 0;
						const auto l_cur_size = l_zlib_blob.size();
						l_zlib_blob.resize(l_cur_size + dwBytesRead);
						memcpy(l_zlib_blob.data() + l_cur_size, l_MessageBody.data(), dwBytesRead);
					}
#ifdef _DEBUG
/*
std::string l_hex_dump;
				for(unsigned long i=0;i<l_zlib_blob.size(); ++i)
				{
					if(i % 10 == 0)
						l_hex_dump += "\r\n";
					char b[7] = {0};
					sprintf(b, "%#x ", (unsigned char)l_zlib_blob[i] & 0xFF);
					l_hex_dump += b;
				}
					l_fly_server_log.step( "l_hex_dump = " + l_hex_dump);
*/
#endif
					// TODO обобщить в утилитную функцию и подменить в области DHT с другим стартовым коэф-центом. для уменьшения кол-ва релокаций
					// TODO коэф. можно высчитывать динамические и не делалать 10-кой
					if(l_zlib_blob.size())
					{
					if(p_is_disable_zlib_out == true)
					{
						const auto l_cur_size = l_zlib_blob.size();
						l_zlib_blob.resize(l_cur_size + 1);
						l_zlib_blob[l_cur_size] = 0;
						l_result_query = (const char*) l_zlib_blob.data();
						//l_decompress.resize(l_zlib_blob.size());
						//memcpy(l_decompress.data(), l_zlib_blob.data(), l_decompress.size());
					}
					else
					{
					std::vector<unsigned char> l_decompress; // TODO расширять динамически
					unsigned long l_decompress_size = l_zlib_blob.size() * 10;
					l_decompress.resize(l_decompress_size);
					while(true)
					{
					    const int l_un_compress_result = uncompress(l_decompress.data(), &l_decompress_size, (uint8_t*)l_zlib_blob.data(), l_zlib_blob.size());
						l_fly_server_log.step(l_log_string + ", Response: " + Util::toString(l_zlib_blob.size()) + " / " +  Util::toString(l_decompress_size));
						if (l_un_compress_result == Z_BUF_ERROR)
						{
							l_decompress_size *= 2;
							l_decompress.resize(l_decompress_size);
							continue;
						}

						if (l_un_compress_result == Z_OK)
						{
							l_result_query = (const char*) l_decompress.data();
						}
						else
						{
							l_fly_server_log.step(" InternetReadFile - uncompress error! error = " + Util::toString(l_un_compress_result));
							dcassert(l_un_compress_result == Z_OK);
						}
						break;
					}
					}
				    }
					p_is_send = true;
#ifdef _DEBUG
					l_fly_server_log.step(" InternetReadFile Ok! size = " + Util::toString(l_result_query.size()));
#endif
				}
				else
				{
					l_fly_server_log.step("HttpSendRequest error " + Util::translateError(GetLastError()));
				}
			}
			else
			{
				l_fly_server_log.step("HttpOpenRequest error " + Util::translateError(GetLastError()));
			}
		}
		else
		{
			l_fly_server_log.step("InternetConnect error " + Util::translateError(GetLastError()));
		}
	}
	else
	{
		l_fly_server_log.step("InternetOpen error " + Util::translateError(GetLastError()));
	}
	l_Server.setTimeResponse(l_fly_server_log.calcSumTime());
	return l_result_query;
}
//======================================================================================================
string CFlyServerAdapter::CFlyServerJSON::connect(const CFlyServerKeyArray& p_fileInfoArray, bool p_is_fly_set_query )
{
	dcassert(!p_fileInfoArray.empty());
	Thread::ConditionLockerWithSpin l(g_running);
	login(); // Запросим свой ИД у сервера
	// CFlyLog l_log("[flylinkdc-server]",true);
	Json::Value  l_root;   
	Json::Value& l_arrays = l_root["array"];
	l_root["header"]["ID"] = g_fly_server_id;
	int  l_count = 0;
	int  l_count_only_ext_info = 0;
	int  l_count_only_counter  = 0;
	int  l_count_cache  = 0;
	bool is_all_file_only_ext_info = false;
	bool is_all_file_only_counter  = false;
	if(!p_is_fly_set_query) // Для get-запроса все записи хотят только расширенную инфу?
	{
	 for(auto i = p_fileInfoArray.cbegin(); i != p_fileInfoArray.cend(); ++i)
	 {
	  if(i->m_only_ext_info)
		++l_count_only_ext_info;
	  if(i->m_only_counter)
		++l_count_only_counter;
	  if(i->m_is_cache)
	    ++l_count_cache; 
	 }
	 if(l_count_only_counter == p_fileInfoArray.size())
	 {
	   is_all_file_only_counter = true;
	   l_root["only_counter"] = 1; // Поместим признак в корень чтобы не размножать запрос всем.
	 }
	 if(l_count_only_counter > 0 && l_count_only_counter != p_fileInfoArray.size())
	 {
	   l_root["different_counter"] = 1; 
	 }
	 if(l_count_cache)
	 {
	   l_root["cache"] = l_count_cache;  // Для статистики TODO - позже убрать
	 }
	 if(is_all_file_only_counter == false) // Если не просят только счетчики - проставим запросы на расширенную инфу
	 {
	  if(l_count_only_ext_info == p_fileInfoArray.size()) // Полную инфу отдаем только для точечных запросов
	  {
	   is_all_file_only_ext_info = true;
	   l_root["only_ext_info"] = 1; // Поместим признак в корень чтобы не размножать запрос всем.
	  }
	 if(l_count_only_ext_info > 0 && l_count_only_ext_info != p_fileInfoArray.size())
	  {
	   l_root["different_ext_info"] = 1; // Поместим признак различных способов запроса к медиаинфе 
	                                     // чтобы на стороне сервера не делать лишний поиск параметра для каждого файла.
	  }
	 }
	}
	for(auto i = p_fileInfoArray.cbegin(); i != p_fileInfoArray.cend(); ++i)
	{
	// Начинаем создавать JSON пакет для передачи на сервер.
	const string l_tth_base32 = i->m_tth.toBase32();
	dcassert(i->m_file_size && !l_tth_base32.empty());
	if(!i->m_file_size || l_tth_base32.empty())
	{
		LogManager::getInstance()->message("Error !i->m_file_size || l_tth_base32.empty(). i->m_file_size = " +
			        Util::toString(i->m_file_size) 
					+ " l_tth_base32 = " + l_tth_base32 /*+ " i->m_file_name = " + i->m_file_name */);
		continue;
	}
	Json::Value& l_array_item = l_arrays[l_count++];
	l_array_item["tth"]  = l_tth_base32;
	l_array_item["size"] = Util::toString(i->m_file_size);
	 // TODO - l_array_item["name"] = i->m_file_name;  имя пока не помнить. оно разное всегда
	 /* Проброс хитов и времени хеша/файла так-же исключаем
	 if(i->m_hit)
	  l_array_item["hit"] = i->m_hit;
	 if(i->m_time_hash)
	 {
		l_array_item["time_hash"] = i->m_time_hash;
		l_array_item["time_file"] = i->m_time_file;
	 }
	 */
	if (p_is_fly_set_query == false)
	{
		if(is_all_file_only_ext_info == false && i->m_only_ext_info == true)
	        l_array_item["only_ext_info"] = 1;
		if(is_all_file_only_counter == false && i->m_only_counter == true)
	        l_array_item["only_counter"] = 1;
	}
	else
	{
		Json::Value& l_mediaInfo = l_array_item["media"];
		if(!i->m_media.m_audio.empty())
		  l_mediaInfo["fly_audio"] = i->m_media.m_audio;
		if(!i->m_media.m_video.empty())
		  l_mediaInfo["fly_video"] = i->m_media.m_video;
		if(i->m_media.m_bitrate)
		  l_mediaInfo["fly_audio_br"] = i->m_media.m_bitrate;
		if(i->m_media.m_mediaX && i->m_media.m_mediaY)
		{
		  l_mediaInfo["fly_xy"] = i->m_media.getXY();
		}

	if (i->m_media.isMediaAttrExists())
	{
				Json::Value& l_mediaInfo_ext = l_array_item["media-ext"];
				Json::Value* l_ptr_mediaInfo_ext_general = nullptr;
				Json::Value* l_ptr_mediaInfo_ext_audio   = nullptr;
				Json::Value* l_ptr_mediaInfo_ext_video   = nullptr;
				int l_count_audio_channel = 0;
				int l_last_channel = 0;
				for(auto j = i->m_media.m_ext_array.cbegin(); j!= i->m_media.m_ext_array.cend(); ++j)
				{
						if(j->m_stream_type == MediaInfoLib::Stream_Audio)
					{
						if(j->m_channel != l_last_channel)
						{
							l_last_channel = j->m_channel;
							++l_count_audio_channel;
						}
					}
				}
				boost::unordered_map<int,Json::Value*> l_cache_channel;
				for(auto j = i->m_media.m_ext_array.cbegin(); j!= i->m_media.m_ext_array.cend(); ++j)
				{
					if(g_fly_server_config.isSupportTag(j->m_param))
					{
					switch(j->m_stream_type)
					{
					case MediaInfoLib::Stream_General:
						{
							if(!l_ptr_mediaInfo_ext_general)
								l_ptr_mediaInfo_ext_general = &l_mediaInfo_ext["general"];
							Json::Value& l_info = *l_ptr_mediaInfo_ext_general;
							l_info[j->m_param] = j->m_value;
						}
						break;
					case MediaInfoLib::Stream_Video:
						{
							if(!l_ptr_mediaInfo_ext_video)
								l_ptr_mediaInfo_ext_video = &l_mediaInfo_ext["video"];
							Json::Value& l_info = *l_ptr_mediaInfo_ext_video;
							l_info[j->m_param] = j->m_value;
						}
						break;
					case MediaInfoLib::Stream_Audio:
						{
							if(!l_ptr_mediaInfo_ext_audio)
								l_ptr_mediaInfo_ext_audio = &l_mediaInfo_ext["audio"];
							Json::Value& l_info = *l_ptr_mediaInfo_ext_audio;
							if(l_count_audio_channel == 0)
							  {
								  l_info[j->m_param] = j->m_value;
							  }
							else
							  {
								  // Определяем, в какую секцию канала должна попасть информация.
								  uint8_t l_channel_num = j->m_channel;
								  const auto l_ch_item = l_cache_channel.find(l_channel_num);
								  Json::Value* l_channel_info = 0;
								  if(l_ch_item == l_cache_channel.end())
								  {
									  const string l_channel_id = "channel-" + (l_channel_num != CFlyMediaInfo::ExtItem::channel_all ? Util::toString(l_channel_num) : string("all"));
									  l_channel_info = &l_info[l_channel_id];
									  l_cache_channel[l_channel_num] =  l_channel_info;
								  }
								  else
								  {
										  l_channel_info = l_ch_item->second;
								  }
								  (*l_channel_info)[j->m_param] = j->m_value;
								  }
						}
						break;
					}
					}
				}
			}
	  }
	}
// string l_tmp;
 string l_result_query;
 if (l_count > 0)  // Есть что передавать на сервер?
 {
#define FLYLINKDC_USE_HTTP_SERVER
#ifdef FLYLINKDC_USE_HTTP_SERVER
   const std::string l_post_query = l_root.toStyledString();
   bool l_is_send;
   l_result_query = postQuery(p_is_fly_set_query, false, false, false, false, p_is_fly_set_query ? "fly-set" : "fly-zget",l_post_query,l_is_send);
#endif

// #define FLYLINKDC_USE_SOCKET
// #ifndef FLYLINKDC_USE_SOCKET
//  ::MessageBoxA(NULL,outputConfig.c_str(),"json",MB_OK | MB_ICONINFORMATION);
// #endif
#ifdef FLYLINKDC_USE_SOCKET
	unique_ptr<Socket> l_socket(new Socket);
	try
	{
	l_socket->create(Socket::TYPE_TCP);
	l_socket->setBlocking(true);
//#define FLYLINKDC_USE_MEDIAINFO_SERVER_RESOLVE
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_RESOLVE
	const string l_ip = Socket::resolve("flylink.no-ip.org");
	l_log.step("Socket::resolve(flylink.no-ip.org) = ip " + l_ip);
#else
	const string l_ip = g_fly_server_config.m_ip;
#endif
	l_socket->connect(l_ip,g_fly_server_config.m_port); // 
	l_log.step("write");
	const size_t l_size = l_socket->writeAll(outputConfig.c_str(), outputConfig.size(), 500);
	if(l_size != outputConfig.size())
	 {
		 l_log.step("l_socket->write(outputConfig) != outputConfig.size() l_size = " + Util::toString(l_size));
		 return outputConfig;
	 }
	else
	{
		l_log.step("write-OK");
		vector<char> l_buf(64000);
		l_log.step("read");
		l_socket->readAll(&l_buf[0],l_buf.size(),500);
		l_log.step("read-OK");
	}
	l_log.step(outputConfig);
	}
	catch(const SocketException& e)
	{
	l_log.step("Socket error" + e.getError());
	}
#endif
	}
	return l_result_query;
}
//======================================================================================================
string CFlyServerInfo::getMediaInfoAsText(const TTHValue& p_tth,int64_t p_file_size)
{
	CFlyServerKeyArray l_get_array;
	CFlyServerKey l_info(p_tth, p_file_size);
	l_info.m_only_ext_info = true; // Запросим с сервера только расширенную.
	l_get_array.push_back(l_info);
	const string l_json_result = CFlyServerAdapter::CFlyServerJSON::connect(l_get_array, false);
	string l_Infrom;
	Json::Value l_root;
	Json::Reader l_reader;
	const bool l_parsingSuccessful = l_reader.parse(l_json_result, l_root);
	if (!l_parsingSuccessful && !l_json_result.empty())
	{
		l_Infrom = l_json_result;
		LogManager::getInstance()->message("Failed to parse json configuration: l_json_result = " + l_json_result);
	}
	else
	{
		const Json::Value& l_arrays = l_root["array"];
		dcassert(l_arrays.size() == 0 || l_arrays.size() == 1);
		const Json::Value& l_cur_item_in = l_arrays[0U];
		const Json::Value& l_attrs_media_ext = l_cur_item_in["media-ext"];
		const Json::Value& l_attrs_general   = l_attrs_media_ext["general"];
		l_Infrom = l_attrs_general["Inform"].asString();
		const Json::Value& l_attrs_video   = l_attrs_media_ext["video"];
		const string l_InfromVideo = l_attrs_video["Inform"].asString();
		if (!l_InfromVideo.empty())
		{
			l_Infrom += "\r\nVideo ";
			l_Infrom += l_attrs_video["Inform"].asString();
		}
		const Json::Value& l_attrs_audio   = l_attrs_media_ext["audio"];
		int i = 0;
		for (;; ++i)
		{
			const string l_channel_id = "channel-" + Util::toString(i);
			const Json::Value& l_attrs_channel   = l_attrs_audio[l_channel_id];
			const string& l_channel_value = l_attrs_channel["Inform"].asString();
			if (l_channel_value.empty())
				break;
			else
			{
				if (!l_channel_value.empty())
				{
					l_Infrom += "\r\nAudio ";
					l_Infrom += l_channel_value + ":\r\n";
				}
			}
		}
		if (i == 0) // Каналов нет?
		{
			const string l_audio = l_attrs_audio["Inform"].asString();
			if (!l_audio.empty())
			{
				l_Infrom += "\r\nAudio ";
				l_Infrom += l_audio;
			}
		}
	}
	return l_Infrom;
}
//======================================================================================================
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
