/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#pragma once

#ifndef DCPLUSPLUS_CLIENT_SETTINGS_MANAGER_H
#define DCPLUSPLUS_CLIENT_SETTINGS_MANAGER_H

#include "Util.h"
#include "Speaker.h"
#include "Singleton.h"
#include "..\boost\boost\logic\tribool.hpp"
#define MAX_SOCKET_BUFFER_SIZE (64 * 1024) // [+] IRainman fix.

#define URL_GET_IP_DEFAULT  "http://checkip.dyndns.com"

STANDARD_EXCEPTION(SearchTypeException);

class SimpleXML;

class SettingsManagerListener
{
	public:
		virtual ~SettingsManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Load;
		typedef X<1> Repaint;
		
		virtual void on(Load, SimpleXML&) { }
		virtual void on(Repaint) { }
};

class SettingsManager : public Singleton<SettingsManager>, public Speaker<SettingsManagerListener>
{
	public:
	
		typedef boost::unordered_map<string, StringList> SearchTypes;
		typedef SearchTypes::iterator SearchTypesIter;
		
		static StringList g_connectionSpeeds;
		static boost::logic::tribool g_TestUDPSearchLevel;
		static boost::logic::tribool g_TestUDPDHTLevel;
		static boost::logic::tribool g_TestTCPLevel;
		static boost::logic::tribool g_TestTLSLevel;
		
		static boost::logic::tribool g_upnpUDPSearchLevel;
		static boost::logic::tribool g_upnpUDPDHTLevel;
		static boost::logic::tribool g_upnpTCPLevel;
		static boost::logic::tribool g_upnpTLSLevel;
		
		static void upnpPortLevelInit()
		{
			g_upnpUDPSearchLevel = boost::logic::indeterminate;
			g_upnpUDPDHTLevel = boost::logic::indeterminate;
			g_upnpTCPLevel = boost::logic::indeterminate;
			g_upnpTLSLevel = boost::logic::indeterminate;
		}
		
		static void testPortLevelInit()
		{
			g_TestUDPSearchLevel = boost::logic::indeterminate;
			g_TestUDPDHTLevel = boost::logic::indeterminate;
			g_TestTCPLevel = boost::logic::indeterminate;
			g_TestTLSLevel = boost::logic::indeterminate;
		}
		static string g_UDPTestExternalIP;
		
		enum StrSetting { STR_FIRST,
		                  NICK = STR_FIRST, UPLOAD_SPEED, DESCRIPTION, DOWNLOAD_DIRECTORY, EMAIL, EXTERNAL_IP,
		                  TEXT_FONT, MAINFRAME_ORDER, MAINFRAME_WIDTHS, HUBFRAME_ORDER, HUBFRAME_WIDTHS,
		                  
		                  DEFAULT_CODEPAGE, LANGUAGE_FILE, SEARCHFRAME_ORDER, SEARCHFRAME_WIDTHS, FAVORITESFRAME_ORDER, FAVORITESFRAME_WIDTHS, FAVORITESFRAME_VISIBLE,
		                  HUBLIST_SERVERS, QUEUEFRAME_ORDER, QUEUEFRAME_WIDTHS, PUBLICHUBSFRAME_ORDER, PUBLICHUBSFRAME_WIDTHS, PUBLICHUBSFRAME_VISIBLE,
		                  USERSFRAME_ORDER, USERSFRAME_WIDTHS, USERSFRAME_VISIBLE, LOG_DIRECTORY, LOG_FORMAT_POST_DOWNLOAD,
		                  
		                  LOG_FORMAT_POST_UPLOAD, LOG_FORMAT_MAIN_CHAT, LOG_FORMAT_PRIVATE_CHAT, FINISHED_ORDER, FINISHED_WIDTHS,
		                  TEMP_DOWNLOAD_DIRECTORY, BIND_ADDRESS, SOCKS_SERVER, SOCKS_USER, SOCKS_PASSWORD, CONFIG_VERSION,
		                  
		                  DEFAULT_AWAY_MESSAGE, TIME_STAMPS_FORMAT, ADLSEARCHFRAME_ORDER, ADLSEARCHFRAME_WIDTHS, ADLSEARCHFRAME_VISIBLE,
		                  FINISHED_UL_WIDTHS, FINISHED_UL_ORDER, PRIVATE_ID, SPYFRAME_WIDTHS, SPYFRAME_ORDER, SPYFRAME_VISIBLE,
		                  
		                  SOUND_BEEPFILE, SOUND_BEGINFILE, SOUND_FINISHFILE, SOUND_SOURCEFILE, SOUND_UPLOADFILE, SOUND_FAKERFILE, SOUND_CHATNAMEFILE, SOUND_TTH, SOUND_HUBCON,  SOUND_HUBDISCON, SOUND_FAVUSER, SOUND_FAVUSER_OFFLINE, SOUND_TYPING_NOTIFY, SOUND_SEARCHSPY,
		                  
		                  KICK_MSG_RECENT_01, KICK_MSG_RECENT_02, KICK_MSG_RECENT_03, KICK_MSG_RECENT_04, KICK_MSG_RECENT_05,
		                  KICK_MSG_RECENT_06, KICK_MSG_RECENT_07, KICK_MSG_RECENT_08, KICK_MSG_RECENT_09, KICK_MSG_RECENT_10,
		                  KICK_MSG_RECENT_11, KICK_MSG_RECENT_12, KICK_MSG_RECENT_13, KICK_MSG_RECENT_14, KICK_MSG_RECENT_15,
		                  KICK_MSG_RECENT_16, KICK_MSG_RECENT_17, KICK_MSG_RECENT_18, KICK_MSG_RECENT_19, KICK_MSG_RECENT_20,
		                  TOOLBAR, TOOLBARIMAGE, TOOLBARHOTIMAGE, USERLIST_IMAGE,
		                  UPLOADQUEUEFRAME_ORDER, UPLOADQUEUEFRAME_WIDTHS,
		                  
