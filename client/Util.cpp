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

#include <regex>
#include <Mmsystem.h>
#include <shlobj.h>
#include "CompatibilityManager.h"
#include "ShareManager.h"
#include "idna/idna.h"
#include "MD5Calc.h"
#include <boost/algorithm/string.hpp>

const string g_tth = "TTH:";
const time_t Util::g_startTime = time(NULL);
const string Util::emptyString;
const wstring Util::emptyStringW;
const tstring Util::emptyStringT;

const vector<uint8_t> Util::emptyByteVector;

const string Util::m_dot = ".";
const string Util::m_dot_dot = "..";
const tstring Util::m_dotT = _T(".");
const tstring Util::m_dot_dotT = _T("..");

bool Util::g_away = false;
string Util::g_awayMsg;
time_t Util::g_awayTime;

string Util::g_paths[Util::PATH_LAST];
string Util::g_sysPaths[Util::SYS_PATH_LAST];
NUMBERFMT Util::g_nf = { 0 };
bool Util::g_localMode = true;

static string g_caption = "FlylinkDC++";
static tstring g_captionT = _T("FlylinkDC++");
static bool g_is_load_name = false;
const string getFlylinkDCAppCaption()
{
	if (g_is_load_name == false)
	{
		g_is_load_name = true;
		const auto l_path_local_test_file = Text::toT(Util::getModuleCustomFileName("fly-caption.config"));
		if (File::isExist(l_path_local_test_file))
		{
			string l_caption = File(l_path_local_test_file, File::READ, File::OPEN).read();
			if (l_caption.length() < 30 && l_caption.length() > 2)
			{
				Text::replace_all(l_caption, "\r", "");
				Text::replace_all(l_caption, "\n", "");
				Text::replace_all(l_caption, "\t", "");
				Text::replace_all(l_caption, " ", "");
				boost::trim(l_caption);
				Text::acpToWide(l_caption, g_captionT, Text::g_code1251);
				g_caption = Text::fromT(g_captionT);
			}
		}
	}
	return g_caption;
}

const tstring getFlylinkDCAppCaptionT()
{
	if (g_is_load_name == false)
	{
		getFlylinkDCAppCaption();
	}
	return g_captionT;
}

const string getFlylinkDCAppCaptionWithVersion()
{
	return getFlylinkDCAppCaption() + " " + A_VERSIONSTRING;
}
const tstring getFlylinkDCAppCaptionWithVersionT()
{
	return Text::toT(getFlylinkDCAppCaptionWithVersion());
}


tstring g_full_user_agent = Text::toT(string(getFlylinkDCAppCaptionWithVersion()
#ifdef _DEBUG
                                             + " DEBUG"
#else
                                             + ""
#endif
                                            ));


static void sgenrand(unsigned long seed);

extern "C" void bz_internal_error(int errcode)
{
	dcdebug("bzip2 internal error: %d\n", errcode);
	// TODO - логирование?
}

#if (_MSC_VER >= 1400 )
void WINAPI invalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	//do nothing, this exist because vs2k5 crt needs it not to crash on errors.
}
#endif
bool Util::checkForbidenFolders(const string& p_path)
{
	// don't share Windows directory
	if (Util::locatedInSysPath(Util::WINDOWS, p_path) ||
	        Util::locatedInSysPath(Util::APPDATA, p_path) ||
	        Util::locatedInSysPath(Util::LOCAL_APPDATA, p_path) ||
	        Util::locatedInSysPath(Util::PROGRAM_FILES, p_path) ||
	        Util::locatedInSysPath(Util::PROGRAM_FILESX86, p_path))
		return true;
	else
		return false;
}
bool Util::locatedInSysPath(Util::SysPaths path, const string& currentPath)
{
	const string& l_path = g_sysPaths[path];
	// dcassert(!l_path.empty());
	return !l_path.empty() && strnicmp(currentPath, l_path, l_path.size()) == 0;
}

void Util::intiProfileConfig()
{
	g_paths[PATH_USER_CONFIG] = getSysPath(APPDATA) + "FlylinkDC++" PATH_SEPARATOR_STR;
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
	g_paths[PATH_ALL_USER_CONFIG] = getSysPath(COMMON_APPDATA) + "FlylinkDC++" PATH_SEPARATOR_STR;
# endif
}
static const string g_configFileLists[] =
{
	"ADLSearch.xml",
	"DCPlusPlus.xml",
	"Favorites.xml",
	"IPTrust.ini",
#ifdef SSA_IPGRANT_FEATURE
	"IPGrant.ini",
#endif
	"IPGuard.ini"
};
void Util::moveSettings()
{
	const string bkpath = g_paths[PATH_USER_CONFIG];
	const string sourcepath = g_paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR;
	File::ensureDirectory(bkpath);
	for (size_t i = 0; i < _countof(g_configFileLists); ++i)
	{
		if (!File::isExist(bkpath + g_configFileLists[i]) && File::isExist(sourcepath + g_configFileLists[i]))
		{
			try
			{
				File::copyFile(sourcepath + g_configFileLists[i], bkpath + g_configFileLists[i]);
			}
			catch (const FileException & e)
			{
				const string l_error = "Error [Util::moveSettings] File::copyFile = sourcepath + FileList[i] = " + sourcepath + g_configFileLists[i]
				                       + " , bkpath + FileList[i] = " + bkpath + g_configFileLists[i] + " error = " + e.getError();
				LogManager::message(l_error);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
				//CFlyServerJSON::pushError(12, "Error = " + l_error);
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
			}
		}
	}
}
const string Util::getModuleCustomFileName(const string& p_file_name)
{
	string l_path = Util::getFilePath(Text::fromT(Util::getModuleFileName()));
	l_path += p_file_name;
	return l_path;
}
const tstring Util::getModuleFileName()
{
	static tstring g_module_file_name;
	if (g_module_file_name.empty())
	{
		LocalArray<TCHAR, MAX_PATH> l_buf;
		const DWORD x = GetModuleFileName(NULL, l_buf.data(), MAX_PATH);
		g_module_file_name = tstring(l_buf.data(), x);
	}
	return g_module_file_name;
}

void Util::initialize()
{
#ifdef _DEBUG
//	CFlyServerJSON::pushError(11, "Test init!");
#endif
	Text::initialize();
	
	sgenrand((unsigned long)time(NULL));
	
#if (_MSC_VER >= 1400)
	_set_invalid_parameter_handler(reinterpret_cast<_invalid_parameter_handler>(invalidParameterHandler));
#endif
	static TCHAR g_sep[2] = _T(",");
	static wchar_t g_Dummy[16] = { 0 };
	g_nf.lpDecimalSep = g_sep;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SGROUPING, g_Dummy, 16);
	g_nf.Grouping = _wtoi(g_Dummy);
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, g_Dummy, 16);
	g_nf.lpThousandSep = g_Dummy;
	
	g_paths[PATH_EXE] = Util::getModuleCustomFileName("");
	LocalArray<TCHAR, MAX_PATH> l_buf;
#define SYS_WIN_PATH_INIT(path) \
	if(::SHGetFolderPath(NULL, CSIDL_##path, NULL, SHGFP_TYPE_CURRENT, l_buf.data()) == S_OK) \
	{ \
		g_sysPaths[path] = Text::fromT(l_buf.data()) + PATH_SEPARATOR; \
	} \
	
	//LogManager::message("Sysytem Path: " + g_sysPaths[path]);
	//LogManager::message("Error SHGetFolderPath: GetLastError() = " + Util::toString(GetLastError()));
	
	SYS_WIN_PATH_INIT(WINDOWS);
	SYS_WIN_PATH_INIT(PROGRAM_FILESX86);
	SYS_WIN_PATH_INIT(PROGRAM_FILES);
	if (CompatibilityManager::runningIsWow64())
	{
		// [!] Correct PF path on 64 bit system with run 32 bit programm.
		const char* l_PFW6432 = getenv("ProgramW6432");
		if (l_PFW6432)
		{
			g_sysPaths[PROGRAM_FILES] = string(l_PFW6432) + PATH_SEPARATOR;
		}
	}
	SYS_WIN_PATH_INIT(APPDATA);
	SYS_WIN_PATH_INIT(LOCAL_APPDATA);
	SYS_WIN_PATH_INIT(COMMON_APPDATA);
	SYS_WIN_PATH_INIT(PERSONAL);
	
#undef SYS_WIN_PATH_INIT
	// Global config path is FlylinkDC++ executable path...
#ifdef _DEBUG2 // Тестируем запрет доступа
	//g_paths[PATH_EXE] = "C:\\Program Files (x86)\\f\\";
	//g_paths[PATH_GLOBAL_CONFIG] = g_paths[PATH_EXE];
#else
	g_paths[PATH_GLOBAL_CONFIG] = g_paths[PATH_EXE];
#endif
#ifdef USE_APPDATA
	if (File::isExist(g_paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR "DCPlusPlus.xml") ||
	        !(locatedInSysPath(PROGRAM_FILES, g_paths[PATH_EXE]) || locatedInSysPath(PROGRAM_FILESX86, g_paths[PATH_EXE]))
	   )
	{
		// Проверим права записи
		g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
		const auto l_marker_file = g_paths[PATH_USER_CONFIG] + ".flylinkdc-test-readonly.tmp";
		try
		{
			{
				File l_f_ro_test(l_marker_file, File::WRITE, File::CREATE | File::TRUNCATE);
			}
			File::deleteFile(l_marker_file);
		}
		catch (const FileException&)
		{
			const DWORD l_error = GetLastError();
			if (l_error == 5)
			{
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
				//CFlyServerJSON::pushError(11, "Error create/write + " + l_marker_file);
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
				intiProfileConfig();
				// Если возможно уносим настройки в профиль (если их тамеще нет)
				moveSettings();
			}
		}
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		g_paths[PATH_ALL_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
# endif
	}
	else
	{
		intiProfileConfig();
	}
#else // USE_APPDATA
	g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
#endif //USE_APPDATA    
	g_paths[PATH_LANGUAGES] = g_paths[PATH_GLOBAL_CONFIG] + "Lang" PATH_SEPARATOR_STR;
	
#ifdef FLYLINKDC_USE_EXTERNAL_MAIN_ICON
	g_paths[PATH_EXTERNAL_ICO] = g_paths[PATH_GLOBAL_CONFIG] + "FlylinkDC.ico";
#endif
	g_paths[PATH_EXTERNAL_LOGO] = g_paths[PATH_GLOBAL_CONFIG] + "FlylinkDC.png";
	
	g_paths[PATH_THEMES] = g_paths[PATH_GLOBAL_CONFIG] + "Themes" PATH_SEPARATOR_STR;
	
	g_paths[PATH_GPGPU] = g_paths[PATH_GLOBAL_CONFIG] + "GPUProgramms" PATH_SEPARATOR_STR;
	
	g_paths[PATH_SOUNDS] = g_paths[PATH_GLOBAL_CONFIG] + "Sounds" PATH_SEPARATOR_STR;
	
	loadBootConfig();
	
	if (!File::isAbsolute(g_paths[PATH_USER_CONFIG]))
	{
		g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + g_paths[PATH_USER_CONFIG];
	}
	
	g_paths[PATH_USER_CONFIG] = validateFileName(g_paths[PATH_USER_CONFIG]);
	
	if (g_localMode)
	{
		g_paths[PATH_USER_LOCAL] = g_paths[PATH_USER_CONFIG];
	}
	else
	{
		if (!getSysPath(PERSONAL).empty())
		{
			g_paths[PATH_USER_CONFIG] = getSysPath(PERSONAL) + "FlylinkDC++" PATH_SEPARATOR_STR;
		}
	
		g_paths[PATH_USER_LOCAL] = !getSysPath(PERSONAL).empty() ? getSysPath(PERSONAL) + "FlylinkDC++" PATH_SEPARATOR_STR : g_paths[PATH_USER_CONFIG];
	}
	
	g_paths[PATH_DOWNLOADS] = getDownloadPath(CompatibilityManager::getDefaultPath());
//	g_paths[PATH_RESOURCES] = exePath;
	g_paths[PATH_WEB_SERVER] = g_paths[PATH_EXE] + "WEBserver" PATH_SEPARATOR_STR;
	
	g_paths[PATH_FILE_LISTS] = g_paths[PATH_USER_LOCAL] + "FileLists" PATH_SEPARATOR_STR;
	g_paths[PATH_HUB_LISTS] = g_paths[PATH_USER_LOCAL] + "HubLists" PATH_SEPARATOR_STR;
	g_paths[PATH_NOTEPAD] = g_paths[PATH_USER_CONFIG] + "Notepad.txt";
	g_paths[PATH_EMOPACKS] = g_paths[PATH_GLOBAL_CONFIG] + "EmoPacks" PATH_SEPARATOR_STR;
	
	
	shrink_to_fit(&g_paths[0], &g_paths[PATH_LAST]);
	shrink_to_fit(&g_sysPaths[0], &g_sysPaths[SYS_PATH_LAST]);
	
	
	File::ensureDirectory(g_paths[PATH_USER_CONFIG]);
	File::ensureDirectory(g_paths[PATH_USER_LOCAL]);
	File::ensureDirectory(getTempPath()); // airdc++
}
//==========================================================================
static const char* g_countryCodes[] = // TODO: needs update this table! http://en.wikipedia.org/wiki/ISO_3166-1
{
	"AD", "AE", "AF", "AG", "AI", "AL", "AM", "AN", "AO", "AQ", "AR", "AS", "AT", "AU", "AW", "AX", "AZ", "BA", "BB",
	"BD", "BE", "BF", "BG", "BH", "BI", "BJ", "BM", "BN", "BO", "BR", "BS", "BT", "BV", "BW", "BY", "BZ", "CA", "CC",
	"CD", "CF", "CG", "CH", "CI", "CK", "CL", "CM", "CN", "CO", "CR", "CS", "CU", "CV", "CX", "CY", "CZ", "DE", "DJ",
	"DK", "DM", "DO", "DZ", "EC", "EE", "EG", "EH", "ER", "ES", "ET", "EU", "FI", "FJ", "FK", "FM", "FO", "FR", "GA",
	"GB", "GD", "GE", "GF", "GG", "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS", "GT", "GU", "GW", "GY", "HK",
	"HM", "HN", "HR", "HT", "HU", "ID", "IE", "IL", "IM", "IN", "IO", "IQ", "IR", "IS", "IT", "JE", "JM", "JO", "JP",
	"KE", "KG", "KH", "KI", "KM", "KN", "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LI", "LK", "LR", "LS", "LT",
	"LU", "LV", "LY", "MA", "MC", "MD", "ME", "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ", "MR", "MS", "MT",
	"MU", "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE", "NF", "NG", "NI", "NL", "NO", "NP", "NR", "NU", "NZ", "OM",
	"PA", "PE", "PF", "PG", "PH", "PK", "PL", "PM", "PN", "PR", "PS", "PT", "PW", "PY", "QA", "RE", "RO", "RS", "RU",
	"RW", "SA", "SB", "SC", "SD", "SE", "SG", "SH", "SI", "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "ST", "SV", "SY",
	"SZ", "TC", "TD", "TF", "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW", "TZ", "UA", "UG",
	"UM", "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI", "VN", "VU", "WF", "WS", "YE", "YT", "YU", "ZA", "ZM", "ZW"
};
//==========================================================================
const char* Util::getCountryShortName(uint16_t p_flag_index)
{
	if (p_flag_index < _countof(g_countryCodes))
		return g_countryCodes[p_flag_index];
	else
		return "";
}
//==========================================================================
int Util::getFlagIndexByCode(uint16_t p_countryCode)
{
	// country codes are sorted, use binary search for better performance
	int begin = 0;
	int end = _countof(g_countryCodes) - 1;
	
	while (begin <= end)
	{
		const int mid = (begin + end) / 2;
		const int cmp = memcmp(&p_countryCode, g_countryCodes[mid], 2);
	
		if (cmp > 0)
			begin = mid + 1;
		else if (cmp < 0)
			end = mid - 1;
		else
			return mid + 1;
	}
	return 0;
}
//==========================================================================
void Util::loadIBlockList()
{
	// https://www.iblocklist.com/
	CFlyLog l_log("[iblocklist.com]");
	const string fileName = getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
	                            true
#endif
	                        ) + "iblocklist-com.ini";
	
	try
	{
		const uint64_t l_timeStampFile = File::getSafeTimeStamp(fileName);
		const uint64_t l_timeStampDb = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_TimeStampIBlockListCom);
		if (l_timeStampFile != l_timeStampDb)
		{
			const string l_data = File(fileName, File::READ, File::OPEN).read();
			l_log.step("read:" + fileName);
			size_t linestart = 0;
			size_t lineend = 0;
			CFlyP2PGuardArray l_sqlite_array;
			l_sqlite_array.reserve(19000);
			for (;;)
			{
				lineend = l_data.find('\n', linestart);
				if (lineend == string::npos)
					break;
				if (lineend == linestart)
				{
					linestart++;
					continue;
				}
				const string l_currentLine = l_data.substr(linestart, lineend - linestart);
				linestart = lineend + 1;
				size_t ip_range_start = l_currentLine.find(':');
				if (ip_range_start == string::npos)
					continue;
				uint32_t a = 0, b = 0, c = 0, d = 0, a2 = 0, b2 = 0, c2 = 0, d2 = 0;
				const int l_Items = sscanf_s(l_currentLine.c_str() + ip_range_start + 1, "%u.%u.%u.%u-%u.%u.%u.%u", &a, &b, &c, &d, &a2, &b2, &c2, &d2);
				if (l_Items == 8)
				{
					const uint32_t l_startIP = (a << 24) + (b << 16) + (c << 8) + d;
					const uint32_t l_endIP = (a2 << 24) + (b2 << 16) + (c2 << 8) + d2;
					if (l_startIP > l_endIP)
					{
						dcassert(0);
						l_log.step("Error parse : " + STRING(INVALID_RANGE) + " Line: " + l_currentLine);
					}
					else
					{
						if (ip_range_start)
						{
							const string l_note = l_currentLine.substr(0, ip_range_start);
							dcassert(!l_note.empty())
							l_sqlite_array.push_back(CFlyP2PGuardIP(l_note, l_startIP, l_endIP));
						}
					}
				}
				else
				{
					dcassert(0);
					l_log.step("Error parse: " + l_currentLine);
				}
			}
			{
				CFlyLog l_geo_log_sqlite("[iblocklist.com-sqlite]");
				CFlylinkDBManager::getInstance()->save_p2p_guard(l_sqlite_array, "", 3);
			}
			CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_TimeStampIBlockListCom, l_timeStampFile);
		}
	}
	catch (const FileException&)
	{
		LogManager::message("Error open " + fileName);
	}
}
	
