/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H
#define DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H

#pragma once

#include "version.h" // [+] FlylinkDC++ HE

//#define FLYLINKDC_TRACE_ENABLE

//#define IRAINMAN_INTEL_CPP_TEST 1

# define BOOST_NO_RTTI 1
#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifndef BOOST_USE_WINDOWS_H
#define BOOST_USE_WINDOWS_H
#endif

#ifdef IRAINMAN_INTEL_CPP_TEST
//#define BOOST_NO_RTTI
# ifndef __INTEL_COMPILER
#  define __INTEL_COMPILER 1210
# endif // __INTEL_COMPILER

# ifndef _NATIVE_NULLPTR_SUPPORTED
#  define nullptr NULL
# endif

#endif // IRAINMAN_INTEL_CPP_TEST


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
# pragma warning(disable: 4996)
# pragma warning(disable: 4481) // nonstandard extension used: override specifier 'override'
# pragma warning(disable: 4711) // function 'xxx' selected for automatic inline expansion
# pragma warning(disable: 4786) // identifier was truncated to '255' characters in the debug information
# pragma warning(disable: 4290) // C++ Exception Specification ignored
# pragma warning(disable: 4127) // constant expression
# pragma warning(disable: 4710) // function not inlined
# pragma warning(disable: 4503) // decorated name length exceeded, name was truncated
# pragma warning(disable: 4428) // universal-character-name encountered in source
# pragma warning(disable: 4201) // nonstadard extension used : nameless struct/union
//[+]PPA
# pragma warning(disable: 4244) // 'argument' : conversion from 'int' to 'unsigned short', possible loss of data
# pragma warning(disable: 4512) // 'boost::detail::future_object_base::relocker' : assignment operator could not be generated
# pragma warning(disable: 4100) //  unreferenced formal parameter

#ifdef __INTEL_COMPILER
# pragma warning(disable: 869)  // [IntelC++ 2012 beta2]
# pragma warning(disable: 734)  // [IntelC++ 2012 beta2]
# pragma warning(disable: 1195) // [IntelC++ 2012 beta2]
#endif

#ifdef _WIN64
# pragma warning(disable: 4267) // conversion from 'xxx' to 'yyy', possible loss of data
#endif

#if _MSC_VER == 1900
#if _MSC_FULL_VER < 190023918
#error Visual Studio 2015 Update 2 is required
// https://www.visualstudio.com/en-us/news/vs2015-update2-vs.aspx
#endif
# pragma warning(disable: 4458) // C4458: declaration of 'nativeImage' hides class member
# pragma warning(disable: 4592) // 'trustedKeyprint' : symbol will be dynamically initialized(implementation limitation)

// https://connect.microsoft.com/VisualStudio/feedback/details/1892487/code-generated-by-msvc-doesnt-operate-atomically-on-std-atomic-t-object-when-sizeof-t-n-alignof-t-n-n-2-4-8
// Enable a bugfix in VS2015 update 2, remove in the next version of VS2015
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#endif

// VC++2015
# pragma warning(disable: 4091) // 'typedef ': ignored on left of 'tagDTI_ADTIWUI' when no variable is declared

//[+]PPA
typedef signed __int8 int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;
typedef signed __int64 int64_t;

typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