		                  WINAMP_FORMAT,
		                  
		                  WEBSERVER_POWER_USER, WEBSERVER_POWER_PASS, WEBSERVER_BIND_ADDRESS,// [+] IRainman
		                  LOG_FORMAT_WEBSERVER, LOG_FORMAT_CUSTOM_LOCATION, LOG_FORMAT_TRACE_SQLITE, LOG_FORMAT_VIRUS_TRACE,
		                  LOG_FORMAT_DDOS_TRACE, LOG_FORMAT_DHT_TRACE, LOG_FORMAT_PSR_TRACE, LOG_FORMAT_FLOOD_TRACE, LOG_FORMAT_CMDDEBUG_TRACE,
		                  WEBSERVER_USER, WEBSERVER_PASS, LOG_FILE_MAIN_CHAT,
		                  LOG_FILE_PRIVATE_CHAT, LOG_FILE_STATUS, LOG_FILE_UPLOAD, LOG_FILE_DOWNLOAD, LOG_FILE_SYSTEM, LOG_FORMAT_SYSTEM,
		                  LOG_FORMAT_STATUS, LOG_FILE_WEBSERVER, LOG_FILE_CUSTOM_LOCATION,
		                  
		                  LOG_FILE_TRACE_SQLITE,
		                  LOG_FILE_VIRUS_TRACE,
		                  LOG_FILE_DDOS_TRACE,
		                  LOG_FILE_CMDDEBUG_TRACE,
		                  LOG_FILE_DHT_TRACE,
		                  LOG_FILE_PSR_TRACE,
		                  LOG_FILE_FLOOD_TRACE,
		                  
		                  DIRECTORYLISTINGFRAME_ORDER, DIRECTORYLISTINGFRAME_WIDTHS,
		                  MAINFRAME_VISIBLE, SEARCHFRAME_VISIBLE, QUEUEFRAME_VISIBLE, HUBFRAME_VISIBLE, UPLOADQUEUEFRAME_VISIBLE,
		                  EMOTICONS_FILE,
		                  TLS_PRIVATE_KEY_FILE, TLS_CERTIFICATE_FILE, TLS_TRUSTED_CERTIFICATES_PATH,
		                  FINISHED_VISIBLE, FINISHED_UL_VISIBLE, BETAUSR, BETAPASS, PASSWORD, SKIPLIST_SHARE, DIRECTORYLISTINGFRAME_VISIBLE,
		                  
		                  RECENTFRAME_ORDER, RECENTFRAME_WIDTHS, RECENTFRAME_VISIBLE, HIGH_PRIO_FILES, LOW_PRIO_FILES, SECONDARY_AWAY_MESSAGE, PROT_USERS, WMP_FORMAT,
		                  ITUNES_FORMAT, MPLAYERC_FORMAT, RAW1_TEXT, RAW2_TEXT, RAW3_TEXT, RAW4_TEXT, RAW5_TEXT, POPUP_FONT, POPUP_TITLE_FONT, POPUPFILE,
		                  
		                  BAN_MESSAGE, SLOT_ASK, // !SMT!-S
		                  URL_GET_IP, // [+]PPA
		                  PM_PASSWORD, PM_PASSWORD_HINT, PM_PASSWORD_OK_HINT, // !SMT!-PSW
		                  COPY_WMLINK,//[+]necros
		                  RATIO_TEMPLATE, //[+] WhiteD. Custom ratio message
		                  URL_IPTRUST, //[+]PPA
		                  TOOLBAR_SETTINGS, // [*]Drakon
		                  WINAMPTOOLBAR,  // [*]Drakon
#ifdef RIP_USE_LOG_PROTOCOL
		                  LOG_FILE_PROTOCOL, LOG_FORMAT_PROTOCOL,
#endif
		                  CUSTOM_MENU_PATH, // [+] SSA
		                  RSS_COLUMNS_ORDER, // [+] SSA
		                  RSS_COLUMNS_WIDTHS, // [+] SSA
		                  RSS_COLUMNS_VISIBLE, // [+] SSA
#ifdef STRONG_USE_DHT
		                  DHT_KEY,
#endif
		                  MAPPER,
		                  PORTAL_BROWSER_UPDATE_URL, // [+]PPA
		                  ISP_RESOURCE_ROOT_URL, // [+]PPA
		                  BT_MAGNET_OPEN_CMD, //[+]IRainman
		                  //UPDATE_CHANEL, // [+] IRainman TODO update chanels
		                  THEME_MANAGER_THEME_DLL_NAME, // [+] SSA
		                  THEME_MANAGER_SOUNDS_THEME_NAME, // [+] SCALOlaz: Sound Themes
		                  AUTOUPDATE_SERVER_URL, // [+] SSA
		                  AUTOUPDATE_IGNORE_VERSION, // [+] SSA
		                  AUTOUPDATE_PATH_WITH_UPDATE, // [+] SSA
		                  JETAUDIO_FORMAT,
		                  QCDQMP_FORMAT,
		                  DCLST_FOLDER_DIR, // [+] SSA
		                  INT_PREVIEW_CLIENT_PATH, // [+] SSA
		                  SAVED_SEARCH_SIZE,
		                  FLY_LOCATOR_COUNTRY,
		                  FLY_LOCATOR_CITY,
		                  FLY_LOCATOR_ISP,
		                  GPU_DEV_NAME_FOR_TTH_COMP,
		                  STR_LAST
		                };
		                