void Util::loadP2PGuard()
{
	CFlyLog l_log("[P2P Guard]");
	/*
	What steps will reproduce the problem?
	Please add support for external IPFilter lists such ipfilter.dat(utorrent format) or guarding.p2p(emule format)
	For example, it could be found here: http://upd.emule-security.org/ipfilter.zip
	Homepage: http://emule-security.org
	*/
	
	const string fileName = getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
	                            true
#endif
	                        ) + "P2PGuard.ini";
	
	try
	{
		const uint64_t l_timeStampFile  = File::getSafeTimeStamp(fileName);
		const uint64_t l_timeStampDb = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_TimeStampP2PGuard);
		if (l_timeStampFile != l_timeStampDb)
		{
			const string l_data = File(fileName, File::READ, File::OPEN).read();
			l_log.step("read:" + fileName);
			size_t linestart = 0;
			size_t lineend = 0;
			CFlyP2PGuardArray l_sqlite_array;
			l_sqlite_array.reserve(220000);
			for (;;)
			{
				lineend = l_data.find('\n', linestart);
				if (lineend == string::npos)
					break;
				if (lineend == linestart)
				{
					linestart++;
					continue;
				}
				const string l_currentLine = l_data.substr(linestart, lineend - linestart - 1);
				linestart = lineend + 1;
				uint32_t a = 0, b = 0, c = 0, d = 0, a2 = 0, b2 = 0, c2 = 0, d2 = 0;
				const int l_Items = sscanf_s(l_currentLine.c_str(), "%u.%u.%u.%u-%u.%u.%u.%u", &a, &b, &c, &d, &a2, &b2, &c2, &d2);
				if (l_Items == 8)
				{
					const uint32_t l_startIP = (a << 24) + (b << 16) + (c << 8) + d;
					const uint32_t l_endIP = (a2 << 24) + (b2 << 16) + (c2 << 8) + d2;
					if (l_startIP > l_endIP)
					{
						dcassert(0);
						l_log.step("Error parse : " + STRING(INVALID_RANGE) + " Line: " + l_currentLine);
					}
					else
					{
						const auto l_pos = l_currentLine.find(' ');
						if (l_pos != string::npos)
						{
							const string l_note =  l_currentLine.substr(l_pos + 1);
							dcassert(!l_note.empty())
							l_sqlite_array.push_back(CFlyP2PGuardIP(l_note, l_startIP, l_endIP));
						}
					}
				}
				else
				{
					dcassert(0);
					l_log.step("Error parse: " + l_currentLine);
				}
			}
			{
				CFlyLog l_geo_log_sqlite("[P2P Guard-sqlite]");
				CFlylinkDBManager::getInstance()->save_p2p_guard(l_sqlite_array, "", 2);
			}
			CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_TimeStampP2PGuard, l_timeStampFile);
		}
	}
	catch (const FileException&)
	{
		LogManager::message("Error open " + fileName);
	}
}
	
//==========================================================================
#ifdef FLYLINKDC_USE_GEO_IP
void Util::loadGeoIp()
{
	{
		CFlyLog l_log("[GeoIP]");
		// This product includes GeoIP data created by MaxMind, available from http://maxmind.com/
		// Updates at http://www.maxmind.com/app/geoip_country
		const string fileName = getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		                            true
#endif
		                        ) + "GeoIPCountryWhois.csv";
	
		try
		{
			const uint64_t l_timeStampFile  = File::getSafeTimeStamp(fileName);
			const uint64_t l_timeStampDb = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_TimeStampGeoIP);
			if (l_timeStampFile != l_timeStampDb)
			{
				const string data = File(fileName, File::READ, File::OPEN).read();
				l_log.step("read:" + fileName);
				const char* start = data.c_str();
				size_t linestart = 0;
				size_t lineend = 0;
				uint32_t startIP = 0, stopIP = 0;
				uint16_t flagIndex = 0;
				CFlyLocationIPArray l_sqlite_array;
				l_sqlite_array.reserve(100000);
				while (true)
				{
					auto pos = data.find(',', linestart);
					if (pos == string::npos) break;
					pos = data.find(',', pos + 6); // тут можно прибавлять не 1 а 6 т.к. минимальная длина IP в виде текста равна 7 символам "1.1.1.1"
					if (pos == string::npos) break;
					startIP = toUInt32(start + pos + 2);
	
					pos = data.find(',', pos + 7); // тут можно прибавлять не 1 а 7 т.к. минимальная длина IP в виде числа равна 8 символам 1.0.0.0 = 16777216
					if (pos == string::npos) break;
					stopIP = toUInt32(start + pos + 2);
	
					pos = data.find(',', pos + 7); // тут можно прибавлять не 1 а 7 т.к. минимальная длина IP в виде числа равна 8 символам 1.0.0.0 = 16777216
					if (pos == string::npos) break;
					flagIndex = getFlagIndexByCode(*reinterpret_cast<const uint16_t*>(start + pos + 2));
					pos = data.find(',', pos + 1);
					if (pos == string::npos) break;
					lineend = data.find('\n', pos);
					if (lineend == string::npos) break;
					pos += 2;
					l_sqlite_array.push_back(CFlyLocationIP(data.substr(pos, lineend - 1 - pos), startIP, stopIP, flagIndex));
					linestart = lineend + 1;
				}
				{
					CFlyLog l_geo_log_sqlite("[GeoIP-sqlite]");
					CFlylinkDBManager::getInstance()->save_geoip(l_sqlite_array);
				}
				CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_TimeStampGeoIP, l_timeStampFile);
			}
		}
		catch (const FileException&)
		{
			LogManager::message("Error open " + fileName);
		}
	}
}
#endif
	
void customLocationLog(const string& p_line, const string& p_error)
{
	if (BOOLSETTING(LOG_CUSTOM_LOCATION))
	{
		StringMap params;
		params["line"] = p_line;
		params["error"] = p_error;
		LOG(CUSTOM_LOCATION, params);
	}
}
	
void Util::loadCustomlocations()
{
	const tstring l_fileName = Text::toT(getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
	                                         true
#endif
	                                     )) + _T("CustomLocations.ini");
	const uint64_t l_timeStampFile = File::getSafeTimeStamp(Text::fromT(l_fileName)); // TOOD - fix fromT
	const uint64_t l_timeStampDb = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_TimeStampCustomLocation);
	if (l_timeStampFile != l_timeStampDb)
	{
		std::ifstream l_file(l_fileName.c_str());
		string l_currentLine;
		if (l_file.is_open())
		{
			CFlyLog l_log("[CustomLocations.ini]");
			CFlyLocationIPArray l_sqliteArray;
			l_sqliteArray.reserve(6000);
	
			auto parseValidLine = [](CFlyLocationIPArray & p_sqliteArray, const string & p_line, uint32_t p_startIp, uint32_t p_endIp) -> void
			{
				const string::size_type l_space = p_line.find(' ');
				if (l_space != string::npos)
				{
					string l_fullNetStr = p_line.substr(l_space + 1); //TODO Crash
					boost::trim(l_fullNetStr);
					const auto l_comma = l_fullNetStr.find(',');
					if (l_comma != string::npos)
					{
						p_sqliteArray.push_back(CFlyLocationIP(l_fullNetStr.substr(l_comma + 1), p_startIp, p_endIp, Util::toInt(l_fullNetStr.c_str())));
					}
					else
					{
						customLocationLog(p_line, STRING(COMMA_NOT_FOUND));
					}
				}
				else
				{
					customLocationLog(p_line, STRING(SPACE_NOT_FOUND));
				}
			};
	
			try
			{
				uint32_t a = 0, b = 0, c = 0, d = 0, a2 = 0, b2 = 0, c2 = 0, d2 = 0, n = 0;
				bool l_end_file;
				do
				{
					l_end_file = getline(l_file, l_currentLine).eof();
	
					if (!l_currentLine.empty() && isdigit((unsigned char)l_currentLine[0]))
					{
						if (l_currentLine.find('-') != string::npos && count(l_currentLine.begin(), l_currentLine.end(), '.') >= 6)
						{
							const int l_Items = sscanf_s(l_currentLine.c_str(), "%u.%u.%u.%u-%u.%u.%u.%u", &a, &b, &c, &d, &a2, &b2, &c2, &d2);
							if (l_Items == 8)
							{
								const uint32_t l_startIP = (a << 24) + (b << 16) + (c << 8) + d;
								const uint32_t l_endIP = (a2 << 24) + (b2 << 16) + (c2 << 8) + d2 + 1;
								if (l_startIP >= l_endIP)
								{
									customLocationLog(l_currentLine, STRING(INVALID_RANGE));
								}
								else
								{
									parseValidLine(l_sqliteArray, l_currentLine, l_startIP, l_endIP);
								}
							}
							else
							{
								customLocationLog(l_currentLine, STRING(MASK_NOT_FOUND) + " d.d.d.d-d.d.d.d");
							}
						}
						else if (l_currentLine.find('+') != string::npos && count(l_currentLine.begin(), l_currentLine.end(), '.') >= 3)
						{
							const int l_Items = sscanf_s(l_currentLine.c_str(), "%u.%u.%u.%u+%u", &a, &b, &c, &d, &n);
							if (l_Items == 5)
							{
								const uint32_t l_startIP = (a << 24) + (b << 16) + (c << 8) + d;
								parseValidLine(l_sqliteArray, l_currentLine, l_startIP, l_startIP + n);
							}
							else
							{
								customLocationLog(l_currentLine, STRING(MASK_NOT_FOUND) + " d.d.d.d+d");
							}
						}
					}
				}
				while (!l_end_file);
			}
			catch (const Exception& e)
			{
				customLocationLog(l_currentLine, "Parser fatal error:" + e.getError());
			}
			{
				CFlyLog l_logSqlite("[CustomLocation-sqlite]");
				CFlylinkDBManager::getInstance()->save_location(l_sqliteArray);
			}
			CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_TimeStampCustomLocation, l_timeStampFile);
		}
		else
		{
			LogManager::message("Error open " + Text::fromT(l_fileName));
		}
	}
}
	
void Util::migrate(const string& p_file)
{
	if (g_localMode)
		return;
	if (File::getSize(p_file) != -1)
		return;
	string fname = getFileName(p_file);
	string old = g_paths[PATH_GLOBAL_CONFIG] + "Settings\\" + fname;
	if (File::getSize(old) == -1)
		return;
	LogManager::message("Util::migrate old = " + old + " new = " + p_file);
	File::renameFile(old, p_file);
}
	