//[+] Drakon
#ifdef _MSC_VER
# pragma warning (disable : 4512)
# pragma warning (disable : 4505)
# pragma warning (disable : 4100)
# ifdef _WIN64
#  ifndef _DEBUG
#   pragma warning (disable : 4267)
#   pragma warning (disable : 4244)
#  endif
# endif
#endif
//End
#ifdef __INTEL_COMPILER
# pragma warning(disable: 383) //value copied to temporary, reference to temporary used
# pragma warning(disable: 981) //operands are evaluated in unspecified order
# pragma warning(disable: 1418) //external definition with no prior declaration
# pragma warning(disable: 69) //integer conversion resulted in truncation
# pragma warning(disable: 810)
# pragma warning(disable: 811)
# pragma warning(disable: 504)
# pragma warning(disable: 809)
# pragma warning(disable: 654)
# pragma warning(disable: 181)
# pragma warning(disable: 304)
# pragma warning(disable: 444)
# pragma warning(disable: 373)
# pragma warning(disable: 174)
# pragma warning(disable: 1599)
# pragma warning(disable: 1461)
# pragma warning(disable: 869)
# pragma warning(disable: 584) //warning #584: omission of exception specification is incompatible with previous function 
# pragma warning(disable: 2259) //non-pointer conversion from "int" to "Flags::MaskType={uint16_t={unsigned short}}" may lose significant bits
# pragma warning(disable: 2267) //remark #2267: the declaration of the copy assignment operator for "StringOutputStream" has been suppressed because member "StringOutputStream::str" has reference type
# pragma warning(disable: 1) //remark #1: last line of file ends without a newline
# pragma warning(disable: 593) //remark #593: variable "bHandled" was set but never used
# pragma warning(disable: 2266) //remark #2266: the declaration of the copy assignment operator for "StringTask" has been suppressed because member "StringTask::str" is const
# pragma warning(disable: 1572) // remark #1572: floating-point equality and inequality comparisons are unreliable
# pragma warning(disable: 2268) //remark #2268: the declaration of the copy assignment operator for "MessageTask" has been suppressed because that of "StringTask" was suppressed
# pragma warning(disable: 1419) //remark #1419: external declaration in primary source file
# pragma warning(disable: 2259)
# pragma warning(disable: 2605)
# pragma warning(disable: 2602)
# pragma warning(disable: 2599)
# pragma warning(disable: 377)
# pragma warning(disable: 381)
# pragma warning(disable: 693)
# pragma warning(disable: 1879)
# pragma warning(disable: 1628)
# pragma warning(disable: 2407)
# pragma warning(disable: 367)
#endif

#define FLYLINKDC_USE_TORRENT

//[+]PPA
// #define FLYLINKDC_USE_DEAD_CODE
// #define FLYLINKDC_USE_DNS
#define FLYLINKDC_USE_AUTO_FOLLOW
#define FLYLINKDC_USE_DROP_SLOW
#define FLYLINKDC_USE_STATS_FRAME
#define FLYLINKDC_USE_SQLITE
//#define FLYLINKDC_USE_NETLIMITER
//#define FLYLINKDC_USE_ASK_SLOT // отключаем автопопрошайку
#define FLYLINKDC_USE_USE_UNORDERED_SET_SHAREMANAGER
//#define FLYLINKDC_USE_ONLINE_SWEEP_DB // Удалять файлы из базы данных если они пропали из каталога.
//#define FLYLINKDC_USE_VACUUM

#define FLYLINKDC_USE_DOS_GUARD // Включаем защиту от DoS атаки старых версий - http://www.flylinkdc.ru/2011/01/flylinkdc-dos.html
//#define FLYLINKDC_USE_OLD_INNOSETUP_WIZARD
//# define FLYLINKDC_USE_APEX_EX_MESSAGE_BOX // TODO: глючит - много дампов по переполнению стека, необходимо найти альтернативу.
#define PPA_USER_COMMANDS_HUBS_SET
#define FLYLINKDC_USE_IPFILTER
#ifndef FLYLINKDC_HE
# define FLYLINKDC_USE_LASTIP_AND_USER_RATIO
# ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
#  define FLYLINKDC_USE_COLUMN_RATIO
//#  define FLYLINKDC_USE_SHOW_UD_RATIO
# endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
#endif // FLYLINKDC_HE
#ifdef _DEBUG
//# define FLYLINKDC_USE_HIGH_LOAD_FOR_SEARCH_ENGINE_IN_DEBUG // Отдаем на поиск больше данных - в релизе никогда не включать
#endif