		enum IntSetting { INT_FIRST = STR_LAST + 1,
		                  INCOMING_CONNECTIONS = INT_FIRST, AVDB_BLOCK_CONNECTIONS, AUTO_PASSIVE_INCOMING_CONNECTIONS, FORCE_PASSIVE_INCOMING_CONNECTIONS, TCP_PORT, SLOTS, AUTO_FOLLOW, CLEAR_SEARCH,
		                  BACKGROUND_COLOR, TEXT_COLOR, SHARE_HIDDEN,
		                  SHARE_VIRTUAL, SHARE_SYSTEM, // [+]IRainman
		                  FILTER_MESSAGES, MINIMIZE_TRAY,
		                  AUTO_SEARCH, TIME_STAMPS, CONFIRM_EXIT,
		                  SUPPRESS_PMS, // [+]IRainman
		                  POPUP_HUB_PMS, POPUP_BOT_PMS, IGNORE_HUB_PMS, IGNORE_BOT_PMS,
		                  BUFFER_SIZE_FOR_DOWNLOADS, DOWNLOAD_SLOTS, MAX_DOWNLOAD_SPEED, LOG_MAIN_CHAT, LOG_PRIVATE_CHAT,
		                  LOG_DOWNLOADS, LOG_UPLOADS,
		                  LOG_IF_SUPPRESS_PMS, // [+]IRainman
		                  STATUS_IN_CHAT, SHOW_JOINS, PRIVATE_MESSAGE_BEEP, PRIVATE_MESSAGE_BEEP_OPEN,
		                  USE_SYSTEM_ICONS, POPUP_PMS, MIN_UPLOAD_SPEED, URL_HANDLER, MAIN_WINDOW_STATE,
		                  MAIN_WINDOW_SIZE_X, MAIN_WINDOW_SIZE_Y, MAIN_WINDOW_POS_X, MAIN_WINDOW_POS_Y, AUTO_AWAY,
		                  SOCKS_PORT, SOCKS_RESOLVE, KEEP_LISTS, AUTO_KICK, QUEUEFRAME_SHOW_TREE, QUEUEFRAME_SPLIT,
		                  COMPRESS_TRANSFERS, SHOW_PROGRESS_BARS, MAX_TAB_ROWS,
		                  MAX_COMPRESSION, ANTI_FRAG, ANTI_FRAG_MAX, MDI_MAXIMIZED,
		                  // [-] NO_AWAYMSG_TO_BOTS, [-] IRainman fix.
		                  SKIP_ZERO_BYTE, ADLS_BREAK_ON_FIRST,
		                  HUB_USER_COMMANDS,
		                  SEND_BLOOM,
		                  AUTO_SEARCH_AUTO_MATCH, DOWNLOAD_BAR_COLOR, UPLOAD_BAR_COLOR, LOG_SYSTEM,
		                  LOG_CUSTOM_LOCATION, // [+] IRainman
		                  
		                  LOG_SQLITE_TRACE,
		                  LOG_VIRUS_TRACE,
		                  LOG_DDOS_TRACE,
		                  LOG_CMDDEBUG_TRACE,
		                  LOG_DHT_TRACE,
		                  LOG_PSR_TRACE,
		                  LOG_FLOOD_TRACE,
		                  