void Util::loadBootConfig()
{
	// Load boot settings
	try
	{
		SimpleXML boot;
		boot.fromXML(File(getPath(PATH_GLOBAL_CONFIG) + "dcppboot.xml", File::READ, File::OPEN).read());
		boot.stepIn();
	
		if (boot.findChild("LocalMode"))
		{
			g_localMode = boot.getChildData() != "0";
		}
		boot.resetCurrentChild();
		// [!] FlylinkDC Dont merge this code from another projects!!!!!
		StringMap params;
#ifdef _WIN32
		// @todo load environment variables instead? would make it more useful on *nix
	
		params["APPDATA"] = RemovePathSeparator(getSysPath(APPDATA));
		params["LOCAL_APPDATA"] = RemovePathSeparator(getSysPath(LOCAL_APPDATA));
		params["COMMON_APPDATA"] = RemovePathSeparator(getSysPath(COMMON_APPDATA));
		params["PERSONAL"] = RemovePathSeparator(getSysPath(PERSONAL));
		params["PROGRAM_FILESX86"] = RemovePathSeparator(getSysPath(PROGRAM_FILESX86));
		params["PROGRAM_FILES"] = RemovePathSeparator(getSysPath(PROGRAM_FILES));
#endif
	
		if (boot.findChild("ConfigPath"))
		{
	
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
			g_paths[PATH_ALL_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			AppendPathSeparator(g_paths[PATH_ALL_USER_CONFIG]);
#endif
			g_paths[PATH_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			AppendPathSeparator(g_paths[PATH_USER_CONFIG]);
		}
#ifdef USE_APPDATA
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		boot.resetCurrentChild();
	
		if (boot.findChild("SharedConfigPath"))
		{
			g_paths[PATH_ALL_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			AppendPathSeparator(g_paths[PATH_ALL_USER_CONFIG]);
		}
# endif
		boot.resetCurrentChild();
	
		if (boot.findChild("UserConfigPath"))
		{
			g_paths[PATH_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			AppendPathSeparator(g_paths[PATH_USER_CONFIG]);
		}
#endif
	}
	catch (const Exception&)
	{
		//-V565
		// Unable to load boot settings...
	}
}
	
#ifdef _WIN32
static const char g_badChars[] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, '<', '>', '/', '"', '|', '?', '*', 0
};
#else
	
static const char g_badChars[] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, '<', '>', '\\', '"', '|', '?', '*', 0
};
#endif
void Util::fixFileNameMaxPathLimit(string& p_File)
{
	const int l_limit = MAX_PATH - 46 - 10;
	if (p_File.length() >= l_limit) // 46 it one character first dot + 39 characters TTH + 6 characters .dctmp
	{
		const string l_orig_file = p_File;
		string l_ext = Util::getFileExt(p_File);
		p_File       = p_File.erase(l_limit);
		p_File  += l_ext;
		dcassert(p_File == Util::validateFileName(p_File));
		LogManager::message("Fix MAX_PATH limit [" + l_orig_file + "] convert -> [" + p_File + "]");
	}
}
/**
 * Replaces all strange characters in a file with '_'
 * @todo Check for invalid names such as nul and aux...
 */
string Util::validateFileName(string tmp)
{
	string::size_type i = 0;
	
	// First, eliminate forbidden chars
	while ((i = tmp.find_first_of(g_badChars, i)) != string::npos)
	{
		tmp[i] = '_';
		i++;
	}
	
	// Then, eliminate all ':' that are not the second letter ("c:\...")
	i = 0;
	while ((i = tmp.find(':', i)) != string::npos)
	{
		if (i == 1)
		{
			i++;
			continue;
		}
		tmp[i] = '_';
		i++;
	}
	
	// Remove the .\ that doesn't serve any purpose
	i = 0;
	while ((i = tmp.find("\\.\\", i)) != string::npos)
	{
		tmp.erase(i + 1, 2);
	}
	i = 0;
	while ((i = tmp.find("/./", i)) != string::npos)
	{
		tmp.erase(i + 1, 2);
	}
	
	// Remove any double \\ that are not at the beginning of the path...
	i = 1;
	while ((i = tmp.find("\\\\", i)) != string::npos)
	{
		tmp.erase(i + 1, 1);
	}
	i = 1;
	while ((i = tmp.find("//", i)) != string::npos)
	{
		tmp.erase(i + 1, 1);
	}
	
	// And last, but not least, the infamous ..\! ...
	i = 0;
	while (((i = tmp.find("\\..\\", i)) != string::npos))
	{
		tmp[i + 1] = '_';
		tmp[i + 2] = '_';
		tmp[i + 3] = '_';
		i += 2;
	}
	i = 0;
	while (((i = tmp.find("/../", i)) != string::npos))
	{
		tmp[i + 1] = '_';
		tmp[i + 2] = '_';
		tmp[i + 3] = '_';
		i += 2;
	}
	
	// Dots at the end of path names aren't popular
	i = 0;
	while (((i = tmp.find(".\\", i)) != string::npos))
	{
		if (i != 0)
			tmp[i] = '_';
		i += 1;
	}
	i = 0;
	while (((i = tmp.find("./", i)) != string::npos))
	{
		if (i != 0)
			tmp[i] = '_';
		i += 1;
	}
	
	
	return tmp;
}
	
string Util::cleanPathChars(string aNick)
{
	string::size_type i = 0;
	
	while ((i = aNick.find_first_of("/.\\", i)) != string::npos)
	{
		aNick[i] = '_';
	}
	return aNick;
}
	
string Util::getShortTimeString(time_t t)
{
	/*
#ifdef _DEBUG
	    static int l_count = 0;
	    if (t == time(NULL))
	        dcdebug("\n\n\nUtil::getShortTimeString called with curent time. Count = %d\n\n\n", ++l_count);
#endif
	*/
	tm* _tm = localtime(&t);
	if (_tm == NULL)
	{
		return "xx:xx";
	}
	else
	{
		string l_buf;
		l_buf.resize(255);
		l_buf.resize(strftime(&l_buf[0], l_buf.size(), SETTING(TIME_STAMPS_FORMAT).c_str(), _tm));
#ifdef _WIN32
		if (!Text::validateUtf8(l_buf))
			return Text::toUtf8(l_buf);
		else
			return l_buf;
#else
		return Text::toUtf8(l_buf);
#endif
	}
}
	
/**
 * Decodes a URL the best it can...
 * Default ports:
 * http:// -> port 80
 * https:// -> port 443
 * dchub:// -> port 411
 */
void Util::decodeUrl(const string& url, string& protocol, string& host, uint16_t& port, string& path, bool& isSecure, string& query, string& fragment)
{
	auto fragmentEnd = url.size();
	auto fragmentStart = url.rfind('#');
	
	size_t queryEnd;
	if (fragmentStart == string::npos)
	{
		queryEnd = fragmentStart = fragmentEnd;
	}
	else
	{
		dcdebug("f");
		queryEnd = fragmentStart;
		fragmentStart++;
	}
	
	auto queryStart = url.rfind('?', queryEnd);
	size_t fileEnd;
	
	if (queryStart == string::npos)
	{
		fileEnd = queryStart = queryEnd;
	}
	else
	{
		dcdebug("q");
		fileEnd = queryStart;
		queryStart++;
	}
	
	size_t protoStart = 0;
	auto protoEnd = url.find("://", protoStart);
	
	auto authorityStart = protoEnd == string::npos ? protoStart : protoEnd + 3;
	auto authorityEnd = url.find_first_of("/#?", authorityStart);
	
	size_t fileStart;
	if (authorityEnd == string::npos)
	{
		authorityEnd = fileStart = fileEnd;
	}
	else
	{
		dcdebug("a");
		fileStart = authorityEnd;
	}
	
	protocol = (protoEnd == string::npos ? Util::emptyString : Text::toLower(url.substr(protoStart, protoEnd - protoStart)));
	if (protocol.empty())
		protocol = "dchub";
	
	if (authorityEnd > authorityStart)
	{
		dcdebug("x");
		size_t portStart = string::npos;
		if (url[authorityStart] == '[')
		{
			// IPv6?
			auto hostEnd = url.find(']');
			if (hostEnd == string::npos)
			{
				return;
			}
	
			host = url.substr(authorityStart + 1, hostEnd - authorityStart - 1);
			if (hostEnd + 1 < url.size() && url[hostEnd + 1] == ':')
			{
				portStart = hostEnd + 2;
			}
		}
		else
		{
			size_t hostEnd;
			portStart = url.find(':', authorityStart);
			if (portStart != string::npos && portStart > authorityEnd)
			{
				portStart = string::npos;
			}
	
			if (portStart == string::npos)
			{
				hostEnd = authorityEnd;
			}
			else
			{
				hostEnd = portStart;
				portStart++;
			}
	
			dcdebug("h");
			host = Text::toLower(url.substr(authorityStart, hostEnd - authorityStart));
		}
	
		if (portStart == string::npos)
		{
			if (protocol == "dchub")
			{
				port = 411;
			}
			else if (protocol == "nmdcs")
			{
				isSecure = true;
				port = 411;
			}
			else if (protocol == "http" || protocol == "steam")
			{
				port = 80;
			}
			else if (protocol == "https")
			{
				port = 443;
				isSecure = true;
			}
			else if (protocol == "mtasa")
			{
				port = 22004;
			}
			else if (protocol == "samp")
			{
				port = 7790;
			}
		}
		else
		{
			dcdebug("p");
			port = static_cast<uint16_t>(Util::toInt(url.substr(portStart, authorityEnd - portStart)));
		}
	}
	
	dcdebug("\n");
	path = url.substr(fileStart, fileEnd - fileStart);
	query = url.substr(queryStart, queryEnd - queryStart);
	fragment = url.substr(fragmentStart, fragmentEnd - fragmentStart);  //http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2606
	if (!Text::isAscii(host))
	{
		static const BOOL l_is_success = IDNA_init(0);
		if (l_is_success)
		{
			const string l_host_acp = Text::utf8ToAcp(host);
			// http://www.rfc-editor.org/rfc/rfc3490.txt
			LocalArray<char, MAX_HOST_LEN> buff;
			size_t size = MAX_HOST_LEN;
			memzero(buff.data(), buff.size());
			memcpy(buff.data(), l_host_acp.c_str(), std::min(buff.size(), l_host_acp.size()));
			if (IDNA_convert_to_ACE(buff.data(), &size))
			{
				host = buff.data();
			}
		}
	}
}
bool Util::isValidSearch(const string& p_search)
{
	auto l_marker_file = p_search.find(' ', 8);
	if (l_marker_file != string::npos && p_search.size() > 12)
	{
		const bool l_is_passive = p_search.compare(8, 4, "Hub:", 4) == 0;
		if (!l_is_passive)
		{
			const auto l_is_ddos = p_search.substr(8, l_marker_file);
			unsigned short l_count_slash = 0;
			unsigned short l_count_colon = 0;
			for (unsigned i = 0; i < l_is_ddos.size(); ++i)
			{
				if (l_is_ddos[i] == '/')
					l_count_slash++;
				else if (l_is_ddos[i] == ':')
					l_count_colon++;
				if (l_count_colon == 2 && l_count_slash == 2)
				{
					return false;
				}
			}
		}
	}
	return true;
}
	
void Util::parseIpPort(const string& aIpPort, string& ip, uint16_t& port)
{
	string::size_type i = aIpPort.rfind(':');
	if (i == string::npos)
	{
		ip   = aIpPort;
	}
	else
	{
		ip   = aIpPort.substr(0, i);
		port = Util::toInt(aIpPort.substr(i + 1));
	}
}
	
std::map<string, string> Util::decodeQuery(const string& query)
{
	std::map<string, string> ret;
	size_t start = 0;
	while (start < query.size())
	{
		auto eq = query.find('=', start);
		if (eq == string::npos)
		{
			break;
		}
	
		auto param = eq + 1;
		auto end = query.find('&', param);
	
		if (end == string::npos)
		{
			end = query.size();
		}
	
		if (eq > start && end > param)
		{
			ret[query.substr(start, eq - start)] = query.substr(param, end - param);
		}
	
		start = end + 1;
	}
	
	return ret;
}
	
	
void Util::setAway(bool aAway, bool notUpdateInfo /*= false*/)
{
	g_away = aAway;
	
	SET_SETTING(AWAY, aAway);
	
	if (g_away)
		g_awayTime = time(NULL);
	
	if (!notUpdateInfo)
	{
		ClientManager::infoUpdated();
	}
}
string Util::getAwayMessage(StringMap& params)
{
	time_t currentTime;
	time(&currentTime);
	int currentHour = localtime(&currentTime)->tm_hour;
	
	params["idleTI"] = formatSeconds(time(NULL) - g_awayTime);
	
	if (BOOLSETTING(AWAY_TIME_THROTTLE) && ((SETTING(AWAY_START) < SETTING(AWAY_END) &&
	                                         currentHour >= SETTING(AWAY_START) && currentHour < SETTING(AWAY_END)) ||
	                                        (SETTING(AWAY_START) > SETTING(AWAY_END) &&
	                                         (currentHour >= SETTING(AWAY_START) || currentHour < SETTING(AWAY_END)))))
	{
		return formatParams((g_awayMsg.empty() ? SETTING(SECONDARY_AWAY_MESSAGE) : g_awayMsg), params, false, g_awayTime);
	}
	else
	{
		return formatParams((g_awayMsg.empty() ? SETTING(DEFAULT_AWAY_MESSAGE) : g_awayMsg), params, false, g_awayTime);
	}
}
	
wstring Util::formatSecondsW(int64_t aSec, bool supressHours /*= false*/)
{
	wchar_t buf[64];
	if (!supressHours)
		_snwprintf(buf, _countof(buf), L"%01lu:%02u:%02u", unsigned long(aSec / (60 * 60)), unsigned((aSec / 60) % 60), unsigned(aSec % 60));
	else
		_snwprintf(buf, _countof(buf), L"%02u:%02u", unsigned(aSec / 60), unsigned(aSec % 60));
	return buf;
}
	
string Util::formatSeconds(int64_t aSec, bool supressHours /*= false*/)
{
	char buf[64];
	if (!supressHours)
		_snprintf(buf, _countof(buf), "%01lu:%02u:%02u", unsigned long(aSec / (60 * 60)), unsigned((aSec / 60) % 60), unsigned(aSec % 60));
	else
		_snprintf(buf, _countof(buf), "%02u:%02u", unsigned(aSec / 60), unsigned(aSec % 60));
	return buf;
}
	
string Util::formatBytes(int64_t aBytes)
{
	char buf[64];
	buf[0] = 0;
	if (aBytes < 1024)
	{
		_snprintf(buf, _countof(buf), "%d %s", (int)aBytes & 0xffffffff, CSTRING(B));
	}
	else if (aBytes < 1048576)
	{
		_snprintf(buf, _countof(buf), "%.02f %s", (double)aBytes / (1024.0), CSTRING(KB));
	}
	else if (aBytes < 1073741824)
	{
		_snprintf(buf, _countof(buf), "%.02f %s", (double)aBytes / (1048576.0), CSTRING(MB));
	}
	else if (aBytes < (int64_t)1099511627776)
	{
		_snprintf(buf, _countof(buf), "%.02f %s", (double)aBytes / (1073741824.0), CSTRING(GB));
	}
	else if (aBytes < (int64_t)1125899906842624)
	{
		_snprintf(buf, _countof(buf), "%.03f %s", (double)aBytes / (1099511627776.0), CSTRING(TB));
	}
	else if (aBytes < (int64_t)1152921504606846976)
	{
		_snprintf(buf, _countof(buf), "%.03f %s", (double)aBytes / (1125899906842624.0), CSTRING(PB));
	}
	else
	{
		_snprintf(buf, _countof(buf), "%.03f %s", (double)aBytes / (1152921504606846976.0), CSTRING(EB));
	}
	return buf;
}
string Util::formatBytes(double aBytes)
{
	char buf[64];
	buf[0] = 0;
	if (aBytes < 1024)
	{
		_snprintf(buf, _countof(buf), "%d %s", (int)aBytes & 0xffffffff, CSTRING(B));
	}
	else if (aBytes < 1048576)
	{
		_snprintf(buf, _countof(buf), "%.02f %s", aBytes / (1024.0), CSTRING(KB));
	}
	else if (aBytes < 1073741824)
	{
		_snprintf(buf, _countof(buf), "%.02f %s", aBytes / (1048576.0), CSTRING(MB));
	}
	else if (aBytes < (int64_t)1099511627776)
	{
		_snprintf(buf, _countof(buf), "%.02f %s", aBytes / (1073741824.0), CSTRING(GB));
	}
	else if (aBytes < (int64_t)1125899906842624)
	{
		_snprintf(buf, _countof(buf), "%.03f %s", aBytes / (1099511627776.0), CSTRING(TB));
	}
	else if (aBytes < (int64_t)1152921504606846976)
	{
		_snprintf(buf, _countof(buf), "%.03f %s", aBytes / (1125899906842624.0), CSTRING(PB));
	}
	else
	{
		_snprintf(buf, _countof(buf), "%.03f %s", aBytes / (1152921504606846976.0), CSTRING(EB));
	}
	return buf;
}
	
wstring Util::formatBytesW(int64_t aBytes)
{
	wchar_t buf[128];
	if (aBytes < 1024)
	{
		_snwprintf(buf, _countof(buf), L"%d %s", (int)(aBytes & 0xffffffff), CWSTRING(B));
	}
	else if (aBytes < 1048576)
	{
		_snwprintf(buf, _countof(buf), L"%.02f %s", (double)aBytes / (1024.0), CWSTRING(KB));
	}
	else if (aBytes < 1073741824)
	{
		_snwprintf(buf, _countof(buf), L"%.02f %s", (double)aBytes / (1048576.0), CWSTRING(MB));
	}
	else if (aBytes < (int64_t)1099511627776)
	{
		_snwprintf(buf, _countof(buf), L"%.02f %s", (double)aBytes / (1073741824.0), CWSTRING(GB));
	}
	else if (aBytes < (int64_t)1125899906842624)
	{
		_snwprintf(buf, _countof(buf), L"%.03f %s", (double)aBytes / (1099511627776.0), CWSTRING(TB));
	}
	else if (aBytes < (int64_t)1152921504606846976)
	{
		_snwprintf(buf, _countof(buf), L"%.03f %s", (double)aBytes / (1125899906842624.0), CWSTRING(PB));
	}
	else
	{
		_snwprintf(buf, _countof(buf), L"%.03f %s", (double)aBytes / (1152921504606846976.0), CWSTRING(EB)); //TODO Crash beta-16
	}
	
	return buf; // https://drdump.com/Problem.aspx?ProblemID=240683
}
	
wstring Util::formatExactSize(int64_t aBytes)
{
#ifdef _WIN32
	wchar_t l_number[64];
	l_number[0] = 0;
	_snwprintf(l_number, _countof(l_number), _T(I64_FMT), aBytes);
	wchar_t l_buf_nf[64];
	l_buf_nf[0] = 0;
	GetNumberFormat(LOCALE_USER_DEFAULT, 0, l_number, &g_nf, l_buf_nf, _countof(l_buf_nf));
	_snwprintf(l_buf_nf, _countof(l_buf_nf), _T("%s %s"), l_buf_nf, CWSTRING(B));
	return l_buf_nf;
#else
	wchar_t buf[64];
	_snwprintf(buf, _countof(buf), _T(I64_FMT), (long long int)aBytes);
	return tstring(buf) + TSTRING(B);
#endif
}
static string findBindIP(const string& tmp, const string& p_gateway_mask, const bool p_check_bind_address, sockaddr_in& dest, const hostent* he)
{
	for (int i = 1; he->h_addr_list[i]; ++i)
	{
		memcpy(&dest.sin_addr, he->h_addr_list[i], he->h_length);
		const string tmp2 = inet_ntoa(dest.sin_addr);
		if (p_check_bind_address && tmp2 == SETTING(BIND_ADDRESS))
			return tmp2;
		if (tmp2.find(p_gateway_mask) != string::npos)
		{
			if (Util::isPrivateIp(tmp2)) // Проблема с Hamachi
			{
				return tmp2;
			}
		}
		if (tmp2 == "192.168.56.1") // Virtual Box ?
		{
			continue;
		}
		// Проблема с Hamachi - часть 2
		//else if (isNotPrivateIpAndNot169(tmp2))
		//{
		//  tmp = tmp2;
		//}
		/*
		 Выявилась проблема с UPnP, при установленной программе Hamachi.
		У пользователя win7. Хамачи сделал свой сетевой интферфейс с ip адресом 25.41.14.130
	
		UPnP пытается сделать перенаправление именно на этот адрес
		если во флайлинке задать "сетевой интерфейс для всех соединений" 192.168.0.103 (адрес который дает ему роутер), то это не помогает :(
	
		К письму прикладываю вывод UPnP правил из роутера, для того что-бы ты понял о чем речь.
		*/
	}
	return tmp;
}
string Util::getLocalOrBindIp(const bool p_check_bind_address)
{
	string tmp;
	char buf[256];
	if (!gethostname(buf, 255)) // двойной вызов
	{
		boost::logic::tribool l_is_wifi_router;
		string l_gateway_ip = Socket::getDefaultGateWay(l_is_wifi_router);
		const auto l_dot = l_gateway_ip.rfind('.');
		if (l_dot != string::npos)
		{
			l_gateway_ip = l_gateway_ip.substr(0, l_dot + 1);
		}
		else
		{
			l_gateway_ip.clear();
		}
		const hostent* he = gethostbyname(buf);
		if (he == nullptr || he->h_addr_list[0] == 0)
			return Util::emptyString;
		sockaddr_in dest  = { { 0 } };
		// We take the first ip as default, but if we can find a better one, use it instead...
		memcpy(&dest.sin_addr, he->h_addr_list[0], he->h_length);
		tmp = inet_ntoa(dest.sin_addr);
		if (p_check_bind_address && tmp == SETTING(BIND_ADDRESS))
		{
			return tmp;
		}
		if (Util::isPrivateIp(tmp) || strncmp(tmp.c_str(), "169", 3) == 0)
		{
			const auto l_bind_address = findBindIP(tmp, l_gateway_ip, p_check_bind_address, dest, he);
			if (!l_bind_address.empty())
			{
				return l_bind_address;
			}
		}
	}
	return tmp;
}
	
bool Util::isPrivateIp(const string& ip)
{
	dcassert(!ip.empty());
	struct in_addr addr = {0};
	addr.s_addr = inet_addr(ip.c_str());
	if (addr.s_addr != INADDR_NONE)
	{
		const uint32_t haddr = ntohl(addr.s_addr);
		return isPrivateIp(haddr);
	}
	return false;
}
	
	
static wchar_t utf8ToLC(const uint8_t* & str)
{
	wchar_t c = 0;
	if (str[0] & 0x80)
	{
		if (str[0] & 0x40)
		{
			if (str[0] & 0x20)
			{
				if (str[1] == 0 || str[2] == 0 ||
				        !((((unsigned char)str[1]) & ~0x3f) == 0x80) ||
				        !((((unsigned char)str[2]) & ~0x3f) == 0x80))
				{
					str++;
					return 0;
				}
				c = ((wchar_t)(unsigned char)str[0] & 0xf) << 12 |
				    ((wchar_t)(unsigned char)str[1] & 0x3f) << 6 |
				    ((wchar_t)(unsigned char)str[2] & 0x3f);
				str += 3;
			}
			else
			{
				if (str[1] == 0 ||
				        !((((unsigned char)str[1]) & ~0x3f) == 0x80))
				{
					str++;
					return 0;
				}
				c = ((wchar_t)(unsigned char)str[0] & 0x1f) << 6 |
				    ((wchar_t)(unsigned char)str[1] & 0x3f);
				str += 2;
			}
		}
		else
		{
			str++;
			return 0;
		}
	}
	else
	{
		c = Text::asciiToLower((char)str[0]);
		str++;
		return c;
	}
	
	return Text::toLower(c);
}
string Util::toString(const char* p_sep, const StringList& p_lst)
{
	string ret;
	for (auto i = p_lst.cbegin(), iend = p_lst.cend(); i != iend; ++i)
	{
		ret += *i;
		if (i + 1 != iend)
			ret += string(p_sep);
	}
	return ret;
}
string Util::toString(char p_sep, const StringList& p_lst)
{
	string ret;
	for (auto i = p_lst.cbegin(), iend = p_lst.cend(); i != iend; ++i)
	{
		ret += *i;
		if (i + 1 != iend)
			ret += p_sep;
	}
	return ret;
}
string Util::toString(char p_sep, const StringSet& p_set)
{
	string ret;
	char l_start_sep = ' ';
	for (auto i = p_set.cbegin(), iend = p_set.cend(); i != iend; ++i)
	{
		ret += l_start_sep;
		ret += *i;
		l_start_sep = p_sep;
	}
	return ret;
}
	
string Util::toString(const StringList& lst)
{
	if (lst.empty())
		return emptyString;
	if (lst.size() == 1)
		return lst[0];
	string tmp("[");
	for (auto i = lst.cbegin(), iend = lst.cend(); i != iend; ++i)
	{
		tmp += *i + ',';
	}
	if (tmp.length() == 1)
		tmp.push_back(']');
	else
		tmp[tmp.length() - 1] = ']';
	return tmp;
}
	
string::size_type Util::findSubString(const string& aString, const string& aSubString, string::size_type start) noexcept
{
	if (aString.length() < start)
		return (string::size_type)string::npos;
	
	if (aString.length() - start < aSubString.length())
		return (string::size_type)string::npos;
	
	if (aSubString.empty())
		return 0;
	
	// Hm, should start measure in characters or in bytes? bytes for now...
	const uint8_t* tx = (const uint8_t*)aString.c_str() + start;
	const uint8_t* px = (const uint8_t*)aSubString.c_str();
	
	const uint8_t* end = tx + aString.length() - start - aSubString.length() + 1;
	
	wchar_t wp = utf8ToLC(px);
	
	while (tx < end)
	{
		const uint8_t* otx = tx;
		if (wp == utf8ToLC(tx))
		{
			const uint8_t* px2 = px;
			const uint8_t* tx2 = tx;
	
			for (;;)
			{
				if (*px2 == 0)
					return otx - (uint8_t*)aString.c_str();
	
				if (utf8ToLC(px2) != utf8ToLC(tx2))
					break;
			}
		}
	}
	return (string::size_type)string::npos;
}
	
wstring::size_type Util::findSubString(const wstring& aString, const wstring& aSubString, wstring::size_type pos) noexcept
{
	if (aString.length() < pos)
		return static_cast<wstring::size_type>(wstring::npos);
	
	if (aString.length() - pos < aSubString.length())
		return static_cast<wstring::size_type>(wstring::npos);
	
	if (aSubString.empty())
		return 0;
	
	wstring::size_type j = 0;
	wstring::size_type end = aString.length() - aSubString.length() + 1;
	
	for (; pos < end; ++pos)
	{
		if (Text::toLower(aString[pos]) == Text::toLower(aSubString[j]))
		{
			wstring::size_type tmp = pos + 1;
			bool found = true;
			for (++j; j < aSubString.length(); ++j, ++tmp)
			{
				if (Text::toLower(aString[tmp]) != Text::toLower(aSubString[j]))
				{
					j = 0;
					found = false;
					break;
				}
			}
	
			if (found)
				return pos;
		}
	}
	return static_cast<wstring::size_type>(wstring::npos);
}
	
string Util::encodeURI(const string& aString, bool reverse)
{
	// reference: rfc2396
	string tmp = aString;
	if (reverse)
	{
		// TODO idna: convert host name from punycode
		string::size_type idx;
		for (idx = 0; idx < tmp.length(); ++idx)
		{
			if (tmp.length() > idx + 2 && tmp[idx] == '%' && isxdigit(tmp[idx + 1]) && isxdigit(tmp[idx + 2]))
			{
				tmp[idx] = fromHexEscape(tmp.substr(idx + 1, 2));
				tmp.erase(idx + 1, 2);
			}
			else   // reference: rfc1630, magnet-uri draft
			{
				if (tmp[idx] == '+')
					tmp[idx] = ' ';
			}
		}
	}
	else
	{
		static const string disallowed = ";/?:@&=+$," // reserved
		                                 "<>#%\" "    // delimiters
		                                 "{}|\\^[]`"; // unwise
		string::size_type idx;
		for (idx = 0; idx < tmp.length(); ++idx)
		{
			if (tmp[idx] == ' ')
			{
				tmp[idx] = '+';
			}
			else
			{
				if (tmp[idx] <= 0x1F || tmp[idx] >= 0x7f || (disallowed.find_first_of(tmp[idx])) != string::npos)
				{
					tmp.replace(idx, 1, toHexEscape(tmp[idx]));
					idx += 2;
				}
			}
		}
	}
	return tmp;
}
	
/**
 * This function takes a string and a set of parameters and transforms them according to
 * a simple formatting rule, similar to strftime. In the message, every parameter should be
 * represented by %[name]. It will then be replaced by the corresponding item in
 * the params stringmap. After that, the string is passed through strftime with the current
 * date/time and then finally written to the log file. If the parameter is not present at all,
 * it is removed from the string completely...
 */
string Util::formatParams(const string& msg, const StringMap& params, bool filter, const time_t t)
{
	string result = msg;
	
	string::size_type c = 0;
	static const string g_goodchars = "aAbBcdHIjmMpSUwWxXyYzZ%";
	bool l_find_alcohol = false;
	while ((c = result.find('%', c)) != string::npos)
	{
		l_find_alcohol = true;
		if (c < result.length() - 1)
		{
			if (g_goodchars.find(result[c + 1], 0) == string::npos) // [6] https://www.box.net/shared/68bcb4f96c1b5c39f12d
			{
				result.replace(c, 1, "%%");
				c++;
			}
			c++;
		}
		else
		{
			result.replace(c, 1, "%%");
			break;
		}
	}
	if (l_find_alcohol) // Не пытаемся искать %[ т.к. не нашли %
	{
		result = formatTime(result, t);
		string::size_type i, j, k;
		i = 0;
		while ((j = result.find("%[", i)) != string::npos)
		{
	
			if (result.size() < j + 2)
				break;
	
			if ((k = result.find(']', j + 2)) == string::npos)
			{
				result.replace(j, 2, "");
				break;
			}
	
			string name = result.substr(j + 2, k - j - 2);
			const auto& smi = params.find(name);
			if (smi == params.end())
			{
				result.erase(j, k - j + 1);
				i = j;
			}
			else
			{
				if (smi->second.find_first_of("\\./") != string::npos)
				{
					string tmp = smi->second;
	
					if (filter)
					{
						// Filter chars that produce bad effects on file systems
						c = 0;
						static const char badchars[] = "\\./:*?|<>";
						while ((c = tmp.find_first_of(badchars, c)) != string::npos)
						{
							tmp[c] = '_';
						}
					}
	
					result.replace(j, k - j + 1, tmp);
					i = j + tmp.size();
				}
				else
				{
					result.replace(j, k - j + 1, smi->second);
					i = j + smi->second.size();
				}
			}
		}
	}
	return result;
}
	
string Util::formatRegExp(const string& msg, const StringMap& params)
{
	string result = msg;
	string::size_type i, j, k;
	i = 0;
	while ((j = result.find("%[", i)) != string::npos)
	{
		if ((result.size() < j + 2) || ((k = result.find(']', j + 2)) == string::npos))
		{
			break;
		}
		const string name = result.substr(j + 2, k - j - 2);
		const auto& smi = params.find(name);
		if (smi != params.end())
		{
			result.replace(j, k - j + 1, smi->second);
			i = j + smi->second.size();
		}
		else
		{
			i = k + 1;
		}
	}
	return result;
}
	
uint64_t Util::getDirSize(const string &sFullPath)
{
	uint64_t total = 0;
	
	WIN32_FIND_DATA fData;
	HANDLE hFind = FindFirstFileEx(Text::toT(sFullPath + "\\*").c_str(),
	                               CompatibilityManager::g_find_file_level,
	                               &fData,
	                               FindExSearchNameMatch,
	                               NULL,
	                               CompatibilityManager::g_find_file_flags);
	
	if (hFind != INVALID_HANDLE_VALUE)
	{
		const string l_tmp_path = SETTING(TEMP_DOWNLOAD_DIRECTORY);
		do
		{
			if ((fData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) && !BOOLSETTING(SHARE_HIDDEN))
				continue;
			const string name = Text::fromT(fData.cFileName);
			if (name == Util::m_dot || name == Util::m_dot_dot)
				continue;
			if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				const string newName = sFullPath + PATH_SEPARATOR + name;
				// TODO TEMP_DOWNLOAD_DIRECTORY может содержать шаблон "[targetdrive]" сравнивать с ним не всегда верно
				if (stricmp(newName + PATH_SEPARATOR, l_tmp_path) != 0)
				{
					total += getDirSize(newName);
				}
			}
			else
			{
				total += (uint64_t)fData.nFileSizeLow | ((uint64_t)fData.nFileSizeHigh) << 32;
			}
		}
		while (FindNextFile(hFind, &fData));
		FindClose(hFind);
	}
	return total;
}
	
bool Util::validatePath(const string &sPath)
{
	if (sPath.empty())
		return false;
	
	if ((sPath.substr(1, 2) == ":\\") || (sPath.substr(0, 2) == "\\\\"))
	{
		if (GetFileAttributes(Text::toT(sPath).c_str()) & FILE_ATTRIBUTE_DIRECTORY)
			return true;
	}
	
	return false;
}
	
string Util::getFilenameForRenaming(const string& p_filename)
{
	string outFilename;
	const string ext = getFileExt(p_filename);
	const string fname = getFileName(p_filename);
	int i = 0;
	do
	{
		i++;
		outFilename = p_filename.substr(0, p_filename.length() - fname.length());
		outFilename += fname.substr(0, fname.length() - ext.length());
		outFilename += '(' + Util::toString(i) + ')';
		outFilename += ext;
	}
	while (File::isExist(outFilename));
	
	return outFilename;
}
	
string Util::formatDigitalClock(const string &p_msg, const time_t& p_t, bool p_is_gmt)
{
	/*
#ifdef _DEBUG
	    static int l_count = 0;
	    if (p_t == time(NULL))
	        dcdebug("\n\n\nUtil::formatDigitalClock called with curent time. Count = %d\n\n\n", ++l_count);
#endif
	*/
	tm* l_loc = p_is_gmt ? gmtime(&p_t) : localtime(&p_t);
	if (!l_loc)
	{
		return Util::emptyString;
	}
	const size_t l_bufsize = p_msg.size() + 15;
	string l_buf;
	l_buf.resize(l_bufsize + 1);
	const size_t l_len = strftime(&l_buf[0], l_bufsize, p_msg.c_str(), l_loc);
	if (!l_len)
		return p_msg;
	else
	{
		l_buf.resize(l_len);
		return l_buf;
	}
}
string Util::formatTime(const string &p_msg, const time_t p_t)
{
	/*
#ifdef _DEBUG
	    static int l_count = 0;
	    if (t == time(NULL))
	        dcdebug("\n\n\nUtil::formatTime(1) called with curent time. Count = %d\n\n\n", ++l_count);
#endif
	*/
	if (!p_msg.empty())
	{
		tm* l_loc = localtime(&p_t);
		if (!l_loc)
		{
			return Util::emptyString;
		}
	
		const string l_msgAnsi = Text::fromUtf8(p_msg);
		size_t bufsize = l_msgAnsi.size() + 256;
		string buf;
		buf.resize(bufsize + 1);
		while (true)
		{
			const size_t l_len = strftime(&buf[0], bufsize, l_msgAnsi.c_str(), l_loc);
			if (l_len)
			{
				buf.resize(l_len);
#ifdef _WIN32
				if (!Text::validateUtf8(buf))
#endif
				{
					buf = Text::toUtf8(buf);
				}
				return buf;
			}
	
			if (errno == EINVAL
			        || bufsize > l_msgAnsi.size() + 1024)
				return Util::emptyString;
	
			bufsize += 64;
			buf.resize(bufsize);
		}
	
	}
	return Util::emptyString;
}
	
string Util::formatTime(uint64_t rest, const bool withSecond /*= true*/)
{
#define formatTimeformatInterval(n) _snprintf(buf, _countof(buf), first ? "%I64u " : " %I64u ", n);\
	formatedTime += (string)buf;\
	first = false
	
	/*
#ifdef _DEBUG
	    static int l_count = 0;
	    if (rest == time(NULL))
	        dcdebug("\n\n\nUtil::formatTime(2) called with curent time. Count = %d\n\n\n", ++l_count);
#endif
	*/
	char buf[32];
	buf[0] = 0;
	string formatedTime;
	uint64_t n;
	uint8_t i = 0;
	bool first = true;
	n = rest / (24 * 3600 * 7);
	rest %= (24 * 3600 * 7);
	if (n)
	{
		formatTimeformatInterval(n);
		formatedTime += (n >= 2) ? STRING(DATETIME_WEEKS) : STRING(DATETIME_WEEK);
		i++;
	}
	n = rest / (24 * 3600);
	rest %= (24 * 3600);
	if (n)
	{
		formatTimeformatInterval(n);
		formatedTime += (n >= 2) ? STRING(DATETIME_DAYS) : STRING(DATETIME_DAY);
		i++;
	}
	n = rest / (3600);
	rest %= (3600);
	if (n)
	{
		formatTimeformatInterval(n);
		formatedTime += (n >= 2) ? STRING(DATETIME_HOURS) : STRING(DATETIME_HOUR);
		i++;
	}
	n = rest / (60);
	rest %= (60);
	if (n)
	{
		formatTimeformatInterval(n);
		formatedTime += (n >= 2) ? STRING(DATETIME_MINUTES) : STRING(DATETIME_MINUTE);
		i++;
	}
	if (withSecond && i <= 2)
	{
		if (rest)
		{
			formatTimeformatInterval(rest);
			formatedTime += STRING(DATETIME_SECONDS);
		}
	}
	return formatedTime;
	
#undef formatTimeformatInterval
}
	
/* Below is a high-speed random number generator with much
   better granularity than the CRT one in msvc...(no, I didn't
   write it...see copyright) */
/* Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.
   Any feedback is very welcome. For any question, comments,
   see http://www.math.keio.ac.jp/matumoto/emt.html or email
   matumoto@math.keio.ac.jp */
/* Period parameters */
	
// TODO убрать магические числа!!!
#define N 624
#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */
	
/* Tempering parameters */
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)
	