#define IRAINMAN_NTFS_STREAM_TTH
#define IRAINMAN_IP_AUTOUPDATE
#define IRAINMAN_ENABLE_WHOIS
#define IRAINMAN_ENABLE_MORE_CLIENT_COMMAND
//#define IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
#ifndef FLYLINKDC_HE
# ifdef FLYLINKDC_BETA
#  define AUTOUPDATE_NOT_DISABLE
#  ifdef NIGHT_BUILD
#   define HOURLY_CHECK_UPDATE
#  endif
# endif
#endif
//#define IRAINMAN_USE_STRING_POOL
#define IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
//#define IRAINMAN_SPEED_LIMITER_5S4_10 // Включает ограничение: скорость отдачи = 5 * количество слотов + 4, скорость загрузки = 10 * скорость отдачи
//#define IRAINMAN_INCLUDE_USER_CHECK // - Не понял нахрена оно нужно. если юзеров 100 тыщ то что будет?
#define IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
#define IRAINMAN_USE_BB_CODES // BB codes support http://ru.wikipedia.org/wiki/BbCode
#define IRAINMAN_EXCHANGED_UPNP_ALGORITHM // [!]IRainman UPnP refactoring
#ifdef IRAINMAN_EXCHANGED_UPNP_ALGORITHM
# define IRAINMAN_FULL_UPNP_LOG
#endif
#define SPEED_APPROXIMATION_INTERVAL_S 30 // [!]IRainman: Interval of speed approximation in seconds.
#ifndef FLYLINKDC_HE
# define IRAINMAN_ENABLE_AUTO_BAN
# define IRAINMAN_INCLUDE_SMILE // Disable this to cut all smile support from code.
# define IRAINMAN_INCLUDE_RSS // Disable this to cut rss-manager from code.
# define IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
#endif // FLYLINKDC_HE
// #define IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
#define IRAINMAN_ENABLE_OP_VIP_MODE
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
# define IRAINMAN_ENABLE_OP_VIP_MODE_ON_NMDC
#endif
#define IRAINMAN_INCLUDE_HIDE_SHARE_MOD
#define IRAINMAN_AUTOUPDATE_ALL_USERS_DATA 1
//#define IRAINMAN_AUTOUPDATE_CORE_DIFFERENCE 1 // TODO
//#define IRAINMAN_AUTOUPDATE_ARCH_DIFFERENCE 1 // TODO
//#define IRAINMAN_THEME_MANAGER_LISTENER_ENABLE
#define IRAINMAN_FAST_FLAT_TAB
//#ifndef FLYLINKDC_HE
#define IRAINMAN_USE_GDI_PLUS_TAB 1 // if disable - used old-school tab.
//#endif // FLYLINKDC_HE
//#define IRAINMAN_DISALLOWED_BAN_MSG
#ifndef IRAINMAN_DISALLOWED_BAN_MSG
// #define SMT_ENABLE_FEATURE_BAN_MSG // please DON'T enable this!
#endif
#define USE_SETTINGS_PATH_TO_UPDATA_DATA
#define IRAINMAN_USE_HIDDEN_USERS // http://adc.sourceforge.net/ADC-EXT.html#_hidden_status_for_client_type
//#endif
//#define IRAINMAN_ENABLE_TTH_GET_FLAG // This code is off. All clients support ADC teams have this flag is always set. Our version 4xx also do not use it. He is unlikely to ever need. Leave only to simplify merge.
//#define IRAINMAN_CONNECTION_MANAGER_TOKENS_DEBUG // TODO: must correct work with tokens in the ConnectionManager. This error runs either with Strong, or even from a very long time. After confirming correct downloads, I'll probably fix the problem.
//#define IRAINMAN_CORRRECT_CALL_FOR_CLIENT_MANAGER_DEBUG // TODO: correct the error with the transfer of incorrect addresses in ClientManager :: findHub: error or very old or relatively new and due to the large merzh in the making of the branches r5xx.
#define IRAINMAN_USE_NG_CORE
#ifdef IRAINMAN_USE_NG_CORE
# define IRAINMAN_ALLOW_ALL_CLIENT_FEATURES_ON_NMDC // TODO: use new ADC features.
# define IRAINMAN_USE_NG_FAST_USER_INFO 1 // optimizing the use of memory and CPU resources. 
# ifndef IRAINMAN_USE_NG_FAST_USER_INFO
//#  define IRAINMAN_USE_OLD_CODE_IN_USER_INFO_ONLY_FOR_TEST 1
# endif // IRAINMAN_USE_NG_FAST_USER_INFO
# define IRAINMAN_IDENTITY_IS_NON_COPYABLE
#ifndef _DEBUG
# define IRAINMAN_USE_SPIN_LOCK
#endif
# define IRAINMAN_USE_READ_WRITE_POLITICS
# ifdef IRAINMAN_USE_READ_WRITE_POLITICS
// [!] IRainman opt: use policies without copying data.
#  define IRAINMAN_USE_SHARED_SPIN_LOCK
#   ifdef IRAINMAN_USE_SHARED_SPIN_LOCK
#    define IRAINMAN_USE_SEPARATE_CS_IN_FAVORITE_MANAGER
#    define IRAINMAN_USE_SEPARATE_CS_IN_FINISHED_MANAGER
#   endif // IRAINMAN_USE_SHARED_SPIN_LOCK
# endif // IRAINMAN_USE_READ_WRITE_POLITICS
# ifdef FLYLINKDC_HE
#  ifdef IRAINMAN_USE_READ_WRITE_POLITICS
#// [-]   define IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
// [!] IRainman opt: use policies without copying data.
#   ifdef IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
#    define IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
#    define IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
#   endif // IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
#  endif // IRAINMAN_USE_READ_WRITE_POLITICS
# endif // FLYLINKDC_HE
#endif // IRAINMAN_USE_NG_CORE
#if defined(IRAINMAN_USE_SPIN_LOCK) || defined(IRAINMAN_USE_SHARED_SPIN_LOCK)
# define IRAINMAN_USE_NON_RECURSIVE_BEHAVIOR // TODO: recursive entry into a non-recursive mutex or spin lock.
#endif
#ifndef IRAINMAN_USE_NON_RECURSIVE_BEHAVIOR
# ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
#  define UPDATE_CON_STATUS_ON_FAV_HUBS_IN_REALTIME // TODO.
# endif
#endif
#define IRAINMAN_NOT_USE_COUNT_UPDATE_INFO_IN_LIST_VIEW_CTRL