		                  LOG_FILELIST_TRANSFERS, SHOW_STATUSBAR, SHOW_TOOLBAR, SHOW_TRANSFERVIEW, SHOW_TRANSFERVIEW_TOOLBAR,
		                  SEARCH_PASSIVE, SET_MINISLOT_SIZE, SHUTDOWN_TIMEOUT,
		                  // MAX_UPLOAD_SPEED_LIMIT, MAX_DOWNLOAD_SPEED_LIMIT, // [-]IRainman SpeedLimiter
		                  EXTRA_SLOTS,
		                  TEXT_GENERAL_BACK_COLOR, TEXT_GENERAL_FORE_COLOR, TEXT_GENERAL_BOLD, TEXT_GENERAL_ITALIC,
		                  TEXT_MYOWN_BACK_COLOR, TEXT_MYOWN_FORE_COLOR, TEXT_MYOWN_BOLD, TEXT_MYOWN_ITALIC,
		                  TEXT_PRIVATE_BACK_COLOR, TEXT_PRIVATE_FORE_COLOR, TEXT_PRIVATE_BOLD, TEXT_PRIVATE_ITALIC,
		                  TEXT_SYSTEM_BACK_COLOR, TEXT_SYSTEM_FORE_COLOR, TEXT_SYSTEM_BOLD, TEXT_SYSTEM_ITALIC,
		                  TEXT_SERVER_BACK_COLOR, TEXT_SERVER_FORE_COLOR, TEXT_SERVER_BOLD, TEXT_SERVER_ITALIC,
		                  TEXT_TIMESTAMP_BACK_COLOR, TEXT_TIMESTAMP_FORE_COLOR, TEXT_TIMESTAMP_BOLD, TEXT_TIMESTAMP_ITALIC,
		                  TEXT_MYNICK_BACK_COLOR, TEXT_MYNICK_FORE_COLOR, TEXT_MYNICK_BOLD, TEXT_MYNICK_ITALIC,
		                  TEXT_FAV_BACK_COLOR, TEXT_FAV_FORE_COLOR, TEXT_FAV_BOLD, TEXT_FAV_ITALIC,
		                  TEXT_OP_BACK_COLOR, TEXT_OP_FORE_COLOR, TEXT_OP_BOLD, TEXT_OP_ITALIC,
		                  TEXT_URL_BACK_COLOR, TEXT_URL_FORE_COLOR, TEXT_URL_BOLD, TEXT_URL_ITALIC,
		                  TEXT_ENEMY_BACK_COLOR, TEXT_ENEMY_FORE_COLOR, TEXT_ENEMY_BOLD, TEXT_ENEMY_ITALIC,
		                  BOLD_AUTHOR_MESS, WINDOWS_STYLE_URL, MAX_UPLOAD_SPEED_LIMIT_NORMAL, THROTTLE_ENABLE, HUB_SLOTS,
		                  MAX_DOWNLOAD_SPEED_LIMIT_NORMAL, MAX_UPLOAD_SPEED_LIMIT_TIME, MAX_DOWNLOAD_SPEED_LIMIT_TIME,
		                  TIME_DEPENDENT_THROTTLE, BANDWIDTH_LIMIT_START, BANDWIDTH_LIMIT_END, REMOVE_FORBIDDEN,
		                  PROGRESS_TEXT_COLOR_DOWN, PROGRESS_TEXT_COLOR_UP, SHOW_INFOTIPS, EXTRA_DOWNLOAD_SLOTS,
		                  MINIMIZE_ON_STARTUP, CONFIRM_DELETE, FREE_SLOTS_DEFAULT,
		                  USE_EXTENSION_DOWNTO, ERROR_COLOR, TRANSFER_SPLIT_SIZE,
		                  DISCONNECT_SPEED, DISCONNECT_FILE_SPEED, DISCONNECT_TIME,
		                  PROGRESS_OVERRIDE_COLORS, PROGRESS_3DDEPTH, PROGRESS_OVERRIDE_COLORS2,
		                  MENUBAR_TWO_COLORS, MENUBAR_LEFT_COLOR, MENUBAR_RIGHT_COLOR, MENUBAR_BUMPED,
		                  DISCONNECTING_ENABLE, DISCONNECT_FILESIZE, UPLOADQUEUEFRAME_SHOW_TREE, UPLOADQUEUEFRAME_SPLIT,
		                  SEGMENTS_MANUAL, NUMBER_OF_SEGMENTS, PERCENT_FAKE_SHARE_TOLERATED,
		                  SEND_UNKNOWN_COMMANDS, REMOVE_SPEED,
		                  IPUPDATE, WAN_IP_MANUAL, IPUPDATE_INTERVAL,
		                  MAX_HASH_SPEED,
		                  SAVE_TTH_IN_NTFS_FILESTREAM, // [+] IRainman
		                  SET_MIN_LENGHT_TTH_IN_NTFS_FILESTREAM, // [+] NightOrion
		                  AUTO_PRIORITY_DEFAULT, USE_OLD_SHARING_UI,
		                  FAV_SHOW_JOINS, LOG_STATUS_MESSAGES, SHOW_LAST_LINES_LOG, SEARCH_ALTERNATE_COLOUR, SOUNDS_DISABLED,
		                  POPUPS_DISABLED, // [+] InfinitySky.
		                  POPUPS_TABS_ENABLED, // [+] SCALOlaz
		                  POPUPS_MESSAGEPANEL_ENABLED,
		                  USE_12_HOUR_FORMAT, // [+] InfinitySky.
		                  MINIMIZE_ON_CLOSE, // [+] InfinitySky.
		                  SHOW_CUSTOM_MINI_ICON_ON_TASKBAR, // [+] InfinitySky.
		                  SHOW_CURRENT_SPEED_IN_TITLE, // [+] InfinitySky.
		                  SHOW_FULL_HUB_INFO_ON_TAB, // [+] NightOrion.
		                  REPORT_ALTERNATES, CHECK_NEW_USERS, GARBAGE_COMMAND_INCOMING, GARBAGE_COMMAND_OUTGOING,
		                  AUTO_SEARCH_TIME, DONT_BEGIN_SEGMENT, DONT_BEGIN_SEGMENT_SPEED, POPUNDER_PM, POPUNDER_FILELIST,
		                  DROP_MULTISOURCE_ONLY, DISPLAY_CHEATS_IN_MAIN_CHAT, MAGNET_ASK, MAGNET_ACTION, MAGNET_REGISTER,
		                  DISCONNECT_RAW, TIMEOUT_RAW, FAKESHARE_RAW, LISTLEN_MISMATCH, FILELIST_TOO_SMALL, FILELIST_UNAVAILABLE,
		                  AWAY, USE_CTRL_FOR_LINE_HISTORY,
		                  POPUP_HUB_CONNECTED, POPUP_HUB_DISCONNECTED, POPUP_FAVORITE_CONNECTED, POPUP_FAVORITE_DISCONNECTED, POPUP_CHEATING_USER,
		                  POPUP_CHAT_LINE, // [+] ProKK
		                  POPUP_DOWNLOAD_START,
		                  POPUP_DOWNLOAD_FAILED, POPUP_DOWNLOAD_FINISHED, POPUP_UPLOAD_FINISHED, POPUP_PM, POPUP_NEW_PM, POPUP_SEARCH_SPY,
		                  POPUP_TYPE, WEBSERVER, WEBSERVER_PORT,
		                  WEBSERVER_SEARCHSIZE, WEBSERVER_SEARCHPAGESIZE, WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR, WEBSERVER_ALLOW_UPNP, //[+] IRainman
		                  LOG_WEBSERVER, SHUTDOWN_ACTION, MINIMUM_SEARCH_INTERVAL,
		                  POPUP_AWAY, POPUP_MINIMIZED, SHOW_SHARE_CHECKED_USERS, MAX_AUTO_MATCH_SOURCES,
		                  RESERVED_SLOT_COLOR, IGNORED_COLOR, FAVORITE_COLOR, NORMAL_COLOUR, FIREBALL_COLOR, SERVER_COLOR, PASIVE_COLOR, OP_COLOR,
		                  FULL_CHECKED_COLOUR, BAD_CLIENT_COLOUR, BAD_FILELIST_COLOUR, DONT_DL_ALREADY_SHARED,
		                  CONFIRM_HUB_REMOVAL, CONFIRM_HUBGROUP_REMOVAL, CONFIRM_USER_REMOVAL, SUPPRESS_MAIN_CHAT, PROGRESS_BACK_COLOR, PROGRESS_COMPRESS_COLOR, PROGRESS_SEGMENT_COLOR,
		                  JOIN_OPEN_NEW_WINDOW, FILE_SLOTS, UDP_PORT, ENABLE_MULTI_CHUNK,
		                  USERLIST_DBLCLICK, TRANSFERLIST_DBLCLICK, CHAT_DBLCLICK, ADC_DEBUG, NMDC_DEBUG,
		                  TOGGLE_ACTIVE_WINDOW, PROGRESSBAR_ODC_STYLE, SEARCH_HISTORY,
		                  //BADSOFT_DETECTIONS, DETECT_BADSOFT, [-]
		                  //ADVANCED_RESUME, [-] merge
		                  ACCEPTED_DISCONNECTS, ACCEPTED_TIMEOUTS,
		                  OPEN_PUBLIC, OPEN_FAVORITE_HUBS, OPEN_FAVORITE_USERS, OPEN_QUEUE, OPEN_FINISHED_DOWNLOADS,
		                  OPEN_FINISHED_UPLOADS, OPEN_SEARCH_SPY, OPEN_NETWORK_STATISTICS, OPEN_NOTEPAD, OUTGOING_CONNECTIONS,
		                  NO_IP_OVERRIDE, FORGET_SEARCH_REQUEST, SAVE_SEARCH_SETTINGS, USE_SEARCH_GROUP_TREE_SETTINGS, SAVED_SEARCH_TYPE, SAVED_SEARCH_SIZEMODE, SAVED_SEARCH_MODE, BOLD_FINISHED_DOWNLOADS, BOLD_FINISHED_UPLOADS, BOLD_QUEUE,
		                  BOLD_HUB, BOLD_PM, BOLD_SEARCH, BOLD_NEWRSS, TABS_POS,
		                  HUB_POSITION, // [+] InfinitySky.
		                  SOCKET_IN_BUFFER, SOCKET_OUT_BUFFER,
		                  COLOR_RUNNING, COLOR_DOWNLOADED, COLOR_VERIFIED, COLOR_AVOIDING, AUTO_REFRESH_TIME, OPEN_WAITING_USERS,
		                  BOLD_WAITING_USERS, AUTO_SEARCH_LIMIT, AUTO_KICK_NO_FAVS, PROMPT_PASSWORD, SPY_FRAME_IGNORE_TTH_SEARCHES,
		                  TLS_PORT, USE_TLS, MAX_COMMAND_LENGTH, ALLOW_UNTRUSTED_HUBS, ALLOW_UNTRUSTED_CLIENTS,
		                  FAST_HASH, DOWNCONN_PER_SEC,
		                  PRIO_HIGHEST_SIZE, PRIO_HIGH_SIZE, PRIO_NORMAL_SIZE, PRIO_LOW_SIZE, PRIO_LOWEST,
		                  ADD_TO_DESCRIPTION, ADD_DESCRIPTION_SLOTS, ADD_DESCRIPTION_LIMIT,
		                  PROTECT_TRAY, PROTECT_START, PROTECT_CLOSE,
		                  STRIP_TOPIC, TB_IMAGE_SIZE, TB_IMAGE_SIZE_HOT, OPEN_CDMDEBUG, SHOW_WINAMP_CONTROL, USER_THERSHOLD, PG_ENABLE, PG_UP, PG_DOWN, PG_SEARCH, PG_LOG,
		                  PM_PREVIEW, FILTER_ENTER, POPUP_TIME, POPUP_W, POPUP_H, POPUP_TRANSP, AWAY_TIME_THROTTLE, AWAY_START, AWAY_END, PROGRESSBAR_ODC_BUMPED,
		                  TOP_SPEED, STEALTHY_STYLE, STEALTHY_STYLE_ICO, STEALTHY_STYLE_ICO_SPEEDIGNORE, PSR_DELAY,
		                  IP_IN_CHAT, COUNTRY_IN_CHAT, ISP_IN_CHAT, TOP_UP_SPEED, BROADCAST, REMEMBER_SETTINGS_PAGE, PAGE, REMEMBER_SETTINGS_WINDOW_POS, SETTINGS_WINDOW_POS_X,
		                  SETTINGS_WINDOW_POS_Y, SETTINGS_WINDOW_SIZE_X, SETTINGS_WINDOW_SIZE_YY, SETTINGS_WINDOW_TRANSP, SETTINGS_WINDOW_COLORIZE, SETTINGS_WINDOW_WIKIHELP,
		                  CHATBUFFERSIZE, ENABLE_HUBMODE_PIC, ENABLE_COUNTRYFLAG, PG_LAST_UP,
		                  DIRECTORYLISTINGFRAME_SPLIT,
		                  FLYSERVER_HUBLIST_SPLIT,
#ifdef IRAINMAN_INCLUDE_TEXT_FORMATTING
		                  FORMAT_BIU,
#endif
		                  MEDIA_PLAYER, PROT_FAVS, MAX_MSG_LENGTH, POPUP_BACKCOLOR, POPUP_TEXTCOLOR, POPUP_TITLE_TEXTCOLOR, POPUP_IMAGE, POPUP_COLORS, SORT_FAVUSERS_FIRST, SHOW_SHELL_MENU, OPEN_LOGS_INTERNAL,
		                  