static std::vector<unsigned long> g_mt(N + 1); /* the array for the state vector  */
static int g_mti = N + 1; /* mti==N+1 means mt[N] is not initialized */
	
/* initializing the array with a NONZERO seed */
static void sgenrand(unsigned long seed)
{
	/* setting initial seeds to mt[N] using         */
	/* the generator Line 25 of Table 1 in          */
	/* [KNUTH 1981, The Art of Computer Programming */
	/*    Vol. 2 (2nd Ed.), pp102]                  */
	g_mt[0] = seed & ULONG_MAX;
	for (g_mti = 1; g_mti < N; g_mti++)
		g_mt[g_mti] = (69069 * g_mt[g_mti - 1]) & ULONG_MAX;
}
	
uint32_t Util::rand()
{
	unsigned long y;
	/* mag01[x] = x * MATRIX_A  for x=0,1 */
	
	if (g_mti >= N)   /* generate N words at one time */
	{
		static unsigned long mag01[2] = {0x0, MATRIX_A};
		int kk;
	
		if (g_mti == N + 1) /* if sgenrand() has not been called, */
			sgenrand(4357); /* a default initial seed is used   */
	
		for (kk = 0; kk < N - M; kk++)
		{
			y = (g_mt[kk] & UPPER_MASK) | (g_mt[kk + 1] & LOWER_MASK);
			g_mt[kk] = g_mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1];
		}
		for (; kk < N - 1; kk++)
		{
			y = (g_mt[kk] & UPPER_MASK) | (g_mt[kk + 1] & LOWER_MASK);
			g_mt[kk] = g_mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1];
		}
		y = (g_mt[N - 1] & UPPER_MASK) | (g_mt[0] & LOWER_MASK);
		g_mt[N - 1] = g_mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1];
	
		g_mti = 0;
	}
	
	y = g_mt[g_mti++];
	y ^= TEMPERING_SHIFT_U(y);
	y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
	y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
	y ^= TEMPERING_SHIFT_L(y);
	
	return y;
}
	