// [+] BRAIN_RIPPER
#ifdef IRAINMAN_NTFS_STREAM_TTH
# define RIP_USE_STREAM_SUPPORT_DETECTION
#endif
//
#define RIP_USE_CONNECTION_AUTODETECT

#define RIP_USE_PORTAL_BROWSER
#ifdef _DEBUG
// #define RIP_USE_LOG_PROTOCOL // выключил т.к. сильно грузит систему
#endif
//[!] #define RIP_USE_CORAL // http://coralcdn.org/ If needed, one have to merge support for CORAL from FlylinkDC

//#define FLYLINKDC_USE_CS_CLIENT_SOCKET [-] IRainman fix.

//#define STRONG_USE_DHT

//#define FLYLINKDC_USE_VIEW_AS_TEXT_OPTION

//[+]IRainman FlylinkDC working with long paths
// http://msdn.microsoft.com/en-us/library/ee681827(VS.85).aspx#limits
#define FULL_MAX_PATH 32760 + 255 + 255 + 8 // Maximum path name length + Maximum file size + Mashine name overhed in UNC path + UNC header
// [+] NightOrion
#define USE_SUPPORT_HUB
//#define NIGHTORION_INTERNAL_TRANSLATE_SOCKET_ERRORS // Включить когда будет готов перевод.
#ifndef FLYLINKDC_HE
# define USE_APPDATA
#endif
#define FLYLINKDC_LOG_IN_SQLITE_BASE

//#define NIGHTORION_USE_STATISTICS_REQUEST

//#define USE_OFFLINE_ICON_FOR_FILELIST // [+] InfinitySky. Нужна доработка. // not yet implemented, don't turn on
// [+] SSA
// SSA_SAVE_LAST_NICK_MACROS нигде не юзается
// #define SSA_SAVE_LAST_NICK_MACROS // /n Macros for last inserted Nick in chat. L: Always enabled.
// [+] SSA - uncomment to use mediainfo.dll (not static)
// [+] SSA - Remove needless words from MediaInfo
#define SSA_REMOVE_NEEDLESS_WORDS_FROM_VIDEO_AUDIO_INFO
// [+] SSA - новый алгоритм поиска имени файла и title для WinAmp
#define SSA_NEW_WINAMP_PROC_FOR_TITLE_AND_FILENAME
// [+] SSA - отображать пусто в magnet, если не найден файл в шаре
// #define SSA_DONT_SHOW_MAGNET_ON_NO_FILE_IN_SHARE
//#define SSA_INCLUDE_FILE_SHARE_PAGE // [!] SSA temporary removed. TODO ?
#ifndef FLYLINKDC_HE
# define SSA_IPGRANT_FEATURE // [+] SSA additional slots for special IP's
// # define SSA_VIDEO_PREVIEW_FEATURE //
# define SSA_WIZARD_FEATURE
#endif
// TODO
//#define SSA_SHELL_INTEGRATION