		                  NSLOOKUP_MODE, NSLOOKUP_DELAY, // !SMT!-IP
		                  ENABLE_AUTO_BAN, // [+] IRainman
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
		                  AUTOBAN_PPROTECT_OP,
#endif
		                  BAN_SLOTS, BAN_SLOTS_H, /*old BAN_SHARE_MAX,*/ BAN_SHARE, BAN_LIMIT, BANMSG_PERIOD, BAN_STEALTH, BAN_FORCE_PM, /*old BAN_SKIP_OPS,*/ EXTRASLOT_TO_DL, // !SMT!-S
		                  BAN_COLOR, DUPE_COLOR, VIRUS_COLOR, MULTILINE_CHAT_INPUT, EXTERNAL_PREVIEW, SEND_SLOTGRANT_MSG, FAVUSERLIST_DBLCLICK, // !SMT!-UI
		                  PROTECT_PRIVATE, PROTECT_PRIVATE_RND, PROTECT_PRIVATE_SAY, // !SMT!-PSW
		                  
		                  UP_TRANSFER_COLORS, // By Drakon
		                  STARTUP_BACKUP, // By Drakon
		                  
		                  GLOBAL_HUBFRAME_CONF,
		                  IGNORE_USE_REGEXP_OR_WC,
		                  STEALTHY_INDICATE_SPEEDS, COLOUR_DUPE, NO_TTH_CHEAT,
		                  DELETE_CHECKED, TOPMOST, LOCK_TOOLBARS,
		                  //AUTO_COMPLETE_SEARCH,//[-]IRainman
		                  KEEP_DL_HISTORY, KEEP_UL_HISTORY,
		                  SHOW_QUICK_SEARCH, SEARCH_DETECT_TTH, FULL_FILELIST_NFO, TABS_CLOSEBUTTONS,
		                  VIEW_GRIDCONTROLS, // [+] ZagZag
		                  DUPE_EX1_COLOR, DUPE_EX2_COLOR, NSL_IGNORE_ME,// [+]NSL
		                  ENABLE_LAST_IP_AND_MESSAGE_COUNTER,
		                  ENABLE_FLY_SERVER,
		                  ENABLE_HIT_FILE_LIST,
		                  ENABLE_RATIO_USER_LIST,
		                  NON_HUBS_FRONT, BLEND_OFFLINE_SEARCH,
		                  MAX_RESIZE_LINES,
		                  USE_CUSTOM_LIST_BACKGROUND,
		                  ENABLE_P2P_GUARD,
		                  ENABLE_IPGUARD, IP_GUARD_IS_DENY_ALL,
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
		                  FLY_TEXT_LOG, FLY_SQLITE_LOG,
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
#ifdef RIP_USE_CONNECTION_AUTODETECT
		                  INCOMING_AUTODETECT_FLAG,
#endif
		                  LOG_PROTOCOL,
		                  TAB_SELECTED_COLOR,
		                  TAB_SELECTED_BORDER_COLOR,
		                  TAB_OFFLINE_COLOR,
		                  TAB_ACTIVITY_COLOR,
		                  TAB_SELECTED_TEXT_COLOR, TAB_OFFLINE_TEXT_COLOR, TAB_ACTIVITY_TEXT_COLOR, // [+] SCALOlaz
		                  HUB_IN_FAV_BK_COLOR, HUB_IN_FAV_CONNECT_BK_COLOR, // [+] SCALOlaz
		                  CHAT_ANIM_SMILES,
		                  SMILE_SELECT_WND_ANIM_SMILES,
		                  USE_CUSTOM_MENU, // [+] SSA
		                  RSS_AUTO_REFRESH_TIME, // [+] SSA
		                  OPEN_RSS, // [+] SSA
		                  POPUP_NEW_RSSNEWS, // [+] SSA
		                  RSS_COLUMNS_SORT, // [+] SCALOlaz
		                  RSS_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  SEARCH_SPY_COLUMNS_SORT, // [+] SCALOlaz
		                  SEARCH_SPY_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  SEARCH_COLUMNS_SORT, // [+] SCALOlaz
		                  SEARCH_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  QUEUE_COLUMNS_SORT, // [+] SCALOlaz
		                  QUEUE_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  FINISHED_DL_COLUMNS_SORT, // [+] SCALOlaz
		                  FINISHED_DL_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  FINISHED_UL_COLUMNS_SORT, // [+] SCALOlaz
		                  FINISHED_UL_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  UPLOAD_QUEUE_COLUMNS_SORT, // [+] SCALOlaz
		                  UPLOAD_QUEUE_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  USERS_COLUMNS_SORT, // [+] SCALOlaz
		                  USERS_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  HUBS_PUBLIC_COLUMNS_SORT, // [+] SCALOlaz
		                  HUBS_PUBLIC_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  HUBS_FAVORITES_COLUMNS_SORT, // [+] SCALOlaz
		                  HUBS_FAVORITES_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  HUBS_RECENTS_COLUMNS_SORT, // [+] SCALOlaz
		                  HUBS_RECENTS_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  DIRLIST_COLUMNS_SORT, // [+] SCALOlaz
		                  DIRLIST_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  HUBFRAME_COLUMNS_SORT, // [+] SCALOlaz
		                  HUBFRAME_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  TRANSFERS_COLUMNS_SORT, // [+] SCALOlaz
		                  TRANSFERS_COLUMNS_SORT_ASC, // [+] SCALOlaz
		                  QUEUED,
		                  OVERLAP_CHUNKS,
		                  EXTRA_PARTIAL_SLOTS,
		                  AUTO_SLOTS,
#ifdef STRONG_USE_DHT
		                  USE_DHT,
		                  USE_DHT_NOTANSWER,    // [+] SCALOlaz: Answer for Use DHT
		                  DHT_PORT,
		                  UPDATE_IP_DHT, // [!] IRainman UPDATE_IP -> UPDATE_IP_DHT: Flylink contains two methods of IP update!
#endif
		                  KEEP_FINISHED_FILES_OPTION,
		                  ALLOW_NAT_TRAVERSAL, USE_EXPLORER_THEME, UC_SUBMENU, AUTO_DETECT_CONNECTION,
		                  // BETA_INFO, // [+] NightOrion [-]
#ifdef RIP_USE_PORTAL_BROWSER
		                  OPEN_PORTAL_BROWSER, //[+] BRAIN_RIPPER
#endif
		                  MIN_MULTI_CHUNK_SIZE, // [+] IRainman
		                  MIN_MEDIAINFO_SIZE, //[+]PPA
#ifdef FLYLINKDC_USE_XXX_ICON
		                  FLY_GENDER, //[+] PPA
#endif
		                  SHOW_SEEKERS_IN_SPY_FRAME,// [+] IRainman
		                  LOG_SEEKERS_IN_SPY_FRAME,
		                  FORMAT_BOT_MESSAGE,// [+] IRainman
#ifdef IRAINMAN_USE_BB_CODES
		                  FORMAT_BB_CODES, // [+] IRainman
#endif
		                  REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY, // [+] IRainman
		                  MULTILINE_CHAT_INPUT_BY_CTRL_ENTER, //[+] SSA
		                  SHOW_SEND_MESSAGE_BUTTON, //[+] SSA
		                  SHOW_BBCODE_PANEL, //[+] SSA
		                  SHOW_EMOTIONS_BTN, //[+] SSA
		                  SHOW_MULTI_CHAT_BTN, // [+] IRainman
		                  CHAT_REFFERING_TO_NICK, // [+] SCALOlaz
		                  USE_MAGNETS_IN_PLAYERS_SPAM, // [+] SSA
		                  USE_BITRATE_FIX_FOR_SPAM, // [+] SSA
		                  ON_DOWNLOAD_SETTING, //[+] SSA
		                  AUTOUPDATE_ENABLE,
		                  AUTOUPDATE_RUNONSTARTUP,
		                  AUTOUPDATE_STARTATTIME,
		                  AUTOUPDATE_SHOWUPDATEREADY,
		                  AUTOUPDATE_TIME,
		                  AUTOUPDATE_EXE,
		                  AUTOUPDATE_ANTIVIRUS_DB,
		                  AUTOUPDATE_UTILITIES,
		                  AUTOUPDATE_LANG,
		                  AUTOUPDATE_PORTALBROWSER,
		                  AUTOUPDATE_EMOPACKS,
		                  AUTOUPDATE_WEBSERVER,
		                  AUTOUPDATE_SOUNDS,
		                  AUTOUPDATE_ICONTHEMES,
		                  AUTOUPDATE_COLORTHEMES,
		                  AUTOUPDATE_DOCUMENTATION,
		                  AUTOUPDATE_UPDATE_CHATBOT,
		                  AUTOUPDATE_FORCE_RESTART,
		                  EXTRA_SLOT_BY_IP,
		                  DCLST_REGISTER, // [+] IRainman dclst support
		                  AUTOUPDATE_TO_BETA, // [+] SSA - update to beta
		                  AUTOUPDATE_USE_CUSTOM_URL, //[+] SSA
		                  DCLST_CREATE_IN_SAME_FOLDER, // [+] SSA
		                  DCLST_ASK, // [+] SSA
		                  DCLST_ACTION, // [+] SSA
		                  DCLST_INCLUDESELF, // [+] SSA
		                  CONNECT_TO_SUPPORT_HUB, // [+] SSA
		                  DISABLE_AUTOREMOVE_VIRUS_HUB,
		                  FILESHARE_INC_FILELIST, // [+] SSA
		                  FILESHARE_REINDEX_ON_START, // [+] SSA
		                  SQLITE_USE_JOURNAL_MEMORY, // [+] IRainman
		                  SQLITE_USE_EXCLUSIVE_LOCK_MODE, // [+] IRAINMAN_SQLITE_USE_EXCLUSIVE_LOCK_MODE
		                  SECURITY_ASK_ON_SHARE_FROM_SHELL, // [+] SSA
		                  POPUP_NEW_FOLDERSHARE, // [+] SSA
		                  MAX_FINISHED_UPLOADS, MAX_FINISHED_DOWNLOADS, // [+] IRainman
		                  DB_LOG_FINISHED_UPLOADS, DB_LOG_FINISHED_DOWNLOADS,
		                  INT_PREVIEW_SERVER_PORT, INT_PREVIEW_SERVER_SPEED, // [+] SSA
		                  INT_PREVIEW_USE_VIDEO_SCROLL, INT_PREVIEW_START_CLIENT,  // [+] SSA
		                  PROVIDER_USE_RESOURCES, // [+] SSA
		                  PROVIDER_USE_MENU, PROVIDER_USE_HUBLIST, PROVIDER_USE_PROVIDER_LOCATIONS, // [+] SSA
		                  AUTOUPDATE_GEOIP, // [+] IRainman
		                  AUTOUPDATE_CUSTOMLOCATION, // [+] IRainman
#ifdef SSA_SHELL_INTEGRATION
		                  AUTOUPDATE_SHELL_EXT, // [+] IRainman
#endif
#ifdef IRAINMAN_USE_BB_CODES
		                  FORMAT_BB_CODES_COLORS, // [+] SSA
#endif
#ifdef NIGHTORION_USE_STATISTICS_REQUEST
		                  SETTINGS_STATISTICS_ASK,
#endif
		                  USE_FLY_SERVER_STATICTICS_SEND,
		                  REPORT_TO_USER_IF_OUTDATED_OS_DETECTED, // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=1032
		                  USE_GPU_IN_TTH_COMPUTING,
		                  TTH_GPU_DEV_NUM,
		                  INT_LAST,
		                  SETTINGS_LAST = INT_LAST
		                };
		                