string Util::getRandomNick(size_t iNickLength /*= 20*/)
{
	static const char  samples[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	static const char* samples3[] =
	{
		"Bear",
		"Cool", "Cow",
		"Dolly", "DCman",
		"Eagle", "Earth",
		"Fire",
		"Hawk", "Head", "Hulk",
		"Indy",
		"Jocker",
		"Man", "Moon", "Monkey",
		"Rabbit",
		"Smile", "Sun",
		"Troll",
		"User",
		"Water"
	};
	
	string name = samples3[Util::rand(_countof(samples3) - 1)];
	name += '_';
	
	for (size_t i = Util::rand(3, 7); i; --i)
	{
		name += samples[Util::rand(_countof(samples) - 1)];
	}
	
	if (name.size() > iNickLength)
	{
		name.resize(iNickLength);
	}
	
	return name;
}
//======================================================================================================================================
tstring Util::CustomNetworkIndex::getCountry() const
{
#ifdef FLYLINKDC_USE_GEO_IP
	if (m_country_cache_index > 0)
	{
		const CFlyLocationDesc l_res = CFlylinkDBManager::getInstance()->get_country_from_cache(m_country_cache_index);
		return Text::toT(Util::getCountryShortName(l_res.m_flag_index - 1));
		//return l_res.m_description;
	}
	else
#endif
	{
		return Util::emptyStringT;
	}
}
//======================================================================================================================================
tstring Util::CustomNetworkIndex::getDescription() const
{
	if (m_location_cache_index > 0)
	{
		const CFlyLocationDesc l_res =  CFlylinkDBManager::getInstance()->get_location_from_cache(m_location_cache_index);
		return l_res.m_description;
	}
#ifdef FLYLINKDC_USE_GEO_IP
	else if (m_country_cache_index > 0)
	{
		const CFlyLocationDesc l_res =  CFlylinkDBManager::getInstance()->get_country_from_cache(m_country_cache_index);
		return l_res.m_description;
	}
	else
#endif
	{
		return Util::emptyStringT;
	}
}
//======================================================================================================================================
int32_t Util::CustomNetworkIndex::getFlagIndex() const
{
	if (m_location_cache_index > 0)
	{
		return CFlylinkDBManager::getInstance()->get_location_index_from_cache(m_location_cache_index);
	}
	else
	{
		return 0;
	}
}
//======================================================================================================================================
#ifdef FLYLINKDC_USE_GEO_IP
int16_t Util::CustomNetworkIndex::getCountryIndex() const
{
	if (m_country_cache_index > 0)
	{
		return CFlylinkDBManager::getInstance()->get_country_index_from_cache(m_country_cache_index);
	}
	else
	{
		return 0;
	}
}
#endif
//======================================================================================================================================
Util::CustomNetworkIndex Util::getIpCountry(uint32_t p_ip, bool p_is_use_only_cache)
{
	if (p_ip && p_ip != INADDR_NONE)
	{
		uint16_t l_country_index = 0;
		uint32_t  l_location_index = uint32_t(-1);
		CFlylinkDBManager::getInstance()->get_country_and_location(p_ip, l_country_index, l_location_index, p_is_use_only_cache);
		if (l_location_index > 0)
		{
			const CustomNetworkIndex l_index(l_location_index, l_country_index);
			return l_index;
		}
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		else
		{
			if (g_fly_server_config.isCollectLostLocation() && BOOLSETTING(AUTOUPDATE_CUSTOMLOCATION) && AutoUpdate::getInstance()->isUpdate())
			{
				CFlylinkDBManager::getInstance()->save_lost_location(p_ip);
			}
		}
#endif //FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		if (l_country_index)
		{
			const CustomNetworkIndex l_index(l_location_index, l_country_index);
			return l_index;
		}
	}
	else
	{
		dcdebug(string("Invalid IP on Util::getIpCountry: " + Util::toString(p_ip) + '\n').c_str());
		dcassert(!p_ip);
	}
	static const CustomNetworkIndex g_unknownLocationIndex(0, 0);
	return g_unknownLocationIndex;
}
//======================================================================================================================================
Util::CustomNetworkIndex Util::getIpCountry(const string& p_ip, bool p_is_use_only_cache)
{
	const uint32_t l_ipNum = Socket::convertIP4(p_ip);
	return getIpCountry(l_ipNum, p_is_use_only_cache);
}
//======================================================================================================================================
string Util::toAdcFile(const string& file)
{
	if (file == "files.xml.bz2" || file == "files.xml")
		return file;
	
	string ret;
	ret.reserve(file.length() + 1);
	ret += '/';
	ret += file;
	for (string::size_type i = 0; i < ret.length(); ++i)
	{
		if (ret[i] == '\\')
		{
			ret[i] = '/';
		}
	}
	return ret;
}
string Util::toNmdcFile(const string& file)
{
	if (file.empty())
		return Util::emptyString;
	
	string ret(file.substr(1));
	for (string::size_type i = 0; i < ret.length(); ++i)
	{
		if (ret[i] == '/')
		{
			ret[i] = '\\';
		}
	}
	return ret;
}
	
string Util::getIETFLang()
{
	string l_lang = SETTING(LANGUAGE_FILE);
	boost::replace_last(l_lang, ".xml", "");
	return l_lang;
}
	
string Util::translateError(DWORD aError)
{
#ifdef _WIN32
#ifdef NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS
	switch (aError)
	{
		case WSAEADDRNOTAVAIL   :
			return STRING(SOCKET_ERROR_WSAEADDRNOTAVAIL);
		case WSAENETDOWN        :
			return STRING(SOCKET_ERROR_WSAENETDOWN);
		case WSAENETUNREACH     :
			return STRING(SOCKET_ERROR_WSAENETUNREACH);
		case WSAENETRESET       :
			return STRING(SOCKET_ERROR_WSAENETRESET);
		case WSAECONNABORTED    :
			return STRING(SOCKET_ERROR_WSAECONNABORTED);
		case WSAECONNRESET      :
			return STRING(SOCKET_ERROR_WSAECONNRESET);
		case WSAENOBUFS         :
			return STRING(SOCKET_ERROR_WSAENOBUFS);
		case WSAEISCONN         :
			return STRING(SOCKET_ERROR_WSAEISCONN);
		case WSAETIMEDOUT       :
			return STRING(SOCKET_ERROR_WSAETIMEDOUT);
		case WSAECONNREFUSED    :
			return STRING(SOCKET_ERROR_WSAECONNREFUSED);
		case WSAELOOP           :
			return STRING(SOCKET_ERROR_WSAELOOP);
		case WSAENAMETOOLONG    :
			return STRING(SOCKET_ERROR_WSAENAMETOOLONG);
		case WSAEHOSTDOWN       :
			return STRING(SOCKET_ERROR_WSAEHOSTDOWN);
		default:
#endif //NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS
			DWORD l_formatMessageFlag =
			    FORMAT_MESSAGE_ALLOCATE_BUFFER |
			    FORMAT_MESSAGE_FROM_SYSTEM |
			    FORMAT_MESSAGE_IGNORE_INSERTS;
	
			LPCVOID lpSource = nullptr;
			// Обработаем расширенные ошибки по инету
			// http://stackoverflow.com/questions/20435591/internetgetlastresponseinfo-returns-strange-characters-instead-of-error-message
			{
				wstring l_error;
				DWORD dwLen = 0;
				DWORD dwErr = aError;
				if (dwErr == ERROR_INTERNET_EXTENDED_ERROR)
				{
					InternetGetLastResponseInfo(&dwErr, NULL, &dwLen); //
					if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER && dwLen)
					{
						dwLen++;
						dcassert(dwLen);
						if (dwLen)
						{
							l_error.resize(dwLen);
							InternetGetLastResponseInfo(&dwErr, &l_error[0], &dwLen);
						}
					}
					if (dwLen)
					{
						return "Internet Error = " + Text::fromT(l_error) + " [Code = " + Util::toString(dwErr) + "]";
					}
				}
			}
			// http://stackoverflow.com/questions/2159458/why-is-formatmessage-failing-to-find-a-message-for-wininet-errors/2159488#2159488
			if (aError >= INTERNET_ERROR_BASE && aError < INTERNET_ERROR_LAST)
			{
				l_formatMessageFlag |= FORMAT_MESSAGE_FROM_HMODULE;
				lpSource = GetModuleHandle(_T("wininet.dll"));
			}
			/*
			else if (aError >= XXX && aError < YYY)
			{
			TODO: Load text for errors from other libraries?
			}
			*/
	
			LPTSTR lpMsgBuf = 0;
			DWORD chars = FormatMessage(
			                  l_formatMessageFlag,
			                  lpSource,
			                  aError,
#if defined (_CONSOLE) || defined (_DEBUG)
			                  MAKELANGID(LANG_NEUTRAL, SUBLANG_ENGLISH_US), // US
#else
			                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
#endif
			                  (LPTSTR) &lpMsgBuf,
			                  0,
			                  NULL
			              );
			string tmp;
			if (chars != 0)
			{
				tmp = Text::fromT(lpMsgBuf);
				// Free the buffer.
				LocalFree(lpMsgBuf);
				string::size_type i = 0;
	
				while ((i = tmp.find_first_of("\r\n", i)) != string::npos)
				{
					tmp.erase(i, 1);
				}
			}
			tmp += "[error: " + toString(aError) + "]";
#if 0 // TODO
			if (aError >= WSAEADDRNOTAVAIL && aError <= WSAEHOSTDOWN)
			{
				tmp += "\r\n\t" + STRING(SOCKET_ERROR_NOTE) + " " + Util::getWikiLink() + "socketerror#error_" + toString(aError);  // as  LANG:socketerror#error_10060
			}
#endif
			return tmp;
#else // _WIN32
	return Text::toUtf8(strerror(aError));
#endif // _WIN32
#ifdef NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS
	}
#endif //NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS
}
	