#ifndef FLYLINKDC_HE
# define SCALOLAZ_PROPPAGE_TRANSPARENCY   //[+] SCALOlaz: Transparency of a Settings window
#endif
#define SCALOLAZ_PROPPAGE_COLOR          //[+] SCALOlaz: Colorized background in Properties
#define SCALOLAZ_PROPPAGE_HELPLINK       //[+] SCALOlaz: FlyWiki Help link per pages
#define SCALOLAZ_HASH_HELPLINK          //[+] SCALOlaz: FlyWiki Help link for HashDialog
#define SCALOLAZ_HUB_SWITCH_BTN    //[+] SCALOlaz: Switch Panels button, change location Chat - Userlist
// TODO: SCALOLAZ_HUB_MODE - In some situations, spoils the whole interface and the process hangs. Need verifycation, optimization
#define SCALOLAZ_HUB_MODE       //[~+] SCALOlaz: Hubs Mode Picture
#define SCALOLAZ_SPEEDLIMIT_DLG // Speed Limit (Up/Dnl) control for StatusBar
#define SCALOLAZ_MEDIAVIDEO_ICO // HD, SD icons on files in filelist
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
//# define PPA_MEDIAVIDEO_BOLD_TEXT  //May configured from Settings (Interface) as bool?
#endif // SCALOLAZ_MEDIAVIDEO_ICO
#define SCALOLAZ_CHAT_REFFERING_TO_NICK
#define SCALOLAZ_BB_COLOR_BUTTON

// TODO: feature not fully implemented, don't turn off this define!
//#if defined(FLYLINKDC_HE) || !defined(FLYLINKDC_BETA) // before the end of the experiments using the old menu, eliminates the hassle.
//# define OLD_MENU_HEADER // [+] JhaoDa: if disable use new menu style, like Windows 7 +
//#endif

#ifdef IRAINMAN_INCLUDE_SMILE
# define IRAINMAN_INCLUDE_GDI_OLE 1
#endif

#if defined(IRAINMAN_INCLUDE_GDI_OLE) || defined(IRAINMAN_USE_GDI_PLUS_TAB)
# define IRAINMAN_INCLUDE_GDI_INIT
#endif
/* TODO.
#ifndef PROPPAGE_COLOR
# define USE_SET_LIST_COLOR_IN_SETTINGS
#endif
*/

// #define FLYLINKDC_USE_SETTINGS_AUTO_UPDATE
//#define USE_REBUILD_MEDIAINFO
#define FLYLINKDC_USE_MEDIAINFO_SERVER
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
// #define FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION // Не собирается стата (некому обрабатывать)
#define FLYLINKDC_USE_GATHER_STATISTICS
#endif
#ifdef FLYLINKDC_BETA
#define FLYLINKDC_COLLECT_UNKNOWN_TAG
#ifdef _DEBUG
#define FLYLINKDC_COLLECT_UNKNOWN_FEATURES
#endif
#endif
#ifdef _DEBUG
// #define FLYLINKDC_USE_GATHER_IDENTITY_STAT
#endif
//#define FLYLINKDC_USE_SQL_EXPLORER // TODO
//#define FLYLINKDC_USE_LIST_VIEW_WATER_MARK // Свистелку отрубить
//#define FLYLINKDC_USE_LIST_VIEW_MATTRESS   // Отключаем полосатые листвью (бестолковая трата ресурсов)


#define FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
#ifdef _DEBUG
// #define FLYLINKDC_USE_COLLECT_STAT  // Собираем статистику команд коннектов и поиска для блокировки DDoS атак http://dchublist.ru/forum/viewtopic.php?f=6&t=1028&hilit=Ddos
// #define FLYLINKDC_USE_LOG_FOR_DUPLICATE_TTH_SEARCH
// #define FLYLINKDC_USE_LOG_FOR_DUPLICATE_FILE_SEARCH
#endif

#define FLYLINKDC_SUPPORT_HUBTOPIC
//#define FLYLINKDC_USE_ANTIVIRUS_DB
#define FLYLINKDC_USE_GEO_IP
#define FLYLINKDC_USE_DDOS_DETECT
// TODO #define FLYLINKDC_USE_GPU_TTH

#define FLYLINKDC_USE_EXT_JSON
#define FLYLINKDC_USE_XXX_ICON
#define FLYLINKDC_USE_EXTERNAL_MAIN_ICON
#define FLYLINKDC_USE_SOCKET_COUNTER

// Make sure we're using the templates from algorithm...
#ifdef min
# undef min
#endif
#ifdef max
# undef max
#endif

#ifdef _DEBUG

#if 0
template <class NonDerivableClass>
class NonDerivable // [+] IRainman fix.
{
		friend NonDerivableClass;
	private:
		NonDerivable() { }
		NonDerivable(const NonDerivable&) { }
};
#endif
#endif // _DEBUG

#endif // DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H