		// [!] IRainman don't uncoment this code, FlylinkDC++ save all statistics on database!
		//enum Int64Setting { INT64_FIRST = INT_LAST + 1,   TOTAL_UPLOAD = INT64_FIRST, TOTAL_DOWNLOAD, INT64_LAST, SETTINGS_LAST = INT64_LAST };
		
		enum {  INCOMING_DIRECT, INCOMING_FIREWALL_UPNP, INCOMING_FIREWALL_NAT,
		        INCOMING_FIREWALL_PASSIVE
		     };
		enum {  OUTGOING_DIRECT, OUTGOING_SOCKS5 };
		
		//enum {    SPEED_64, SPEED_128, SPEED_150, SPEED_192,
		//      SPEED_256, SPEED_384, SPEED_400, SPEED_512, SPEED_600, SPEED_768, SPEED_1M, SPEED_15M,
		//      SPEED_2M, SPEED_4M, SPEED_6M, SPEED_8M, SPEED_10M, SP_LAST
		//   };
		
		enum { SIZE_64, SIZE_128, SIZE_256, SIZE_512, SIZE_1024, SIZE_LAST };
		
		enum { MAGNET_AUTO_SEARCH, MAGNET_AUTO_DOWNLOAD, MAGNET_AUTO_DOWNLOAD_AND_OPEN };
//[+] SSA
		enum { ON_DOWNLOAD_ASK, /*FlylinkDC Team TODO ON_DOWNLOAD_EXIST_FILE_TO_NEW_DEST,*/  ON_DOWNLOAD_REPLACE, ON_DOWNLOAD_RENAME, ON_DOWNLOAD_SKIP };
		