TCHAR* Util::strstr(const TCHAR *str1, const TCHAR *str2, int *pnIdxFound)
{
	TCHAR *s1, *s2;
	TCHAR *cp = const_cast<TCHAR*>(str1);
	if (!*str2)
		return const_cast<TCHAR*>(str1);
	int nIdx = 0;
	while (*cp)
	{
		s1 = cp;
		s2 = (TCHAR *) str2;
		while (*s1 && *s2 && !(*s1 - *s2))
			s1++, s2++;
		if (!*s2)
		{
			if (pnIdxFound != NULL)
				*pnIdxFound = nIdx;
			return cp;
		}
		cp++;
		nIdx++;
	}
	if (pnIdxFound != NULL)
		*pnIdxFound = -1;
	return nullptr;
}
	
/* natural sorting */
	
int Util::DefaultSort(const wchar_t *a, const wchar_t *b, bool noCase /*=  true*/)
{
	{
		return noCase ? lstrcmpi(a, b) : lstrcmp(a, b);
	}
}
void Util::setLimiter(bool aLimiter)
{
	SET_SETTING(THROTTLE_ENABLE, aLimiter);
	ClientManager::infoUpdated();
}
	
std::string Util::getRegistryCommaSubkey(const tstring& p_key)
{
	std::string l_result;
	std::string l_sep;
	HKEY l_hk = nullptr;
	TCHAR l_buf[512];
	l_buf[0] = 0;
	tstring l_key =  FLYLINKDC_REGISTRY_PATH _T("\\");
	l_key += p_key;
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, l_key.c_str(), 0, KEY_READ, &l_hk) == ERROR_SUCCESS)
	{
		DWORD l_dwIndex = 0;
		DWORD l_len = _countof(l_buf);
		while (RegEnumValue(l_hk, l_dwIndex++, l_buf, &l_len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			l_result += l_sep + Text::fromT(l_buf);
			l_len = _countof(l_buf);
			if (l_sep.empty())
				l_sep = ',';
		}
		::RegCloseKey(l_hk);
	}
	return l_result;
}
	
string Util::getRegistryValueString(const TCHAR* p_key, bool p_is_path)
{
	HKEY hk = nullptr;
	TCHAR l_buf[512];
	l_buf[0] = 0;
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, FLYLINKDC_REGISTRY_PATH, 0, KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD l_bufLen = sizeof(l_buf);
		::RegQueryValueEx(hk, p_key, NULL, NULL, (LPBYTE)l_buf, &l_bufLen);
		::RegCloseKey(hk);
		if (l_bufLen)
		{
			string l_result = Text::fromT(l_buf);
			if (p_is_path)
				AppendPathSeparator(l_result);
			return l_result;
		}
	}
	return emptyString;
}
	
bool Util::deleteRegistryValue(const TCHAR* p_key)
{
	HKEY hk = nullptr;
	if (::RegCreateKeyEx(HKEY_CURRENT_USER, FLYLINKDC_REGISTRY_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL) != ERROR_SUCCESS)
	{
		return false;
	}
	const LSTATUS status = ::RegDeleteValue(hk, p_key);
	::RegCloseKey(hk);
	dcassert(status == ERROR_SUCCESS);
	return status == ERROR_SUCCESS;
}
	
bool Util::setRegistryValueInt(const TCHAR* p_key, DWORD p_value)
{
	HKEY hk = nullptr;
	if (::RegCreateKeyEx(HKEY_CURRENT_USER, FLYLINKDC_REGISTRY_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL) != ERROR_SUCCESS)
	{
		return false;
	}
	const LSTATUS status = ::RegSetValueEx(hk, p_key, NULL, REG_DWORD, reinterpret_cast<const BYTE*>(&p_value), sizeof(DWORD));
	const auto status_close = ::RegCloseKey(hk);
	dcassert(status_close == ERROR_SUCCESS);
	dcassert(status == ERROR_SUCCESS);
	return status == ERROR_SUCCESS;
}
DWORD Util::getRegistryValueInt(const TCHAR* p_key)
{
	DWORD l_value = 0;
	HKEY hk = nullptr;
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, FLYLINKDC_REGISTRY_PATH, 0, KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD dwType = 0;
		ULONG nBytes = sizeof(DWORD);
		LONG lRes = ::RegQueryValueEx(hk, p_key, NULL, &dwType, reinterpret_cast<LPBYTE>(&l_value), &nBytes);
		const auto status_close = ::RegCloseKey(hk);
		dcassert(status_close == ERROR_SUCCESS);
		if (dwType != REG_DWORD)
			return 0;
		if (lRes == ERROR_SUCCESS)
			return l_value;
	}
	return 0;
}
	
bool Util::setRegistryValueString(const TCHAR* p_key, const tstring& p_value)
{
	HKEY hk = nullptr;
	if (::RegCreateKeyEx(HKEY_CURRENT_USER, FLYLINKDC_REGISTRY_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL) != ERROR_SUCCESS)
	{
		return false;
	}
	const LSTATUS status = ::RegSetValueEx(hk, p_key, NULL, REG_SZ, (LPBYTE)p_value.c_str(), sizeof(TCHAR) * (p_value.length() + 1));
	::RegCloseKey(hk);
	dcassert(status == ERROR_SUCCESS);
	return status == ERROR_SUCCESS;
}
	
#ifdef SSA_VIDEO_PREVIEW_FEATURE
bool Util::isStreamingVideoFile(const string& p_file)
{
	const string l_file_ext = Text::toLower(Util::getFileExtWithoutDot(p_file));
	return CFlyServerConfig::isMediainfoExt(l_file_ext);
}
#endif
string Util::getWANIP(const string& p_url, LONG p_timeOut /* = 500 */)
{
	CFlyLog l_log("[GetIP]");
	string l_downBuf;
	getDataFromInet(false, p_url, l_downBuf, p_timeOut);
	if (!l_downBuf.empty())
	{
		SimpleXML xml;
		try
		{
			xml.fromXML(l_downBuf);
			if (xml.findChild("html"))
			{
				xml.stepIn();
				if (xml.findChild("body"))
				{
					const string l_IP = xml.getChildData().substr(20);
					l_log.step("Download : " + p_url + " IP = " + l_IP);
					if (isValidIP(l_IP))
					{
						return l_IP;
					}
					else
					{
						dcassert(0);
					}
				}
			}
		}
		catch (SimpleXMLException & e)
		{
			l_log.step(string("Error parse XML: ") + e.what());
		}
		catch (std::exception& e)
		{
			l_log.step(string("std::exception: ") + e.what()); // fix https://drdump.com/Problem.aspx?ProblemID=300455
		}
	}
	else
		l_log.step("Error download : " + Util::translateError());
	return Util::emptyString;
}
	
size_t Util::getDataFromInetSafe(bool p_is_use_cache, const string& p_url, string& p_data, LONG p_time_out /* = 0 */, IDateReceiveReporter* p_reporter /*= NULL */)
{
	std::vector<byte> l_bin_data;
	CFlyHTTPDownloader l_http_downloader;
	l_http_downloader.m_is_use_cache = p_is_use_cache;
	if (p_is_use_cache)
	{
		l_http_downloader.setInetFlag(INTERNET_FLAG_CACHE_IF_NET_FAIL);
	}
	const size_t l_bin_size = l_http_downloader.getBinaryDataFromInetSafe(p_url, l_bin_data, p_time_out, p_reporter);
	if (l_bin_size)
	{
		p_data = string((char*)l_bin_data.data(), l_bin_size);
	}
	else
	{
		p_data.clear();
	}
	return l_bin_size;
}
size_t Util::getDataFromInet(bool p_is_use_cache, const string& p_url, string& p_data, LONG p_time_out /*=0*/, IDateReceiveReporter* p_reporter /* = NULL */)
{
	std::vector<byte> l_bin_data;
	CFlyHTTPDownloader l_http_downloader;
	l_http_downloader.m_is_use_cache = p_is_use_cache;
	const size_t l_bin_size = l_http_downloader.getBinaryDataFromInet(p_url, l_bin_data, p_time_out, p_reporter);
	if (l_bin_size)
	{
		p_data = string((char*)l_bin_data.data(), l_bin_size);
	}
	else
	{
		p_data.clear();
	}
	return l_bin_size;
}
#if 0
string Util::getExtInternetError()
{
	wstring l_error;
	DWORD dwLen = 0;
	DWORD dwErr = GetLastError();
	if (dwErr == ERROR_INTERNET_EXTENDED_ERROR)
	{
		InternetGetLastResponseInfo(&dwErr, NULL, &dwLen);
		if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER && dwLen)
		{
			dwLen++;
			dcassert(dwLen);
			if (dwLen)
			{
				l_error.resize(dwLen);
				InternetGetLastResponseInfo(&dwErr, &l_error[0], &dwLen);
			}
		}
		if (dwLen)
			return "Internet Error = " + Text::fromT(l_error) + " [Code = " + Util::toString(dwErr) + "]";
	}
	return translateError(dwErr);
}
#endif
void CFlyHTTPDownloader::create_error_message(const char* p_type, const string& p_url)
{
	m_last_error_code = GetLastError();
	m_error_message = p_type;
	if (m_is_add_url)
		m_error_message += " [ " + p_url + "] ";
	m_error_message += " error = " + Util::translateError(m_last_error_code);
}
bool CFlyHTTPDownloader::switchMirrorURL(string& p_url, int p_mirror)
{
	if (p_mirror == 2 || p_mirror == 3)
	{
		if (p_url.find("://etc.fly-server.ru/etc/") != string::npos ||
		        p_url.find("://update.fly-server.ru/update/") != string::npos
		   )
		{
			string l_url = p_url;
			const char* l_base_url = ".fly-server.ru/";
			boost::replace_all(l_url, l_base_url, Util::toString(p_mirror) + l_base_url);
			LogManager::message("Use mirror: " + l_url);
			p_url = l_url;
			return true;
		}
	}
	return false;
}
	