		enum { POS_LEFT, POS_RIGHT };// [+] IRainman
		
		enum { TABS_TOP, TABS_BOTTOM, TABS_LEFT, TABS_RIGHT };// [+] IRainman
		
		enum PlayerSelected // [!] IRainman move from
		{
			WinAmp,
			WinMediaPlayer,
			iTunes,
			WinMediaPlayerClassic,
			JetAudio,
			QCDQMP,
			PlayersCount,
			UnknownPlayer = PlayersCount
		};
		
		static const string& get(StrSetting key, const bool useDefault = true);
		static int get(IntSetting key, const bool useDefault = true);
		static bool getBool(IntSetting key, const bool useDefault = true) // [!] IRainman opt.
		{
			return get(key, useDefault) != 0;
		}
		// [!] IRainman all set function return status: true is value automatically corrected, or false if not.
		static bool set(StrSetting key, const string& value);
		static bool set(IntSetting key, int value);
		static bool set(IntSetting key, const std::string& value);
		static bool set(IntSetting key, bool value)
		{
			return set(key, int(value));
		}
		
		static void setDefault(StrSetting key, const string& value) // [!] IRainman opt.
		{
			strDefaults[key - STR_FIRST] = value;
		}
		
		static void setDefault(IntSetting key, int value) // [!] IRainman opt.
		{
			intDefaults[key - INT_FIRST] = value;
		}
		static void setDefault(IntSetting key, const string& value) // [+] IRainman fix.
		{
			intDefaults[key - INT_FIRST] = Util::toInt(value);
		}
		static bool isDefault(int aSet) // [!] IRainman opt.
		{
			return !isSet[aSet];
		}
		static bool LoadLanguage();
		// [+] IRainman fix.
		static void importDctheme(const tstring& file, const bool asDefault = false);
		static void exportDctheme(const tstring& file);
		// [~] IRainman fix.
		static void unset(size_t key) // [!] IRainman opt.
		{
			isSet[key] = false;
		}
		void load()
		{
			Util::migrate(getConfigFile());
			load(getConfigFile());
		}
		void save()
		{
			save(getConfigFile());
		}
		
		void load(const string& aFileName);
		void save(const string& aFileName);
		void setDefaults(); // !SMT!-S
		void loadOtherSettings();
		static void generateNewTCPPort();
		static void generateNewUDPPort();
		
		// Search types
		static void validateSearchTypeName(const string& name);
		
		void setSearchTypeDefaults();
		void addSearchType(const string& name, const StringList& extensions, bool validated = false);
		void delSearchType(const string& name);
		void renameSearchType(const string& oldName, const string& newName);
		void modSearchType(const string& name, const StringList& extensions);
		
		static const SearchTypes& getSearchTypes()
		{
			return g_searchTypes;
		}
		static const StringList& getExtensions(const string& name); // [!] IRainman opt.
		
		static unsigned short getNewPortValue(unsigned short p_OldPortValue); // [+] IRainman
		static string getSoundFilename(const SettingsManager::StrSetting p_sound); // [+] IRainman fix.
		static bool getBeepEnabled(const SettingsManager::IntSetting p_sound); // [+] IRainman fix.
		static bool getPopupEnabled(const SettingsManager::IntSetting p_popup); // [+] IRainman fix.
	private:
		friend class Singleton<SettingsManager>;
		SettingsManager();
		~SettingsManager() { }
		
		// [!] IRainman opt: use this data as static.
		static const string g_settingTags[];
		
		static string strSettings[STR_LAST - STR_FIRST];
		static int    intSettings[INT_LAST - INT_FIRST];
		//static int64_t int64Settings[INT64_LAST - INT64_FIRST];// [!] IRainman don't uncoment this code, FlylinkDC++ save all statistics on database!
		static string strDefaults[STR_LAST - STR_FIRST];
		static int    intDefaults[INT_LAST - INT_FIRST];
		//static int64_t int64Defaults[INT64_LAST - INT64_FIRST];// [!] IRainman don't uncoment this code, FlylinkDC++ save all statistics on database!
		static bool isSet[SETTINGS_LAST];
		// Search types
		static SearchTypes g_searchTypes; // name, extlist
		
		static SearchTypesIter getSearchType(const string& name);
		// [!] IRainman opt: use this data as static.
		
		static string getConfigFile()
		{
			return Util::getConfigPath() + "DCPlusPlus.xml";
		}
};

// Shorthand accessor macros
#define SETTING(k) SettingsManager::get(SettingsManager::k, true)
#define BOOLSETTING(k) SettingsManager::getBool(SettingsManager::k, true)

// [+] IRainman: copy-past fix.
#define SET_SETTING(k, v) SettingsManager::set(SettingsManager::k, v)
#define SOUND_SETTING(k) SettingsManager::getSoundFilename(SettingsManager::k)
#define SOUND_BEEP_BOOLSETTING(k) SettingsManager::getBeepEnabled(SettingsManager::k)
#define POPUP_ENABLED(k) SettingsManager::getPopupEnabled(SettingsManager::k)

#define SPLIT_SETTING_AND_LOWER(key) Util::splitSettingAndLower(SETTING(key))
// [~] IRainman: copy-past fix.

#endif // !defined(SETTINGS_MANAGER_H)

/**
 * @file
 * $Id: SettingsManager.h 575 2011-08-25 19:38:04Z bigmuscle $
 */