int CFlyHTTPDownloader::g_last_stable_mirror = 0;
void CFlyHTTPDownloader::nextMirror()
{
	if (g_last_stable_mirror == 0)
		g_last_stable_mirror = 1;
	g_last_stable_mirror++;
	if (g_last_stable_mirror == 4)
		g_last_stable_mirror = 0;
}
	
uint64_t CFlyHTTPDownloader::getBinaryDataFromInetSafe(const string& p_url, std::vector<unsigned char>& p_data_out,
                                                       LONG p_time_out /*=0*/, IDateReceiveReporter* p_reporter /* = NULL */)
{
	size_t l_length = 0;
	string l_url = p_url;
	if (g_last_stable_mirror != 0 && g_last_stable_mirror != 2 && g_last_stable_mirror != 3)
	{
		g_last_stable_mirror = 0;
	}
	switchMirrorURL(l_url, g_last_stable_mirror);
	for (int i = 2; i < 5; ++i)
	{
#ifdef _DEBUG
		// boost::replace_all(l_url, "etc2.", "etc.");
#endif
		l_length = getBinaryDataFromInet(l_url, p_data_out, p_time_out, p_reporter);
		if (l_length > 0)
		{
			if (i >= 4) // Если на зеркале скачали - остаемся на нем
			{
				g_last_stable_mirror = i - 1; // Продолжим пытаться качать с зеркала для этой сессии.
			}
			break;
		}
		else
		{
			if (l_length == 0 && getLastErrorCode() == 12007)
			{
				break; // https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1650
			}
			if (g_last_stable_mirror != 0)
			{
				g_last_stable_mirror = 0; // При ошибке на зеркале скидываемся обратно на главный хост
				i = 2;
				const char* l_base_url = ".fly-server.ru/";
				boost::replace_all(l_url, "2.fly-server.ru/", l_base_url);
				boost::replace_all(l_url, "3.fly-server.ru/", l_base_url);
				continue;
			}
			if (switchMirrorURL(l_url, i))
			{
				continue;
			}
			break;
		}
	}
	return l_length;
}
	
static void useDebugProxy(HINTERNET hInternet)
{
#ifdef _DEBUG
	static bool g_is_first = false;
	if (!g_is_first)
	{
		//g_is_first = true;
		const LPWSTR proxyName = _T("http://localhost:1111");
	
		INTERNET_PER_CONN_OPTION_LIST OptionList;
		INTERNET_PER_CONN_OPTION Option[3];
		const unsigned long listSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
		Option[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
		Option[1].dwOption = INTERNET_PER_CONN_FLAGS;
		Option[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
		OptionList.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
		OptionList.pszConnection = NULL;
		OptionList.dwOptionCount = 3;
		OptionList.dwOptionError = 0;
	
		Option[0].Value.pszValue = (LPWSTR)proxyName;
		Option[1].Value.dwValue = PROXY_TYPE_PROXY;
		Option[2].Value.pszValue = (LPWSTR)L"";
		OptionList.pOptions = Option;
	
		if (!InternetSetOption(hInternet, INTERNET_OPTION_PER_CONNECTION_OPTION, &OptionList, listSize)) {
			dcassert(0);
		}
		if (!InternetSetOption(hInternet, INTERNET_OPTION_REFRESH, NULL, NULL))
		{
			dcassert(0);
		}
	}
#endif
	
}
uint64_t CFlyHTTPDownloader::getBinaryDataFromInetArray(CFlyUrlItemArray& p_url_array, LONG p_time_out /*= 0*/, IDateReceiveReporter* p_reporter /*= NULL*/)
{
	uint64_t sumBytesRead = 0;
	m_last_error_code = 0;
	const DWORD frameBufferSize = 4096;
	dcassert(frameBufferSize);
	dcassert(!p_url_array.empty());
	if (p_url_array.empty())
		return 0;
	CInternetHandle hInternet(InternetOpen(m_user_agent.empty() ? g_full_user_agent.c_str() : m_user_agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
	if (!hInternet)
	{
		create_error_message("InternetOpen", p_url_array.begin()->m_url);
		LogManager::message(m_error_message);
		dcassert(0);
		return 0;
	}
	//useDebugProxy(hInternet);
	if (p_time_out)
	{
		InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &p_time_out, sizeof(p_time_out));
		InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &p_time_out, sizeof(p_time_out));
		InternetSetOption(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &p_time_out, sizeof(p_time_out));
	}
	bool isUserCancel = false;
	for (auto& u : p_url_array)
	{
		if (isUserCancel)
			break;
		// https://github.com/ak48disk/simulationcraft/blob/392937fde95bdc4f13ccd3681e2fa61813856bb6/engine/interfaces/sc_http.cpp
		// http://msdn.microsoft.com/en-us/library/ms906346.aspx
		// Проверить: конфиг файл действительно меняется?
		// INTERNET_FLAG_NO_CACHE_WRITE - использовать если файл большой
		// INTERNET_FLAG_RESYNCHRONIZE - использовать для xml  + конфиг
		DWORD l_cache_flag = INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES; // | INTERNET_FLAG_CACHE_IF_NET_FAIL;
#define FLYLINKDC_USE_CACHE_WININET
#ifdef FLYLINKDC_USE_CACHE_WININET
		if (!m_is_use_cache)
		{
			//l_cache_flag |= INTERNET_FLAG_PRAGMA_NOCACHE;
			if (u.m_url.size() > 6)
			{
				const auto l_ext3 = u.m_url.c_str() + u.m_url.size() - 4;
				const auto l_ext4 = u.m_url.c_str() + u.m_url.size() - 5;
				if (strcmp(l_ext3, ".xml") == 0 || // TODO - Унести в конфиг
				        strcmp(l_ext3, ".bz2") == 0 ||
				        strcmp(l_ext4, ".sign") == 0)
				{
					l_cache_flag |= INTERNET_FLAG_RELOAD; // INTERNET_FLAG_RESYNCHRONIZE;
				}
			}
		}
#endif
		CInternetHandle hURL(InternetOpenUrlA(hInternet, u.m_url.c_str(), NULL, 0, m_inet_flag | l_cache_flag, 0));
		if (!hURL)
		{
			//dcassert(0);
			create_error_message("InternetOpenUrlA", u.m_url);
			LogManager::message(m_error_message);
			// TODO - залогировать коды ошибок для статы
			return 0;
		}
		uint64_t totalBytesRead = 0;
		for (;;)
		{
			DWORD l_BytesRead = 0;
			u.p_body.resize(totalBytesRead + frameBufferSize);
			if (!InternetReadFile(hURL, &u.p_body[totalBytesRead], frameBufferSize, &l_BytesRead))
			{
				dcassert(0);
				create_error_message("InternetReadFile", u.m_url);
				LogManager::message(m_error_message);
				//// TODO - залогировать коды ошибок для статы
				return 0;
			}
			if (l_BytesRead == 0)
			{
				break;
			}
			else
			{
				totalBytesRead += l_BytesRead;
				if (p_reporter)
				{
					if (!p_reporter->ReportResultReceive(l_BytesRead))
					{
						isUserCancel = true;
						m_error_message = "isUserCancel!";
						break;
					}
				}
			}
		}
		for (auto i = m_get_http_header_item.begin(); i != m_get_http_header_item.end(); ++i)
		{
			dcassert(i->size());
#if 0
			if (*i == "Content-Length")
			{
				char file_size_buf[20];
				file_size_buf[0] = 0;
				DWORD file_size_buf_len = sizeof(file_size_buf);
				DWORD l_zero = 0;
				if (!HttpQueryInfo(hURL, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, file_size_buf, &file_size_buf_len, &l_zero))
				{
					*i = file_size_buf;
				}
			}
			else
#endif
			{
				vector<char> l_buf(i->size() + 1);
				strcpy(&l_buf[0], i->c_str());
				DWORD dwBufSize = l_buf.size();
				if (!HttpQueryInfoA(hURL, HTTP_QUERY_CUSTOM, &l_buf[0], &dwBufSize, NULL))
				{
					i->clear();
				}
				else
				{
					*i = &l_buf[0];
				}
			}
		}
		if (isUserCancel)
		{
			u.p_body.clear();
			totalBytesRead = 0;
		}
		u.p_body.resize(totalBytesRead);
		sumBytesRead += totalBytesRead;
	}
	return sumBytesRead;
	
}
	
uint64_t CFlyHTTPDownloader::getBinaryDataFromInet(const string& p_url, std::vector<unsigned char>& p_data_out, LONG p_time_out /*=0*/, IDateReceiveReporter* p_reporter /* = NULL */)
{
	m_last_error_code = 0;
	const DWORD frameBufferSize = 4096;
	dcassert(frameBufferSize);
	dcassert(!p_url.empty());
	p_data_out.clear();
	if (p_url.empty())
		return 0;
	
	CInternetHandle hInternet(InternetOpen(m_user_agent.empty() ? g_full_user_agent.c_str() : m_user_agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
	if (!hInternet)
	{
		create_error_message("InternetOpen", p_url);
		LogManager::message(m_error_message);
		dcassert(0);
		return 0;
	}
	//useDebugProxy(hInternet);
	if (p_time_out)
	{
		InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &p_time_out, sizeof(p_time_out));
	}
	// https://github.com/ak48disk/simulationcraft/blob/392937fde95bdc4f13ccd3681e2fa61813856bb6/engine/interfaces/sc_http.cpp
	// http://msdn.microsoft.com/en-us/library/ms906346.aspx
	// Проверить: конфиг файл действительно меняется?
	// INTERNET_FLAG_NO_CACHE_WRITE - использовать если файл большой
	// INTERNET_FLAG_RESYNCHRONIZE - использовать для xml  + конфиг
	DWORD l_cache_flag = INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES; // | INTERNET_FLAG_CACHE_IF_NET_FAIL;
#define FLYLINKDC_USE_CACHE_WININET
#ifdef FLYLINKDC_USE_CACHE_WININET
	if (!m_is_use_cache)
	{
		//l_cache_flag |= INTERNET_FLAG_PRAGMA_NOCACHE;
		if (p_url.size() > 6)
		{
			const auto l_ext3 = p_url.c_str() + p_url.size() - 4;
			const auto l_ext4 = p_url.c_str() + p_url.size() - 5;
			if (strcmp(l_ext3, ".xml") == 0 || // TODO - Унести в конфиг
			        strcmp(l_ext3, ".bz2") == 0 ||
			        strcmp(l_ext4, ".sign") == 0)
			{
				l_cache_flag |= INTERNET_FLAG_RELOAD; // INTERNET_FLAG_RESYNCHRONIZE;
			}
		}
	}
#endif
	CInternetHandle hURL(InternetOpenUrlA(hInternet, p_url.c_str(), NULL, 0, m_inet_flag | l_cache_flag, 0));
	if (!hURL)
	{
		//dcassert(0);
		create_error_message("InternetOpenUrlA", p_url);
		LogManager::message(m_error_message);
		// TODO - залогировать коды ошибок для статы
		return 0;
	}
	bool isUserCancel = false;
	uint64_t totalBytesRead = 0;
	for (;;)
	{
		DWORD l_BytesRead = 0;
		p_data_out.resize(totalBytesRead + frameBufferSize);
		if (!InternetReadFile(hURL, &p_data_out[totalBytesRead], frameBufferSize, &l_BytesRead))
		{
			dcassert(0);
			create_error_message("InternetReadFile", p_url);
			LogManager::message(m_error_message);
			//// TODO - залогировать коды ошибок для статы
			return 0;
		}
		if (l_BytesRead == 0)
		{
			break;
		}
		else
		{
			totalBytesRead += l_BytesRead;
			if (p_reporter)
			{
				if (!p_reporter->ReportResultReceive(l_BytesRead))
				{
					isUserCancel = true;
					m_error_message = "isUserCancel!";
					break;
				}
			}
		}
	}
	for (auto i = m_get_http_header_item.begin(); i != m_get_http_header_item.end(); ++i)
	{
		dcassert(i->size());
#if 0
		if (*i == "Content-Length")
		{
			char file_size_buf[20];
			file_size_buf[0] = 0;
			DWORD file_size_buf_len = sizeof(file_size_buf);
			DWORD l_zero = 0;
			if (!HttpQueryInfo(hURL, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, file_size_buf, &file_size_buf_len, &l_zero))
			{
				*i = file_size_buf;
			}
		}
		else
#endif
		{
			vector<char> l_buf(i->size() + 1);
			strcpy(&l_buf[0], i->c_str());
			DWORD dwBufSize = l_buf.size();
			if (!HttpQueryInfoA(hURL, HTTP_QUERY_CUSTOM, &l_buf[0], &dwBufSize, NULL))
			{
				i->clear();
			}
			else
			{
				*i = &l_buf[0];
			}
		}
	}
	if (isUserCancel)
	{
		p_data_out.clear();
		totalBytesRead = 0;
	}
	p_data_out.resize(totalBytesRead);
	return totalBytesRead;
}
	
bool Util::getTTH_MD5(const string& p_filename, size_t p_buffSize, unique_ptr<TigerTree>* p_tth, unique_ptr<MD5Calc>* p_md5 /* = 0 */, bool p_isAbsPath/* = true*/)
{
	dcassert(p_tth != nullptr);
	if (p_tth == nullptr)
		return false;
	AutoArray<uint8_t> buf(p_buffSize);
	try
	{
		File f(p_filename, File::READ, File::OPEN, p_isAbsPath);
	
		*p_tth = unique_ptr<TigerTree>(new TigerTree(TigerTree::calcBlockSize(f.getSize(), 1)));
		if (p_md5)
		{
			*p_md5 = unique_ptr<MD5Calc>(new MD5Calc());
			p_md5->get()->MD5Init();
			// *****************************************************
			if (f.getSize() > 0)
			{
				size_t n = p_buffSize;
				while ((n = f.read(buf.data(), n)) > 0)
				{
					p_tth->get()->update(buf.data(), n);
					// ****************
					p_md5->get()->MD5Update(buf.data(), n);
					// ****************
					n = p_buffSize;
				}
			}
			else
			{
				p_tth->get()->update("", 0);
			}
			// *****************************************************
		}
		else
		{
			// *****************************************************
			if (f.getSize() > 0)
			{
				size_t n = p_buffSize;
				while ((n = f.read(buf.data(), n)) > 0)
				{
					p_tth->get()->update(buf.data(), n);
					n = p_buffSize;
				}
			}
			else
			{
				p_tth->get()->update("", 0);
			}
			// *****************************************************
		}
		f.close();
		p_tth->get()->finalize();
		return true;
	}
	catch (const FileException&)
	{
		//-V565
		// No File
	}
	return false;
}
	
void Util::BackupSettings()
{
	const string bkpath = formatTime(getConfigPath() + "BackUp\\%Y-%m-%d\\", time(NULL));
	const string& sourcepath = getConfigPath();
	File::ensureDirectory(bkpath);
	for (size_t i = 0; i < _countof(g_configFileLists); ++i)
	{
		if (!File::isExist(bkpath + g_configFileLists[i]) && File::isExist(sourcepath + g_configFileLists[i]))
		{
			try
			{
				File::copyFile(sourcepath + g_configFileLists[i], bkpath + g_configFileLists[i]);
			}
			catch (FileException &)
			{
				LogManager::message("Error File::copyFile = sourcepath + FileList[i] = " + sourcepath + g_configFileLists[i]
				                    + " , bkpath + FileList[i] = " + bkpath + g_configFileLists[i]);
			}
		}
	}
}
	
string Util::formatDchubUrl(const string& DchubUrl)
{
#ifdef _DEBUG
	//static unsigned int l_call_count = 0;
	//dcdebug("Util::formatDchubUrl DchubUrl =\"%s\", call count %d\n", DchubUrl.c_str(), ++l_call_count);
#endif
	uint16_t port;
	string proto, host, file, query, fragment;
	
	decodeUrl(DchubUrl, proto, host, port, file, query, fragment);
	const string l_url = proto + "://" + host + ((port == 411 && proto == "dchub") ? "" : ":" + Util::toString(port));
	dcassert(l_url == Text::toLower(l_url));
	return l_url;
}
	
	
string Util::getMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize)
{
	return "magnet:?xt=urn:tree:tiger:" + aHash.toBase32() + "&xl=" + toString(aSize) + "&dn=" + encodeURI(aFile);
}
	
string Util::getWebMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize)
{
	StringMap params;
	params["magnet"] = getMagnet(aHash, aFile, aSize);
	params["size"] = formatBytes(aSize);
	params["TTH"] = aHash.toBase32();
	params["name"] = aFile;
	return formatParams(SETTING(COPY_WMLINK), params, false);
}
	
string Util::getMagnetByPath(const string& aFile)
{
	string outFilename;
	TTHValue outTTH;
	int64_t outSize = 0;
	if (ShareManager::getInstance()->findByRealPathName(aFile, &outTTH, &outFilename,  &outSize))
		return getMagnet(outTTH, outFilename, outSize);
	
	return emptyString;
}
string Util::getDownloadPath(const string& def)
{
	typedef HRESULT(WINAPI * _SHGetKnownFolderPath)(GUID & rfid, DWORD dwFlags, HANDLE hToken, PWSTR * ppszPath);
	
	// Try Vista downloads path
	static HINSTANCE shell32 = nullptr;
	if (!shell32)
	{
		shell32 = ::LoadLibrary(_T("Shell32.dll"));
		if (shell32)
		{
			_SHGetKnownFolderPath getKnownFolderPath = (_SHGetKnownFolderPath)::GetProcAddress(shell32, "SHGetKnownFolderPath");
	
			if (getKnownFolderPath)
			{
				PWSTR path = nullptr;
				// Defined in KnownFolders.h.
				static GUID downloads = {0x374de290, 0x123f, 0x4565, {0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b}};
				if (getKnownFolderPath(downloads, 0, NULL, &path) == S_OK)
				{
					const string ret = Text::fromT(path) + "\\";
					::CoTaskMemFree(path);
					return ret;
				}
			}
			::FreeLibrary(shell32);
		}
	}
	
	return def + "Downloads\\";
}
	
// From RSSManager.h
string Util::ConvertFromHTMLSymbol(const string &htmlString, const string& findString, const string& changeString)
{
	string strRet;
	if (htmlString.empty())
		return htmlString;
	
	string::size_type findFirst = htmlString.find(findString);
	string::size_type prevPos = 0;
	while (findFirst != string::npos)
	{
		strRet.append(htmlString.substr(prevPos, findFirst - prevPos));
		//strRet.append(changeString);
		strRet.append(Text::acpToUtf8(changeString));
		prevPos = findFirst + findString.length();
		findFirst = htmlString.find(findString, prevPos);
	}
	strRet.append(htmlString.substr(prevPos));
	return strRet;
}
string Util::ConvertFromHTML(const string &htmlString)
{
	// Replace &quot; with \"
	string strRet;
	if (htmlString.empty())
		return htmlString;
	strRet = ConvertFromHTMLSymbol(htmlString, "&nbsp;", " ");
	strRet = ConvertFromHTMLSymbol(strRet, "<br>", " \r\n");
	strRet = ConvertFromHTMLSymbol(strRet, "&quot;", "\"");
	strRet = ConvertFromHTMLSymbol(strRet, "&mdash;", "—");
	strRet = ConvertFromHTMLSymbol(strRet, "&ndash;", "–");
	strRet = ConvertFromHTMLSymbol(strRet, "&hellip;", "…");
	strRet = ConvertFromHTMLSymbol(strRet, "&laquo;", "«");
	strRet = ConvertFromHTMLSymbol(strRet, "&raquo;", "»");
//	strRet = ConvertFromHTMLSymbol(strRet, "&lt;", "<");
//	strRet = ConvertFromHTMLSymbol(strRet, "&gt;", ">");
	return strRet;
}
// From RSSManager.h
	
tstring Util::eraseHtmlTags(tstring && p_desc)
{
	static const std::wregex g_tagsRegex(L"<[^>]+>|<(.*)>");
	p_desc = std::regex_replace(p_desc, g_tagsRegex, (tstring) L" ");
//	p_desc = Text::toT(ConvertFromHTML( Text::fromT(p_desc) ));
	/*
	static const tstring g_htmlTags[] = // Tags: from long to short !
	{
	    _T("<br"), // always first.
	    _T("<strong"), _T("</strong"),
	    _T("<table"), _T("</table"),
	    _T("<font"), _T("</font"),
	    _T("<img"),
	    _T("<div"), _T("</div"),
	    _T("<tr"), _T("</tr"),
	    _T("<td"), _T("</td"),
	    _T("<u"), _T("</u"),
	    _T("<i"), _T("</i"),
	    _T("<b"), _T("</b")
	};
	for (size_t i = 0; i < _countof(g_htmlTags); ++i)
	{
	    const tstring& l_find = g_htmlTags[i];
	    size_t l_start = 0;
	    while (true)
	    {
	        l_start = p_desc.find(l_find, 0);
	        if (l_start == string::npos)
	            break;
	
	        const auto l_end = p_desc.find(_T('>'), l_start);
	        if (l_end == tstring::npos)
	        {
	            p_desc.erase(l_start, p_desc.size() - l_start);
	            return p_desc; // Did not find the closing tag? - Stop processing
	        }
	        if (i == 0)  // space instead of <br>
	        {
	            p_desc.replace(l_start, l_end - l_start + 1, _T(" "));
	            ++l_start;
	        }
	        else
	        {
	            p_desc.erase(l_start, l_end - l_start + 1);
	        }
	    }
	}
	*/
	return p_desc;
}
	
void Util::playSound(const string& p_sound, const bool p_beep /* = false */)
{
//#ifdef _DEBUG
//	LogManager::message(p_sound + (p_beep ? string(" p_beep = true") : string(" p_beep = false")));
//#endif
	if (!p_sound.empty())
	{
		PlaySound(Text::toT(p_sound).c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
	}
	else if (p_beep)
	{
		MessageBeep(MB_OK);
	}
}
StringList Util::splitSettingAndLower(const string& patternList)
{
	return StringTokenizer<string>(Text::toLower(patternList), ';').getTokens();
}
	
StringList Util::splitSettingAndReplaceSpace(string patternList)
{
	boost::replace_all(patternList, " ", "");
	return splitSettingAndLower(patternList);
}
	
string Util::toSettingString(const StringList& patternList)
{
	string ret;
	for (auto i = patternList.cbegin(), iend = patternList.cend(); i != iend; ++i)
	{
		ret += *i + ';';
	}
	if (!ret.empty())
	{
		ret.resize(ret.size() - 1);
	}
	return ret;
}
string Util::getLang()
{
	const string l_lang = SETTING(LANGUAGE_FILE);
	dcassert(l_lang.length() == 9);
	return l_lang.substr(0, 2);
}
	
string Util::getWikiLink()
{
	return HELPPAGE + getLang() + ':';
}
	
	
DWORD Util::GetTextResource(const int p_res, LPCSTR& p_data)
{
	HRSRC hResInfo = FindResource(nullptr, MAKEINTRESOURCE(p_res), RT_RCDATA);
	if (hResInfo)
	{
		HGLOBAL hResGlobal = LoadResource(nullptr, hResInfo);
		if (hResGlobal)
		{
			p_data = (LPCSTR)LockResource(hResGlobal);
			if (p_data)
			{
				return SizeofResource(nullptr, hResInfo);
			}
		}
	}
	dcassert(0);
	return 0;
}
	
void Util::WriteTextResourceToFile(const int p_res, const tstring& p_file)
{
	LPCSTR l_data;
	if (const DWORD l_size = GetTextResource(p_res, l_data))
	{
		std::ofstream l_file_out(p_file.c_str());
		l_file_out.write(l_data, l_size);
		return;
	}
	dcassert(0);
}
string Util::formatDigitalClockGMT(const time_t& p_t)
{
	return formatDigitalClock("%Y-%m-%d %H:%M:%S", p_t, true);
}
string Util::formatDigitalClock(const time_t& p_t)
{
	return formatDigitalClock("%Y-%m-%d %H:%M:%S", p_t, false);
}
string Util::formatDigitalDate()
{
	return formatDigitalClock("%Y-%m-%d", GET_TIME(), false);
}
string Util::getTempPath()
{
	LocalArray<TCHAR, MAX_PATH> buf;
	DWORD x = GetTempPath(MAX_PATH, buf.data());
	return Text::fromT(tstring(buf.data(), static_cast<size_t>(x))); // [!] PVS V106 Implicit type conversion second argument 'x' of function 'tstring' to memsize type. util.h 558
}
bool Util::isTorrentFile(const tstring& file)
{
	static const tstring l_torrent = _T(".torrent");
	return isSameFileExt(file, l_torrent, false);
}
bool Util::isDclstFile(const tstring& file)
{
	return isDclstFile(Text::fromT(file));
}
bool Util::isDclstFile(const string& file)
{
	const string l_dcls = ".dcls";
	const string l_dclst = ".dclst";
	return isSameFileExt(file, l_dcls, false) || isSameFileExt(file, l_dclst, false);
}
bool Util::isNmdc(const tstring& p_HubURL)
{
	return _wcsnicmp(L"dchub://", p_HubURL.c_str(), 8) == 0;
}
bool Util::isNmdcS(const tstring& p_HubURL)
{
	return _wcsnicmp(L"nmdcs://", p_HubURL.c_str(), 8) == 0;
}
bool Util::isAdc(const tstring& p_HubURL)
{
	return _wcsnicmp(L"adc://", p_HubURL.c_str(), 6) == 0;
}
bool Util::isAdcS(const tstring& p_HubURL)
{
	return _wcsnicmp(L"adcs://", p_HubURL.c_str(), 7) == 0;
}
bool Util::isNmdc(const string& p_HubURL)
{
	return _strnicmp("dchub://", p_HubURL.c_str(), 8) == 0;
}
bool Util::isNmdcS(const string& p_HubURL)
{
	return _strnicmp("nmdcs://", p_HubURL.c_str(), 8) == 0;
}
bool Util::isAdc(const string& p_HubURL)
{
	return _strnicmp("adc://", p_HubURL.c_str(), 6) == 0;
}
bool Util::isAdcS(const string& p_HubURL)
{
	return _strnicmp("adcs://", p_HubURL.c_str(), 7) == 0;
}
bool Util::isMagnetLink(const char* p_URL)
{
	return _strnicmp(p_URL, "magnet:?", 8) == 0;
}
bool Util::isMagnetLink(const string& p_URL)
{
	return _strnicmp(p_URL.c_str(), "magnet:?", 8) == 0;
}
bool Util::isMagnetLink(const wchar_t* p_URL)
{
	return _wcsnicmp(p_URL, L"magnet:?", 8) == 0;
}
bool Util::isMagnetLink(const tstring& p_URL)
{
	return _wcsnicmp(p_URL.c_str(), L"magnet:?", 8) == 0;
}
bool Util::isTorrentLink(const tstring& sFileName)
{
	return (sFileName.find(_T("xt=urn:btih:")) != tstring::npos &&
	        sFileName.find(_T("xt=urn:tree:tiger:")) == tstring::npos);
}
bool Util::isHttpLink(const tstring& p_url)
{
	return _wcsnicmp(p_url.c_str(), L"http://", 7) == 0;
}
bool Util::isHttpLink(const string& p_url)
{
	return strnicmp(p_url.c_str(), "http://", 7) == 0;
}
bool Util::isValidIP(const string& p_ip)
{
	uint32_t a[4] = { 0 };
	const int l_Items = sscanf_s(p_ip.c_str(), "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]);
	return  l_Items == 4 && a[0] < 256 && a[1] < 256 && a[2] < 256 && a[3] < 256; // TODO - boost
}
bool Util::isHttpsLink(const tstring& p_url)
{
	return _wcsnicmp(p_url.c_str(), L"https://", 8) == 0;
}
bool Util::isHttpsLink(const string& p_url)
{
	return strnicmp(p_url.c_str(), "https://", 8) == 0;
}
	
	
/**
 * @file
 * $Id: Util.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */
	