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

#include "stdafx.h"

#include "Resource.h"

#define COMPILE_MULTIMON_STUBS 1
#include <MultiMon.h>
#include <powrprof.h>
#include <control.h>
#include <strmif.h> // error with missing ddraw.h, get it from MS DirectX SDK
#include <iphlpapi.h>

#include "WinUtil.h"
#include "MainFrm.h"
#include "LimitEditDlg.h"

#ifdef RIP_USE_PORTAL_BROWSER
#include "PortalBrowser.h"
#endif

#include "../client/DownloadManager.h"
#include "MagnetDlg.h"
// AirDC++
#include "winamp.h"
#include "WMPlayerRemoteApi.h"
#include "iTunesCOMInterface.h"
#include "QCDCtrlMsgs.h"
#include "Players.h"
// AirDC++
#include "BarShader.h"
#ifdef FLYLINKDC_USE_CUSTOM_MENU
#include "../FlyFeatures/CustomMenuManager.h"
#endif
#include "DirectoryListingFrm.h"
#include "HTMLColors.h"
#include "SearchFrm.h"

#pragma comment(lib, "iphlpapi.lib")

string UserInfoGuiTraits::g_hubHint;
UserPtr UserInfoBaseHandlerTraitsUser<UserPtr>::g_user = nullptr;
OnlineUserPtr UserInfoBaseHandlerTraitsUser<OnlineUserPtr>::g_user = nullptr;


const TCHAR* g_file_list_type = L"All Lists\0*.xml.bz2;*.dcls;*.dclst;*.torrent\0Torrent files\0*.torrent\0FileLists\0*.xml.bz2\0DCLST metafiles\0*.dcls;*.dclst\0All Files\0*.*\0\0";

FileImage g_fileImage;
UserImage g_userImage;
UserStateImage g_userStateImage;
TrackerImage g_trackerImage;
GenderImage g_genderImage;
FlagImage g_flagImage;
ISPImage  g_ISPImage;
TransferTreeImage g_TransferTreeImage;
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
VideoImage g_videoImage;
#endif

HBRUSH Colors::g_bgBrush = nullptr;
COLORREF Colors::g_textColor = 0;
COLORREF Colors::g_bgColor = 0;

HFONT Fonts::g_font = nullptr;
int Fonts::g_fontHeight = 0;
int Fonts::g_fontHeightPixl = 0;
HFONT Fonts::g_boldFont = nullptr;
HFONT Fonts::g_systemFont = nullptr;
HFONT Fonts::g_halfFont = nullptr;

CMenu WinUtil::g_mainMenu;

OMenu WinUtil::g_copyHubMenu;
OMenu UserInfoGuiTraits::copyUserMenu;
OMenu UserInfoGuiTraits::grantMenu;
OMenu UserInfoGuiTraits::speedMenu;
OMenu UserInfoGuiTraits::userSummaryMenu;
OMenu UserInfoGuiTraits::privateMenu;

OMenu Preview::g_previewMenu;
int Preview::g_previewAppsSize = 0;
dcdrun(bool Preview::_debugIsActivated = false;)
dcdrun(bool Preview::_debugIsClean = true;)

HIconWrapper WinUtil::g_banIconOnline(IDR_BANNED_ONLINE);
HIconWrapper WinUtil::g_banIconOffline(IDR_BANNED_OFF);
HIconWrapper WinUtil::g_hMedicalIcon(IDR_ICON_MEDICAL_BAG);
//HIconWrapper WinUtil::g_hCrutchIcon(IDR_ICON_CRUTCH);
HIconWrapper WinUtil::g_hFirewallIcon(IDR_ICON_FIREWALL);
HIconWrapper WinUtil::g_hXXXBlockIcon(IDR_ICON_XXX_BLOCK); // XXX
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
HIconWrapper WinUtil::g_hClockIcon(IDR_ICON_CLOCK);
#endif

std::unique_ptr<HIconWrapper> WinUtil::g_HubOnIcon;
std::unique_ptr<HIconWrapper> WinUtil::g_HubOffIcon;
std::unique_ptr<HIconWrapper> WinUtil::g_HubFlylinkDCIcon;
std::unique_ptr<HIconWrapper> WinUtil::g_HubFlylinkDCIconVIP[22]; // VIP_ICON
std::unique_ptr<HIconWrapper> WinUtil::g_HubDDoSIcon;
HIconWrapper WinUtil::g_hThermometerIcon(IDR_ICON_THERMOMETR_BAG);
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
std::unique_ptr<HIconWrapper> WinUtil::g_HubAntivirusIcon;
#endif

#ifdef FLYLINKDC_USE_SKULL_TAB
std::unique_ptr<HIconWrapper> WinUtil::g_HubVirusIcon[4];
#else
std::unique_ptr<HIconWrapper> WinUtil::g_HubVirusIcon;
#endif

TStringList LastDir::g_dirs;
HWND WinUtil::g_mainWnd = nullptr;
HWND WinUtil::g_mdiClient = nullptr;
FlatTabCtrl* WinUtil::g_tabCtrl = nullptr;
HHOOK WinUtil::g_hook = nullptr;
bool WinUtil::urlDcADCRegistered = false;
bool WinUtil::urlMagnetRegistered = false;
bool WinUtil::DclstRegistered = false;
bool WinUtil::g_isAppActive = false;
CHARFORMAT2 Colors::g_TextStyleTimestamp;
CHARFORMAT2 Colors::g_ChatTextGeneral;
CHARFORMAT2 Colors::g_ChatTextOldHistory;
CHARFORMAT2 Colors::g_TextStyleMyNick;
CHARFORMAT2 Colors::g_ChatTextMyOwn;
CHARFORMAT2 Colors::g_ChatTextServer;
CHARFORMAT2 Colors::g_ChatTextSystem;
CHARFORMAT2 Colors::g_TextStyleBold;
CHARFORMAT2 Colors::g_TextStyleFavUsers;
CHARFORMAT2 Colors::g_TextStyleFavUsersBan;
CHARFORMAT2 Colors::g_TextStyleOPs;
CHARFORMAT2 Colors::g_TextStyleURL;
CHARFORMAT2 Colors::g_ChatTextPrivate;
CHARFORMAT2 Colors::g_ChatTextLog;

int WinUtil::g_tabPos = SettingsManager::TABS_TOP;

const toolbarButton g_ToolbarButtons[] =
{
	{ID_FILE_CONNECT, 0, true, ResourceManager::MENU_PUBLIC_HUBS},
	{ID_FILE_RECONNECT, 1, false, ResourceManager::MENU_RECONNECT},
	{IDC_FOLLOW, 2, false, ResourceManager::MENU_FOLLOW_REDIRECT},
	{IDC_FAVORITES, 3, true, ResourceManager::MENU_FAVORITE_HUBS},
	{IDC_FAVUSERS, 4, true, ResourceManager::MENU_FAVORITE_USERS},
	{IDC_RECENTS, 5, true, ResourceManager::MENU_FILE_RECENT_HUBS},
	{IDC_QUEUE, 6, true, ResourceManager::MENU_DOWNLOAD_QUEUE},
	{IDC_FINISHED, 7, true, ResourceManager::MENU_FINISHED_DOWNLOADS},
	{IDC_UPLOAD_QUEUE, 8, true, ResourceManager::MENU_WAITING_USERS},
	{IDC_FINISHED_UL, 9, true, ResourceManager::MENU_FINISHED_UPLOADS},
	{ID_FILE_SEARCH, 10, false, ResourceManager::MENU_SEARCH},
	{IDC_FILE_ADL_SEARCH, 11, true, ResourceManager::MENU_ADL_SEARCH},
	{IDC_SEARCH_SPY, 12, true, ResourceManager::MENU_SEARCH_SPY},
	{IDC_NET_STATS, 13, true, ResourceManager::NETWORK_STATISTICS},
	{IDC_OPEN_MY_LIST, 14, false, ResourceManager::MENU_OPEN_OWN_LIST},
	{ID_FILE_SETTINGS, 15, false, ResourceManager::MENU_SETTINGS},
	{IDC_NOTEPAD, 16, true, ResourceManager::MENU_NOTEPAD},
	{IDC_AWAY, 17, true, ResourceManager::AWAY},
	{IDC_SHUTDOWN, 18, true, ResourceManager::SHUTDOWN},
	{IDC_LIMITER, 19, true, ResourceManager::SETCZDC_ENABLE_LIMITING},
	{IDC_UPDATE_FLYLINKDC, 20, false, ResourceManager::UPDATE_CHECK},
	{IDC_DISABLE_SOUNDS, 21, true, ResourceManager::DISABLE_SOUNDS},
	{IDC_OPEN_DOWNLOADS, 22, false, ResourceManager::MENU_OPEN_DOWNLOADS_DIR},
	{IDC_REFRESH_FILE_LIST, 23, false, ResourceManager::MENU_REFRESH_FILE_LIST},
	{ID_TOGGLE_TOOLBAR, 24, true, ResourceManager::TOGGLE_TOOLBAR},
	{ID_FILE_QUICK_CONNECT, 25, false, ResourceManager::MENU_QUICK_CONNECT},
	{IDC_OPEN_FILE_LIST, 26, false, ResourceManager::MENU_OPEN_FILE_LIST},
	{IDC_RECONNECT_DISCONNECTED, 27, false, ResourceManager::MENU_RECONNECT_DISCONNECTED},
	{IDC_RSS, 28, true, ResourceManager::MENU_RSS_NEWS},
	{IDC_DISABLE_POPUPS, 29, true, ResourceManager::DISABLE_POPUPS},
	{0, 0, false, ResourceManager::MENU_NOTEPAD}
};

const toolbarButton g_WinampToolbarButtons[] =
{
	{IDC_WINAMP_SPAM, 0, false, ResourceManager::WINAMP_SPAM},
	{IDC_WINAMP_BACK, 1, false, ResourceManager::WINAMP_BACK},
	{IDC_WINAMP_PLAY, 2, false, ResourceManager::WINAMP_PLAY},
	{IDC_WINAMP_PAUSE, 3, false, ResourceManager::WINAMP_PAUSE},
	{IDC_WINAMP_NEXT, 4, false, ResourceManager::WINAMP_NEXT},
	{IDC_WINAMP_STOP, 5, false, ResourceManager::WINAMP_STOP},
	{IDC_WINAMP_VOL_DOWN, 6, false, ResourceManager::WINAMP_VOL_DOWN},
	{IDC_WINAMP_VOL_HALF, 7, false, ResourceManager::WINAMP_VOL_HALF},
	{IDC_WINAMP_VOL_UP, 8, false, ResourceManager::WINAMP_VOL_UP},
	{0, 0, false, ResourceManager::WINAMP_PLAY}
};

// Images ids MUST be synchronized with icons number in Toolbar-mini.png.
// This is second path, first is described in ToolbarButtons.
// So in this array images id continues after last image in ToolbarButtons array.
const menuImage g_MenuImages[] =
{
	{IDC_CDMDEBUG_WINDOW,    30},
	{ID_WINDOW_CASCADE,      31},
	{ID_WINDOW_TILE_HORZ,    32},
	{ID_WINDOW_TILE_VERT,    33},
	{ID_WINDOW_MINIMIZE_ALL, 34},
	{ID_WINDOW_RESTORE_ALL,  35},
	{ID_GET_TTH,             36},
	{IDC_MATCH_ALL,          37},
	{ID_APP_EXIT,            38},
	{IDC_HASH_PROGRESS,      39},
	{ID_WINDOW_ARRANGE,      40},
	{IDC_HELP_HELP,          41},
	// TODO {IDC_HELP_DONATE,        XX},
	{IDC_HELP_HOMEPAGE,      42},
	{IDC_HELP_DISCUSS,       43},
	{IDC_SITES_FLYLINK_TRAC, 44},
	{ID_APP_ABOUT,           45},
	{IDC_TOPMOST,            46},
	{IDC_ADD_MAGNET,         47},
	{ID_VIEW_TOOLBAR,        48},
	{ID_VIEW_TRANSFER_VIEW,  49},
	{0,  0}
};

const int g_cout_of_ToolbarButtons = _countof(g_ToolbarButtons);
const int g_cout_of_WinampToolbarButtons = _countof(g_WinampToolbarButtons);


static const char* countryNames[] = { "ANDORRA", "UNITED ARAB EMIRATES", "AFGHANISTAN", "ANTIGUA AND BARBUDA",
                                      "ANGUILLA", "ALBANIA", "ARMENIA", "NETHERLANDS ANTILLES", "ANGOLA", "ANTARCTICA", "ARGENTINA", "AMERICAN SAMOA",
                                      "AUSTRIA", "AUSTRALIA", "ARUBA", "ALAND", "AZERBAIJAN", "BOSNIA AND HERZEGOVINA", "BARBADOS", "BANGLADESH",
                                      "BELGIUM", "BURKINA FASO", "BULGARIA", "BAHRAIN", "BURUNDI", "BENIN", "BERMUDA", "BRUNEI DARUSSALAM", "BOLIVIA",
                                      "BRAZIL", "BAHAMAS", "BHUTAN", "BOUVET ISLAND", "BOTSWANA", "BELARUS", "BELIZE", "CANADA", "COCOS ISLANDS",
                                      "THE DEMOCRATIC REPUBLIC OF THE CONGO", "CENTRAL AFRICAN REPUBLIC", "CONGO", "SWITZERLAND", "COTE D'IVOIRE", "COOK ISLANDS",
                                      "CHILE", "CAMEROON", "CHINA", "COLOMBIA", "COSTA RICA", "SERBIA AND MONTENEGRO", "CUBA", "CAPE VERDE",
                                      "CHRISTMAS ISLAND", "CYPRUS", "CZECH REPUBLIC", "GERMANY", "DJIBOUTI", "DENMARK", "DOMINICA", "DOMINICAN REPUBLIC",
                                      "ALGERIA", "ECUADOR", "ESTONIA", "EGYPT", "WESTERN SAHARA", "ERITREA", "SPAIN", "ETHIOPIA", "EUROPE", "FINLAND", "FIJI",
                                      "FALKLAND ISLANDS", "MICRONESIA", "FAROE ISLANDS", "FRANCE", "GABON", "UNITED KINGDOM", "GRENADA", "GEORGIA",
                                      "FRENCH GUIANA", "GUERNSEY", "GHANA", "GIBRALTAR", "GREENLAND", "GAMBIA", "GUINEA", "GUADELOUPE", "EQUATORIAL GUINEA",
                                      "GREECE", "SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS", "GUATEMALA", "GUAM", "GUINEA-BISSAU", "GUYANA",
                                      "HONG KONG", "HEARD ISLAND AND MCDONALD ISLANDS", "HONDURAS", "CROATIA", "HAITI", "HUNGARY",
                                      "INDONESIA", "IRELAND", "ISRAEL",  "ISLE OF MAN", "INDIA", "BRITISH INDIAN OCEAN TERRITORY", "IRAQ", "IRAN", "ICELAND",
                                      "ITALY", "JERSEY", "JAMAICA", "JORDAN", "JAPAN", "KENYA", "KYRGYZSTAN", "CAMBODIA", "KIRIBATI", "COMOROS",
                                      "SAINT KITTS AND NEVIS", "DEMOCRATIC PEOPLE'S REPUBLIC OF KOREA", "SOUTH KOREA", "KUWAIT", "CAYMAN ISLANDS",
                                      "KAZAKHSTAN", "LAO PEOPLE'S DEMOCRATIC REPUBLIC", "LEBANON", "SAINT LUCIA", "LIECHTENSTEIN", "SRI LANKA",
                                      "LIBERIA", "LESOTHO", "LITHUANIA", "LUXEMBOURG", "LATVIA", "LIBYAN ARAB JAMAHIRIYA", "MOROCCO", "MONACO",
                                      "MOLDOVA", "MONTENEGRO", "MADAGASCAR", "MARSHALL ISLANDS", "MACEDONIA", "MALI", "MYANMAR", "MONGOLIA", "MACAO",
                                      "NORTHERN MARIANA ISLANDS", "MARTINIQUE", "MAURITANIA", "MONTSERRAT", "MALTA", "MAURITIUS", "MALDIVES",
                                      "MALAWI", "MEXICO", "MALAYSIA", "MOZAMBIQUE", "NAMIBIA", "NEW CALEDONIA", "NIGER", "NORFOLK ISLAND",
                                      "NIGERIA", "NICARAGUA", "NETHERLANDS", "NORWAY", "NEPAL", "NAURU", "NIUE", "NEW ZEALAND", "OMAN", "PANAMA",
                                      "PERU", "FRENCH POLYNESIA", "PAPUA NEW GUINEA", "PHILIPPINES", "PAKISTAN", "POLAND", "SAINT PIERRE AND MIQUELON",
                                      "PITCAIRN", "PUERTO RICO", "PALESTINIAN TERRITORY", "PORTUGAL", "PALAU", "PARAGUAY", "QATAR", "REUNION",
                                      "ROMANIA", "SERBIA", "RUSSIAN FEDERATION", "RWANDA", "SAUDI ARABIA", "SOLOMON ISLANDS", "SEYCHELLES", "SUDAN",
                                      "SWEDEN", "SINGAPORE", "SAINT HELENA", "SLOVENIA", "SVALBARD AND JAN MAYEN", "SLOVAKIA", "SIERRA LEONE",
                                      "SAN MARINO", "SENEGAL", "SOMALIA", "SURINAME", "SAO TOME AND PRINCIPE", "EL SALVADOR", "SYRIAN ARAB REPUBLIC",
                                      "SWAZILAND", "TURKS AND CAICOS ISLANDS", "CHAD", "FRENCH SOUTHERN TERRITORIES", "TOGO", "THAILAND", "TAJIKISTAN",
                                      "TOKELAU", "TIMOR-LESTE", "TURKMENISTAN", "TUNISIA", "TONGA", "TURKEY", "TRINIDAD AND TOBAGO", "TUVALU", "TAIWAN",
                                      "TANZANIA", "UKRAINE", "UGANDA", "UNITED STATES MINOR OUTLYING ISLANDS", "UNITED STATES", "URUGUAY", "UZBEKISTAN",
                                      "VATICAN", "SAINT VINCENT AND THE GRENADINES", "VENEZUELA", "BRITISH VIRGIN ISLANDS", "U.S. VIRGIN ISLANDS",
                                      "VIET NAM", "VANUATU", "WALLIS AND FUTUNA", "SAMOA", "YEMEN", "MAYOTTE", "YUGOSLAVIA", "SOUTH AFRICA", "ZAMBIA",
                                      "ZIMBABWE"
                                    };

// https://drdump.com/DumpGroup.aspx?DumpGroupID=303960

HLSCOLOR RGB2HLS(COLORREF rgb)
{
	const unsigned char minval = std::min(GetRValue(rgb), std::min(GetGValue(rgb), GetBValue(rgb)));
	const unsigned char maxval = std::max(GetRValue(rgb), std::max(GetGValue(rgb), GetBValue(rgb)));
	const float mdiff  = float(maxval) - float(minval);
	const float msum   = float(maxval) + float(minval);
	
	const float luminance = msum / 510.0f;
	float saturation = 0.0f;
	float hue = 0.0f;
	
	if (maxval != minval)
	{
		float rnorm = (maxval - GetRValue(rgb)) / mdiff;
		float gnorm = (maxval - GetGValue(rgb)) / mdiff;
		float bnorm = (maxval - GetBValue(rgb)) / mdiff;
		
		saturation = (luminance <= 0.5f) ? (mdiff / msum) : (mdiff / (510.0f - msum));
		
		if (GetRValue(rgb) == maxval) hue = 60.0f * (6.0f + bnorm - gnorm);
		if (GetGValue(rgb) == maxval) hue = 60.0f * (2.0f + rnorm - bnorm);
		if (GetBValue(rgb) == maxval) hue = 60.0f * (4.0f + gnorm - rnorm);
		if (hue > 360.0f) hue = hue - 360.0f;
	}
	return HLS((hue * 255) / 360, luminance * 255, saturation * 255);
}

static BYTE _ToRGB(float rm1, float rm2, float rh)
{
	if (rh > 360.0f) rh -= 360.0f;
	else if (rh <   0.0f) rh += 360.0f;
	
	if (rh <  60.0f) rm1 = rm1 + (rm2 - rm1) * rh / 60.0f;
	else if (rh < 180.0f) rm1 = rm2;
	else if (rh < 240.0f) rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;
	
	return (BYTE)(rm1 * 255);
}

COLORREF HLS2RGB(HLSCOLOR hls)
{
	float hue        = ((int)HLS_H(hls) * 360) / 255.0f;
	float luminance  = HLS_L(hls) / 255.0f;
	float saturation = HLS_S(hls) / 255.0f;
	
	if (EqualD(saturation, 0))
	{
		return RGB(HLS_L(hls), HLS_L(hls), HLS_L(hls));
	}
	float rm1, rm2;
	
	if (luminance <= 0.5f) rm2 = luminance + luminance * saturation;
	else                     rm2 = luminance + saturation - luminance * saturation;
	rm1 = 2.0f * luminance - rm2;
	BYTE red   = _ToRGB(rm1, rm2, hue + 120.0f);
	BYTE green = _ToRGB(rm1, rm2, hue);
	BYTE blue  = _ToRGB(rm1, rm2, hue - 120.0f);
	
	return RGB(red, green, blue);
}

COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S)
{
	HLSCOLOR hls = RGB2HLS(rgb);
	BYTE h = HLS_H(hls);
	BYTE l = HLS_L(hls);
	BYTE s = HLS_S(hls);
	
	if (percent_L > 0)
	{
		l = BYTE(l + ((255 - l) * percent_L) / 100);
	}
	else if (percent_L < 0)
	{
		l = BYTE((l * (100 + percent_L)) / 100);
	}
	if (percent_S > 0)
	{
		s = BYTE(s + ((255 - s) * percent_S) / 100);
	}
	else if (percent_S < 0)
	{
		s = BYTE((s * (100 + percent_S)) / 100);
	}
	return HLS2RGB(HLS(h, l, s));
}
// TODO #pragma optimize("", on)


void Colors::getUserColor(bool p_is_op, const UserPtr& user, COLORREF &fg, COLORREF &bg, unsigned short& p_flag_mask, const OnlineUserPtr& onlineUser)
{
	bool l_is_favorites = false;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	if (SETTING(ENABLE_AUTO_BAN))
	{
		if ((p_flag_mask & IS_AUTOBAN) == IS_AUTOBAN)
		{
			if (onlineUser && user->hasAutoBan(&onlineUser->getClient(), l_is_favorites) != User::BAN_NONE)
				p_flag_mask = (p_flag_mask & ~IS_AUTOBAN) | IS_AUTOBAN_ON;
			else
				p_flag_mask = (p_flag_mask & ~IS_AUTOBAN);
			if (l_is_favorites)
				p_flag_mask = (p_flag_mask & ~IS_FAVORITE) | IS_FAVORITE_ON;
			else
				p_flag_mask = (p_flag_mask & ~IS_FAVORITE);
		}
		if (p_flag_mask & IS_AUTOBAN)
		{
			bg = SETTING(BAN_COLOR);
		}
	}
#endif // IRAINMAN_ENABLE_AUTO_BAN
#ifdef FLYLINKDC_USE_DETECT_CHEATING
	if (p_is_op && onlineUser) // Возможно фикс https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=38000
	{
	
		const auto fc = onlineUser->getIdentity().getFakeCard();
		if (fc & Identity::BAD_CLIENT)
		{
			fg = SETTING(BAD_CLIENT_COLOUR);
			return;
		}
		else if (fc & Identity::BAD_LIST)
		{
			fg = SETTING(BAD_FILELIST_COLOUR);
			return;
		}
		else if (fc & Identity::CHECKED && BOOLSETTING(SHOW_SHARE_CHECKED_USERS))
		{
			fg = SETTING(FULL_CHECKED_COLOUR);
			return;
		}
	}
#endif // FLYLINKDC_USE_DETECT_CHEATING
#ifdef _DEBUG
	//LogManager::message("Colors::getUserColor, user = " + user->getLastNick() + " color = " + Util::toString(fg));
#endif
	dcassert(user);
	if ((p_flag_mask & IS_IGNORED_USER) == IS_IGNORED_USER)
	{
		if (UserManager::g_isEmptyIgnoreList == false && UserManager::isInIgnoreList(onlineUser ? onlineUser->getIdentity().getNick() : user->getLastNick()))
			p_flag_mask = (p_flag_mask & ~IS_IGNORED_USER) | IS_IGNORED_USER_ON;
		else
			p_flag_mask = (p_flag_mask & ~IS_IGNORED_USER);
	}
	if ((p_flag_mask & IS_RESERVED_SLOT) == IS_RESERVED_SLOT)
	{
		if (UploadManager::getReservedSlotTime(user))
			p_flag_mask = (p_flag_mask & ~IS_RESERVED_SLOT) | IS_RESERVED_SLOT_ON;
		else
			p_flag_mask = (p_flag_mask & ~IS_RESERVED_SLOT);
	}
	if ((p_flag_mask & IS_FAVORITE) == IS_FAVORITE)
	{
		bool l_is_ban = false;
		l_is_favorites = FavoriteManager::isFavoriteUser(user, l_is_ban);
		if (l_is_favorites)
			p_flag_mask = (p_flag_mask & ~IS_FAVORITE) | IS_FAVORITE_ON;
		else
			p_flag_mask = (p_flag_mask & ~IS_FAVORITE);
		if (l_is_ban)
			p_flag_mask = (p_flag_mask & ~IS_BAN) | IS_BAN_ON;
		else
			p_flag_mask = (p_flag_mask & ~IS_BAN);
	}
	if (p_flag_mask & IS_FAVORITE)
	{
		if (p_flag_mask & IS_BAN)
			fg = SETTING(TEXT_ENEMY_FORE_COLOR);
		else
			fg = SETTING(FAVORITE_COLOR);
	}
	else if (onlineUser && onlineUser->getIdentity().isOp())
	{
		fg = SETTING(OP_COLOR);
	}
	else if (p_flag_mask & IS_RESERVED_SLOT)
	{
		fg = SETTING(RESERVED_SLOT_COLOR);
	}
	else if (p_flag_mask & IS_IGNORED_USER)
	{
		fg = SETTING(IGNORED_COLOR);
	}
	else if (user->isSet(User::FIREBALL))
	{
		fg = SETTING(FIREBALL_COLOR);
	}
	else if (user->isSet(User::SERVER))
	{
		fg = SETTING(SERVER_COLOR);
	}
	else if (onlineUser && !onlineUser->getIdentity().isTcpActive())
	{
		fg = SETTING(PASIVE_COLOR);
	}
	else
	{
		fg = SETTING(NORMAL_COLOUR);
	}
}

void WinUtil::initThemeIcons()
{
	g_HubOnIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_HUB));
	g_HubOffIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_HUB_OFF));
	g_HubFlylinkDCIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_MAINFRAME));
	g_HubFlylinkDCIconVIP[0] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_SCALOLAZ));
	g_HubFlylinkDCIconVIP[1] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_MILENAHUB));
	//g_HubFlylinkDCIconVIP[1] = std::unique_ptr<HIconWrapper>(new HIconWrapper());
	g_HubFlylinkDCIconVIP[2] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_STEALTHHUB));
	g_HubFlylinkDCIconVIP[3] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_KEEPCLEAR));
	g_HubFlylinkDCIconVIP[4] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_PRIME));
	g_HubFlylinkDCIconVIP[5] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_PERSEUS));
	g_HubFlylinkDCIconVIP[6] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_ALLAVTOVO));
	g_HubFlylinkDCIconVIP[7] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_KOROBKA)); // VIP_ICON
	g_HubFlylinkDCIconVIP[8] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_NSK154));
	g_HubFlylinkDCIconVIP[9] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_PROSTOIGRA));
	g_HubFlylinkDCIconVIP[10] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_EVAHUB));
	g_HubFlylinkDCIconVIP[11] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_TITANKALUGA));
	g_HubFlylinkDCIconVIP[12] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_MYDC));
	g_HubFlylinkDCIconVIP[13] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_ADRENALIN));
	g_HubFlylinkDCIconVIP[14] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_GODC));
	g_HubFlylinkDCIconVIP[15] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ZHIGANDC));
	g_HubFlylinkDCIconVIP[16] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_HMN_PP));
	g_HubFlylinkDCIconVIP[17] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_MILLENIUM));
	g_HubFlylinkDCIconVIP[18] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_ICO_ALLAVTOVO)); // piter
	g_HubFlylinkDCIconVIP[19] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_KCAHDER));
	g_HubFlylinkDCIconVIP[20] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_OZERKI));
	g_HubFlylinkDCIconVIP[21] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_VIP_SWALKA));
	
	
	g_HubDDoSIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_ICON_MEDICAL_BAG));
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	g_HubAntivirusIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_ICON_THERMOMETR_BAG));
#endif
	
#ifdef FLYLINKDC_USE_SKULL_TAB
	g_HubVirusIcon[0] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_SKULL_ICO));
	g_HubVirusIcon[1] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_SKULL_RED_ICO));
	g_HubVirusIcon[2] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_SKULL2_ICO));
	g_HubVirusIcon[3] = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_SKULL2_RED_ICO));
#else
	g_HubVirusIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_SKULL2_ICO));
#endif
}


dcdrun(bool WinUtil::g_staticMenuUnlinked = true;)
void WinUtil::unlinkStaticMenus(CMenu &menu)
{
	dcdrun(g_staticMenuUnlinked = true;)
	MENUITEMINFO mif = { sizeof MENUITEMINFO };
	mif.fMask = MIIM_SUBMENU;
	for (int i = menu.GetMenuItemCount(); i; i--)
	{
		menu.GetMenuItemInfo(i - 1, true, &mif);
		if (UserInfoGuiTraits::isUserInfoMenus(mif.hSubMenu) ||
		        mif.hSubMenu == g_copyHubMenu.m_hMenu ||
		        Preview::isPreviewMenu(mif.hSubMenu)
		   )
		{
			menu.RemoveMenu(i - 1, MF_BYPOSITION);
		}
	}
}

int WinUtil::GetMenuItemPosition(const CMenu &p_menu, UINT_PTR p_IDItem)
{
	for (int i = 0; i < p_menu.GetMenuItemCount(); ++i)
	{
		if (p_menu.GetMenuItemID(i) == p_IDItem)
			return i;
	}
	return -1;
}

static LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
		if (WinUtil::g_tabCtrl)
			if (wParam == VK_CONTROL && LOWORD(lParam) == 1)
			{
				if (lParam & 0x80000000)
				{
					WinUtil::g_tabCtrl->endSwitch();
				}
				else
				{
					WinUtil::g_tabCtrl->startSwitch();
				}
			}
	}
	return CallNextHookEx(WinUtil::g_hook, code, wParam, lParam);
}

void UserStateImage::init()
{
	ResourceLoader::LoadImageList(IDR_STATE_USERS, m_images, 16, 16);
}

void TrackerImage::init()
{
	ResourceLoader::LoadImageList(IDR_TRACKER_IMAGES, m_images, 16, 16);
}

void GenderImage::init()
{
	ResourceLoader::LoadImageList(IDR_GENDER_USERS, m_images, 16, 16);
}

void UserImage::init()
{
	if (SETTING(USERLIST_IMAGE).empty())
	{
		ResourceLoader::LoadImageList(IDR_USERS, m_images, 16, 16);
	}
	else
	{
		ResourceLoader::LoadImageList(Text::toT(SETTING(USERLIST_IMAGE)).c_str(), m_images, 16, 16);
	}
}

void VideoImage::init()
{
	ResourceLoader::LoadImageList(IDR_MEDIAFILES, m_images, 16, 16);
}

void TransferTreeImage::init()
{
	if (m_flagImageCount == 0)
	{
		m_flagImageCount = ResourceLoader::LoadImageList(IDR_TRANSFER_TREE, m_images, 16, 16);
		dcassert(m_flagImageCount);
	}
}


void ISPImage::init()
{
	if (m_flagImageCount == 0)
	{
		m_flagImageCount = ResourceLoader::LoadImageList(IDR_FLAGS, m_images, 25, 16);
		dcassert(m_flagImageCount);
		CImageList m_add_images;
		if (ResourceLoader::LoadImageList(IDR_ISP_HUBLIST, m_add_images, 25, 16))
		{
			for (int i = 0; i < m_add_images.GetImageCount(); ++i)
			{
				const auto l_res_merge =  m_images.AddIcon(m_add_images.GetIcon(i));
				dcassert(l_res_merge);
			}
		}
	}
}
void FlagImage::init()
{
	m_flagImageCount = ResourceLoader::LoadImageList(IDR_FLAGS, m_images, 25, 16);
	dcassert(m_flagImageCount);
	dcassert(m_images.GetImageCount() <= 255); // Чтобы не превысить 8 бит
	
	if (!CompatibilityManager::isWine()) // под линуксом пока падаем http://flylinkdc.blogspot.com/2010/08/customlocationsbmp-wine.html
	{
		CBitmap UserLocations;
		if (UserLocations.m_hBitmap = (HBITMAP)::LoadImage(NULL, Text::toT(Util::getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		                                                                       true
#endif
		                                                                   ) + "CustomLocations.bmp").c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE))
			m_images.Add(UserLocations, RGB(77, 17, 77));
	}
}

void WinUtil::init(HWND hWnd)
{
	g_mainWnd = hWnd;
	
	SetTabsPosition(SETTING(TABS_POS));
	
	Preview::init();
	
	g_mainMenu.CreateMenu();
	
	CMenuHandle file;
	file.CreatePopupMenu();
	
	file.AppendMenu(MF_STRING, IDC_OPEN_FILE_LIST, CTSTRING(MENU_OPEN_FILE_LIST));
	file.AppendMenu(MF_STRING, IDC_ADD_MAGNET, CTSTRING(MENU_ADD_MAGNET));
#ifdef FLYLINKDC_USE_TORRENT
	file.AppendMenu(MF_STRING, IDC_OPEN_TORRENT_FILE, CTSTRING(MENU_OPEN_TORRENT_FILE));
#endif
	file.AppendMenu(MF_STRING, IDC_OPEN_MY_LIST, CTSTRING(MENU_OPEN_OWN_LIST));
	file.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST, CTSTRING(MENU_REFRESH_FILE_LIST));
	file.AppendMenu(MF_STRING, IDC_MATCH_ALL, CTSTRING(MENU_OPEN_MATCH_ALL));
//	file.AppendMenu(MF_STRING, IDC_FLYLINK_DISCOVER, _T("Flylink Discover…"));
	file.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST_PURGE, CTSTRING(MENU_REFRESH_FILE_LIST_PURGE)); // https://www.box.net/shared/cw9agvj2n3fbypdcls46
#ifdef USE_REBUILD_MEDIAINFO
	file.AppendMenu(MF_STRING, IDC_REFRESH_MEDIAINFO, CTSTRING(MENU_REFRESH_MEDIAINFO));
#endif
//	if (CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_IsTTHLevelDBConvert) == 0)
	{
		file.AppendMenu(MF_STRING, IDC_CONVERT_TTH_HISTORY, CTSTRING(MENU_CONVERT_TTH_HISTORY_INTO_LEVELDB));
	}
	file.AppendMenu(MF_STRING, IDC_OPEN_DOWNLOADS, CTSTRING(MENU_OPEN_DOWNLOADS_DIR));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, IDC_OPEN_LOGS, CTSTRING(MENU_OPEN_LOGS_DIR));
	file.AppendMenu(MF_STRING, IDC_OPEN_CONFIGS, CTSTRING(MENU_OPEN_CONFIG_DIR));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_GET_TTH, CTSTRING(MENU_TTH));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_FILE_RECONNECT, CTSTRING(MENU_RECONNECT));
	file.AppendMenu(MF_STRING, IDC_RECONNECT_DISCONNECTED, CTSTRING(MENU_RECONNECT_DISCONNECTED));
	file.AppendMenu(MF_STRING, IDC_FOLLOW, CTSTRING(MENU_FOLLOW_REDIRECT));
	file.AppendMenu(MF_STRING, ID_FILE_QUICK_CONNECT, CTSTRING(MENU_QUICK_CONNECT));
	file.AppendMenu(MF_SEPARATOR);
#ifdef FLYLINKDC_USE_SQL_EXPLORER
	file.AppendMenu(MF_STRING, IDC_BROWSESQLLIST, CTSTRING(MENU_OPEN_SQLEXPLORER));
	file.AppendMenu(MF_SEPARATOR);
#endif
#ifdef SSA_WIZARD_FEATURE
	file.AppendMenu(MF_STRING, ID_FILE_SETTINGS_WIZARD, CTSTRING(MENU_SETTINGS_WIZARD));
#endif
	file.AppendMenu(MF_STRING, ID_FILE_SETTINGS, CTSTRING(MENU_SETTINGS));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_APP_EXIT, CTSTRING(MENU_EXIT));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)file, CTSTRING(MENU_FILE));
	
	CMenuHandle view;
	view.CreatePopupMenu();
	view.AppendMenu(MF_STRING, ID_FILE_CONNECT, CTSTRING(MENU_PUBLIC_HUBS));
	view.AppendMenu(MF_STRING, IDC_RECENTS, CTSTRING(MENU_FILE_RECENT_HUBS));
	view.AppendMenu(MF_STRING, IDC_FAVORITES, CTSTRING(MENU_FAVORITE_HUBS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, IDC_FAVUSERS, CTSTRING(MENU_FAVORITE_USERS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, ID_FILE_SEARCH, CTSTRING(MENU_SEARCH));
	view.AppendMenu(MF_STRING, IDC_FILE_ADL_SEARCH, CTSTRING(MENU_ADL_SEARCH));
	view.AppendMenu(MF_STRING, IDC_SEARCH_SPY, CTSTRING(MENU_SEARCH_SPY));
	view.AppendMenu(MF_SEPARATOR);
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	view.AppendMenu(MF_STRING, IDD_PREVIEW_LOG_DLG, CTSTRING(MENU_PREVIEW_LOG_DLG));
#endif
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
	view.AppendMenu(MF_STRING, IDC_CDMDEBUG_WINDOW, CTSTRING(MENU_CDMDEBUG_MESSAGES));
#endif
	view.AppendMenu(MF_STRING, IDC_NOTEPAD, CTSTRING(MENU_NOTEPAD));
	view.AppendMenu(MF_STRING, IDC_HASH_PROGRESS, CTSTRING(MENU_HASH_PROGRESS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, IDC_TOPMOST, CTSTRING(MENU_TOPMOST));
	view.AppendMenu(MF_STRING, ID_VIEW_TOOLBAR, CTSTRING(MENU_TOOLBAR));
	view.AppendMenu(MF_STRING, ID_VIEW_STATUS_BAR, CTSTRING(MENU_STATUS_BAR));
	view.AppendMenu(MF_STRING, ID_VIEW_TRANSFER_VIEW, CTSTRING(MENU_TRANSFER_VIEW));
	view.AppendMenu(MF_STRING, ID_TOGGLE_TOOLBAR, CTSTRING(TOGGLE_TOOLBAR));
	view.AppendMenu(MF_STRING, ID_TOGGLE_QSEARCH, CTSTRING(TOGGLE_QSEARCH));
	view.AppendMenu(MF_STRING, ID_VIEW_TRANSFER_VIEW_TOOLBAR, CTSTRING(MENU_TRANSFER_VIEW_TOOLBAR));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)view, CTSTRING(MENU_VIEW));
	
	CMenuHandle transfers;
	transfers.CreatePopupMenu();
	
	transfers.AppendMenu(MF_STRING, IDC_QUEUE, CTSTRING(MENU_DOWNLOAD_QUEUE));
	transfers.AppendMenu(MF_STRING, IDC_FINISHED, CTSTRING(MENU_FINISHED_DOWNLOADS));
	transfers.AppendMenu(MF_SEPARATOR);
	transfers.AppendMenu(MF_STRING, IDC_UPLOAD_QUEUE, CTSTRING(MENU_WAITING_USERS));
	transfers.AppendMenu(MF_STRING, IDC_FINISHED_UL, CTSTRING(MENU_FINISHED_UPLOADS));
	transfers.AppendMenu(MF_SEPARATOR);
	transfers.AppendMenu(MF_STRING, IDC_NET_STATS, CTSTRING(MENU_NETWORK_STATISTICS));
#ifdef IRAINMAN_INCLUDE_RSS
	transfers.AppendMenu(MF_SEPARATOR);
	transfers.AppendMenu(MF_STRING, IDC_RSS, CTSTRING(MENU_RSS_NEWS));
#endif
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)transfers, CTSTRING(MENU_TRANSFERS));
	
#ifdef RIP_USE_PORTAL_BROWSER
	CMenuHandle PortalBrowserMenu;
	if (InitPortalBrowserMenuItems(PortalBrowserMenu))
		g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)PortalBrowserMenu, CTSTRING(PORTAL_BROWSER));
#endif
#ifdef FLYLINKDC_USE_PROVIDER_RESOURCES
#ifdef FLYLINKDC_USE_CUSTOM_MENU
	CMenuHandle customMenuXML;
	string customMenuNameXML;
	if (FillCustomMenu(customMenuXML, customMenuNameXML))
		g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)customMenuXML, Text::toT(customMenuNameXML).c_str());
#endif
#endif
		
	CMenuHandle window;
	window.CreatePopupMenu();
	
	window.AppendMenu(MF_STRING, ID_WINDOW_CASCADE, CTSTRING(MENU_CASCADE));
	window.AppendMenu(MF_STRING, ID_WINDOW_TILE_HORZ, CTSTRING(MENU_HORIZONTAL_TILE));
	window.AppendMenu(MF_STRING, ID_WINDOW_TILE_VERT, CTSTRING(MENU_VERTICAL_TILE));
	window.AppendMenu(MF_STRING, ID_WINDOW_ARRANGE, CTSTRING(MENU_ARRANGE));
	window.AppendMenu(MF_STRING, ID_WINDOW_MINIMIZE_ALL, CTSTRING(MENU_MINIMIZE_ALL));
	window.AppendMenu(MF_STRING, ID_WINDOW_RESTORE_ALL, CTSTRING(MENU_RESTORE_ALL));
	window.AppendMenu(MF_SEPARATOR);
	window.AppendMenu(MF_STRING, IDC_CLOSE_DISCONNECTED, CTSTRING(MENU_CLOSE_DISCONNECTED));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_HUBS, CTSTRING(MENU_CLOSE_ALL_HUBS));
	window.AppendMenu(MF_STRING, IDC_CLOSE_HUBS_BELOW, CTSTRING(MENU_CLOSE_HUBS_BELOW));
	window.AppendMenu(MF_STRING, IDC_CLOSE_HUBS_NO_USR, CTSTRING(MENU_CLOSE_HUBS_EMPTY));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_PM, CTSTRING(MENU_CLOSE_ALL_PM));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_PM, CTSTRING(MENU_CLOSE_ALL_OFFLINE_PM));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_DIR_LIST, CTSTRING(MENU_CLOSE_ALL_DIR_LIST));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_SEARCH_FRAME, CTSTRING(MENU_CLOSE_ALL_SEARCHFRAME));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)window, CTSTRING(MENU_WINDOW));
	
#ifdef FLYLINKDC_USE_DEAD_CODE
	CMenuHandle sites;
	sites.CreatePopupMenu();
	
	sites.AppendMenu(MF_STRING, IDC_GUIDE, CTSTRING(MENU_SITES_GUIDE));
#endif
	CMenuHandle help;
	help.CreatePopupMenu();
	
	help.AppendMenu(MF_STRING, IDC_HELP_HELP, CTSTRING(MENU_HELP));
	help.AppendMenu(MF_SEPARATOR);
	help.AppendMenu(MF_STRING, IDC_HELP_HOMEPAGE, CTSTRING(MENU_HOMEPAGE));
	help.AppendMenu(MF_STRING, IDC_HELP_DISCUSS, CTSTRING(MENU_DISCUSS));
#ifdef FLYLINKDC_USE_DEAD_CODE
	help.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)sites, CTSTRING(MENU_SITES));
	help.AppendMenu(MF_STRING, IDC_HELP_GEOIPFILE, CTSTRING(MENU_HELP_GEOIPFILE));
#endif
	
	help.AppendMenu(MF_STRING, IDC_SITES_FLYLINK_TRAC, CTSTRING(MENU_JOIN_TEAM));
#ifdef USE_SUPPORT_HUB
	help.AppendMenu(MF_STRING, IDC_CONNECT_TO_FLYSUPPORT_HUB, CTSTRING(MENU_CONNECT_TO_HUB));
#endif //USE_SUPPORT_HUB
	
	help.AppendMenu(MF_SEPARATOR);
	help.AppendMenu(MF_STRING, IDC_UPDATE_FLYLINKDC, CTSTRING(UPDATE_CHECK));
	help.AppendMenu(MF_STRING, ID_APP_ABOUT, CTSTRING(MENU_ABOUT));
	
	g_mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)help, CTSTRING(MENU_HLP));
	
#ifdef FLYLINKDC_USE_EXT_JSON
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
	CMenuHandle l_menu_flylinkdc_location;
	l_menu_flylinkdc_location.CreatePopupMenu();
	l_menu_flylinkdc_location.AppendMenu(MF_STRING, IDC_FLYLINKDC_LOCATION, CTSTRING(MENU_CHANGE_FLYLINKDC_LOCATION)); //  _T("Change FlylinkDC++ location!")
	const string l_text_flylinkdc_location = "|||| " + SETTING(FLY_LOCATOR_COUNTRY) +
	                                         " - " + SETTING(FLY_LOCATOR_CITY) + " - " + SETTING(FLY_LOCATOR_ISP) + " ||||";
	g_mainMenu.AppendMenu(MF_STRING, l_menu_flylinkdc_location, Text::toT(l_text_flylinkdc_location).c_str());
#endif
	
#endif // FLYLINKDC_USE_EXT_JSON
	g_fileImage.init();
	
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
	g_videoImage.init();
#endif
	
	g_flagImage.init();
	
	g_userImage.init();
	g_userStateImage.init();
	g_trackerImage.init();
	g_genderImage.init();
	
	Colors::init();
	
	Fonts::init();
	
	Util::setRegistryValueString(_T("ApplicationPath"), Util::getModuleFileName());
	
	
	if (BOOLSETTING(URL_HANDLER))
	{
		registerDchubHandler();
		registerNMDCSHandler();
		registerADChubHandler();
		registerADCShubHandler();
		urlDcADCRegistered = true;
	}
	
	if (BOOLSETTING(MAGNET_REGISTER))
	{
		registerMagnetHandler();
		urlMagnetRegistered = true;
	}
	if (BOOLSETTING(DCLST_REGISTER))
	{
		registerDclstHandler();
		DclstRegistered = true;
	}
	
	g_hook = SetWindowsHookEx(WH_KEYBOARD, &KeyboardProc, NULL, GetCurrentThreadId());
	
	g_copyHubMenu.CreatePopupMenu();
	g_copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBNAME, CTSTRING(HUB_NAME));
	g_copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBADDRESS, CTSTRING(HUB_ADDRESS));
	g_copyHubMenu.InsertSeparatorFirst(TSTRING(COPY));
	
	
	
	UserInfoGuiTraits::init();
}

void UserInfoGuiTraits::init()
{
	copyUserMenu.CreatePopupMenu();
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_ANTIVIRUS_DB_INFO, CTSTRING(COPY_ANTIVIRUS_DB_INFO));
#endif
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_EXACT_SHARE, CTSTRING(COPY_EXACT_SHARE));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_DESCRIPTION, CTSTRING(COPY_DESCRIPTION));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_APPLICATION, CTSTRING(COPY_APPLICATION));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_TAG, CTSTRING(COPY_TAG));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_CID, CTSTRING(COPY_CID));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_EMAIL_ADDRESS, CTSTRING(COPY_EMAIL_ADDRESS));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_GEO_LOCATION, CTSTRING(COPY_GEO_LOCATION));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_IP, CTSTRING(COPY_IP));
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_NICK_IP, CTSTRING(COPY_NICK_IP));
	
	copyUserMenu.AppendMenu(MF_STRING, IDC_COPY_ALL, CTSTRING(COPY_ALL));
	
	grantMenu.CreatePopupMenu();
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT, CTSTRING(GRANT_EXTRA_SLOT));
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_HOUR, CTSTRING(GRANT_EXTRA_SLOT_HOUR));
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_DAY, CTSTRING(GRANT_EXTRA_SLOT_DAY));
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_WEEK, CTSTRING(GRANT_EXTRA_SLOT_WEEK));
	grantMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT_PERIOD, CTSTRING(EXTRA_SLOT_TIMEOUT));
	grantMenu.AppendMenu(MF_SEPARATOR);
	grantMenu.AppendMenu(MF_STRING, IDC_UNGRANTSLOT, CTSTRING(REMOVE_EXTRA_SLOT));
	
	
	userSummaryMenu.CreatePopupMenu();
	
	
	speedMenu.CreatePopupMenu();
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_NORMAL, CTSTRING(NORMAL));
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_SUPER, CTSTRING(SPEED_SUPER_USER));
	speedMenu.AppendMenu(MF_SEPARATOR);
	
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_128K, (_T("128 ") + TSTRING(KBPS)).c_str());
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_512K, (_T("512 ") + TSTRING(KBPS)).c_str());
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_1024K, (_T("1 ") + TSTRING(MBPS)).c_str());
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_2048K, (_T("2 ") + TSTRING(MBPS)).c_str());
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_4096K, (_T("4 ") + TSTRING(MBPS)).c_str());
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_10240K, (_T("10 ") + TSTRING(MBPS)).c_str());
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_MANUAL, CTSTRING(SPEED_LIMIT_MANUAL));
	
	speedMenu.AppendMenu(MF_STRING, IDC_SPEED_BAN,  CTSTRING(BAN_USER));
	
	
	privateMenu.CreatePopupMenu();
	privateMenu.AppendMenu(MF_STRING, IDC_PM_NORMAL,    CTSTRING(NORMAL));
	privateMenu.AppendMenu(MF_STRING, IDC_PM_IGNORED, CTSTRING(IGNORE_S));
	privateMenu.AppendMenu(MF_STRING, IDC_PM_FREE,    CTSTRING(FREE_PM_ACCESS));
}

void UserInfoGuiTraits::uninit()
{
	copyUserMenu.DestroyMenu();
	grantMenu.DestroyMenu();
	userSummaryMenu.DestroyMenu();
	speedMenu.DestroyMenu();
	privateMenu.DestroyMenu();
}

void Fonts::init()
{
	LOGFONT lf[2] = {0};
	::GetObject((HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(lf[0]), &lf[0]);
	// SettingsManager::setDefault(SettingsManager::TEXT_FONT, Text::fromT(encodeFont(lf)));
	
	lf[0].lfWeight = FW_BOLD;
	g_boldFont = ::CreateFontIndirect(&lf[0]);
	//---------------------------------
	lf[1] = lf[0];
	lf[1].lfHeight += 3;
	lf[1].lfWeight = FW_NORMAL;
	//lf[1].lfUnderline = 1;
	g_halfFont = ::CreateFontIndirect(&lf[1]);
	
	decodeFont(Text::toT(SETTING(TEXT_FONT)), lf[0]);
	//::GetObject((HFONT)GetStockObject(ANSI_FIXED_FONT), sizeof(lf[1]), &lf[1]);
	
	//lf[1].lfHeight = lf[0].lfHeight;
	//lf[1].lfWeight = lf[0].lfWeight;
	//lf[1].lfItalic = lf[0].lfItalic;
	
	g_font = ::CreateFontIndirect(&lf[0]);
	g_fontHeight = WinUtil::getTextHeight(WinUtil::g_mainWnd, g_font);
	g_systemFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	{
		HDC hDC = ::GetDC(NULL);
		const int l_pix = ::GetDeviceCaps(hDC, LOGPIXELSY);
		g_fontHeightPixl = -::MulDiv(lf[0].lfHeight, l_pix, 72);
		int l_res = ReleaseDC(NULL, hDC);
		dcassert(l_res);
	}
}

void Colors::init()
{
	g_textColor = SETTING(TEXT_COLOR);
	g_bgColor = SETTING(BACKGROUND_COLOR);
	
	g_bgBrush = CreateSolidBrush(Colors::g_bgColor); // Leak
	
	CHARFORMAT2 cf;
	memzero(&cf, sizeof(CHARFORMAT2));
	cf.cbSize = sizeof(cf);
	cf.dwReserved = 0;
	cf.dwMask = CFM_BACKCOLOR | CFM_COLOR | CFM_BOLD | CFM_ITALIC;
	cf.dwEffects = 0;
	cf.crBackColor = SETTING(BACKGROUND_COLOR);
	cf.crTextColor = SETTING(TEXT_COLOR);
	
	g_TextStyleTimestamp = cf;
	g_TextStyleTimestamp.crBackColor = SETTING(TEXT_TIMESTAMP_BACK_COLOR);
	g_TextStyleTimestamp.crTextColor = SETTING(TEXT_TIMESTAMP_FORE_COLOR);
	if (SETTING(TEXT_TIMESTAMP_BOLD))
		g_TextStyleTimestamp.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_TIMESTAMP_ITALIC))
		g_TextStyleTimestamp.dwEffects |= CFE_ITALIC;
		
	g_ChatTextGeneral = cf;
	g_ChatTextGeneral.crBackColor = SETTING(TEXT_GENERAL_BACK_COLOR);
	g_ChatTextGeneral.crTextColor = SETTING(TEXT_GENERAL_FORE_COLOR);
	if (SETTING(TEXT_GENERAL_BOLD))
		g_ChatTextGeneral.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_GENERAL_ITALIC))
		g_ChatTextGeneral.dwEffects |= CFE_ITALIC;
		
	g_ChatTextOldHistory = cf;
	g_ChatTextOldHistory.crBackColor = SETTING(TEXT_GENERAL_BACK_COLOR);
	g_ChatTextOldHistory.crTextColor = SETTING(TEXT_GENERAL_FORE_COLOR);
	g_ChatTextOldHistory.yHeight = 5;
	
	g_TextStyleBold = g_ChatTextGeneral;
	g_TextStyleBold.dwEffects = CFE_BOLD;
	
	g_TextStyleMyNick = cf;
	g_TextStyleMyNick.crBackColor = SETTING(TEXT_MYNICK_BACK_COLOR);
	g_TextStyleMyNick.crTextColor = SETTING(TEXT_MYNICK_FORE_COLOR);
	if (SETTING(TEXT_MYNICK_BOLD))
		g_TextStyleMyNick.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_MYNICK_ITALIC))
		g_TextStyleMyNick.dwEffects |= CFE_ITALIC;
		
	g_ChatTextMyOwn = cf;
	g_ChatTextMyOwn.crBackColor = SETTING(TEXT_MYOWN_BACK_COLOR);
	g_ChatTextMyOwn.crTextColor = SETTING(TEXT_MYOWN_FORE_COLOR);
	if (SETTING(TEXT_MYOWN_BOLD))
		g_ChatTextMyOwn.dwEffects       |= CFE_BOLD;
	if (SETTING(TEXT_MYOWN_ITALIC))
		g_ChatTextMyOwn.dwEffects       |= CFE_ITALIC;
		
	g_ChatTextPrivate = cf;
	g_ChatTextPrivate.crBackColor = SETTING(TEXT_PRIVATE_BACK_COLOR);
	g_ChatTextPrivate.crTextColor = SETTING(TEXT_PRIVATE_FORE_COLOR);
	if (SETTING(TEXT_PRIVATE_BOLD))
		g_ChatTextPrivate.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_PRIVATE_ITALIC))
		g_ChatTextPrivate.dwEffects |= CFE_ITALIC;
		
	g_ChatTextSystem = cf;
	g_ChatTextSystem.crBackColor = SETTING(TEXT_SYSTEM_BACK_COLOR);
	g_ChatTextSystem.crTextColor = SETTING(TEXT_SYSTEM_FORE_COLOR);
	if (SETTING(TEXT_SYSTEM_BOLD))
		g_ChatTextSystem.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_SYSTEM_ITALIC))
		g_ChatTextSystem.dwEffects |= CFE_ITALIC;
		
	g_ChatTextServer = cf;
	g_ChatTextServer.crBackColor = SETTING(TEXT_SERVER_BACK_COLOR);
	g_ChatTextServer.crTextColor = SETTING(TEXT_SERVER_FORE_COLOR);
	if (SETTING(TEXT_SERVER_BOLD))
		g_ChatTextServer.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_SERVER_ITALIC))
		g_ChatTextServer.dwEffects |= CFE_ITALIC;
		
	g_ChatTextLog = g_ChatTextGeneral;
	g_ChatTextLog.crTextColor = OperaColors::blendColors(SETTING(TEXT_GENERAL_BACK_COLOR), SETTING(TEXT_GENERAL_FORE_COLOR), 0.4);
	
	g_TextStyleFavUsers = cf;
	g_TextStyleFavUsers.crBackColor = SETTING(TEXT_FAV_BACK_COLOR);
	g_TextStyleFavUsers.crTextColor = SETTING(TEXT_FAV_FORE_COLOR);
	if (SETTING(TEXT_FAV_BOLD))
		g_TextStyleFavUsers.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_FAV_ITALIC))
		g_TextStyleFavUsers.dwEffects |= CFE_ITALIC;
		
	g_TextStyleFavUsersBan = cf;
	g_TextStyleFavUsersBan.crBackColor = SETTING(TEXT_ENEMY_BACK_COLOR);
	g_TextStyleFavUsersBan.crTextColor = SETTING(TEXT_ENEMY_FORE_COLOR);
	if (SETTING(TEXT_ENEMY_BOLD))
		g_TextStyleFavUsersBan.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_ENEMY_ITALIC))
		g_TextStyleFavUsersBan.dwEffects |= CFE_ITALIC;
		
	g_TextStyleOPs = cf;
	g_TextStyleOPs.crBackColor = SETTING(TEXT_OP_BACK_COLOR);
	g_TextStyleOPs.crTextColor = SETTING(TEXT_OP_FORE_COLOR);
	if (SETTING(TEXT_OP_BOLD))
		g_TextStyleOPs.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_OP_ITALIC))
		g_TextStyleOPs.dwEffects |= CFE_ITALIC;
		
	g_TextStyleURL = cf;
	g_TextStyleURL.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_BACKCOLOR | CFM_LINK | CFM_UNDERLINE;
	g_TextStyleURL.crBackColor = SETTING(TEXT_URL_BACK_COLOR);
	g_TextStyleURL.crTextColor = SETTING(TEXT_URL_FORE_COLOR);
	g_TextStyleURL.dwEffects = CFE_LINK | CFE_UNDERLINE;
	if (SETTING(TEXT_URL_BOLD))
		g_TextStyleURL.dwEffects |= CFE_BOLD;
	if (SETTING(TEXT_URL_ITALIC))
		g_TextStyleURL.dwEffects |= CFE_ITALIC;
}

void WinUtil::uninit()
{
	UnhookWindowsHookEx(g_hook);
	g_hook = nullptr;
	
	g_tabCtrl = nullptr;
	g_mainWnd = nullptr;
	g_fileImage.uninit();
	g_userImage.uninit();
	g_trackerImage.uninit();
	g_userStateImage.uninit();
	g_genderImage.uninit();
	g_ISPImage.uninit(); // TODO - позже
	g_TransferTreeImage.uninit();
	g_flagImage.uninit();
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
	g_videoImage.uninit();
#endif
	Fonts::uninit();
	Colors::uninit();
	
	g_mainMenu.DestroyMenu();
	g_copyHubMenu.DestroyMenu();
	
	
	UserInfoGuiTraits::uninit();
}

void Fonts::decodeFont(const tstring& setting, LOGFONT &dest)
{
	const StringTokenizer<tstring, TStringList> st(setting, _T(','));
	const auto& sl = st.getTokens();
	
	::GetObject((HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(dest), &dest);
	tstring face;
	if (sl.size() == 4)
	{
		face = sl[0];
		dest.lfHeight = Util::toInt(sl[1]);
		dest.lfWeight = Util::toInt(sl[2]);
		dest.lfItalic = (BYTE)Util::toInt(sl[3]);
	}
	
	if (!face.empty())
	{
		memzero(dest.lfFaceName, sizeof(dest.lfFaceName));
		// [!] PVS V512 A call of the 'memset' function will lead to underflow of the buffer 'dest.lfFaceName'. flylinkdc   winutil.cpp 845 False
		_tcscpy_s(dest.lfFaceName, face.c_str());
	}
}

int CALLBACK WinUtil::browseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lp*/, LPARAM pData)
{
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
			break;
	}
	return 0;
}

bool WinUtil::browseDirectory(tstring& target, HWND owner /* = NULL */)
{
	AutoArray <TCHAR> buf(FULL_MAX_PATH);
	BROWSEINFO bi = {0};
	bi.hwndOwner = owner;
	bi.pszDisplayName = buf;
	bi.lpszTitle = CTSTRING(CHOOSE_FOLDER);
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
	bi.lParam = (LPARAM)target.c_str();
	bi.lpfn = &browseCallbackProc;
	if (LPITEMIDLIST pidl = SHBrowseForFolder(&bi))
	{
		SHGetPathFromIDList(pidl, buf);
		target = buf;
		
		AppendPathSeparator(target);
		WinUtil::safe_sh_free(pidl);
		return true;
	}
	return false;
}

bool WinUtil::browseFile(tstring& target, HWND owner /* = NULL */, bool save /* = true */, const tstring& initialDir /* = BaseUtil::emptyString */, const TCHAR* types /* = NULL */, const TCHAR* defExt /* = NULL */)
{
	OPENFILENAME ofn = { 0 };       // common dialog box structure
	target = Text::toT(Util::validateFileName(Text::fromT(target)));
	AutoArray <TCHAR> l_buf(FULL_MAX_PATH);
	_tcscpy_s(l_buf, FULL_MAX_PATH, target.c_str());
	// Initialize OPENFILENAME
	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner = owner;
	ofn.lpstrFile = l_buf.data();
	ofn.lpstrFilter = types;
	ofn.lpstrDefExt = defExt;
	ofn.nFilterIndex = 1;
	
	if (!initialDir.empty())
	{
		ofn.lpstrInitialDir = initialDir.c_str();
	}
	ofn.nMaxFile = FULL_MAX_PATH;
	ofn.Flags = (save ? 0 : OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);
	
	// Display the Open dialog box.
	if ((save ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn)) != FALSE)
	{
		target = ofn.lpstrFile;
		return true;
	}
	else
	{
		dcdebug("Error WinUtil::browseFile CommDlgExtendedError() = %x\n", CommDlgExtendedError());
	}
	return false;
}

tstring WinUtil::encodeFont(const LOGFONT& font)
{
	tstring res(font.lfFaceName);
	res += L',';
	res += Util::toStringW(font.lfHeight);
	res += L',';
	res += Util::toStringW(font.lfWeight);
	res += L',';
	res += Util::toStringW(font.lfItalic);
	return res;
}

void WinUtil::setClipboard(const tstring& str)
{
	if (!::OpenClipboard(g_mainWnd))
	{
		return;
	}
	
	EmptyClipboard();
	
	
	// Allocate a global memory object for the text.
	HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (str.size() + 1) * sizeof(TCHAR));
	if (hglbCopy == NULL)
	{
		CloseClipboard();
		return;
	}
	
	// Lock the handle and copy the text to the buffer.
	TCHAR* lptstrCopy = (TCHAR*)GlobalLock(hglbCopy);
	_tcscpy(lptstrCopy, str.c_str());
	GlobalUnlock(hglbCopy);
	
	// Place the handle on the clipboard.
	SetClipboardData(CF_UNICODETEXT, hglbCopy);
	
	CloseClipboard();
}

void WinUtil::splitTokensWidth(int* p_array, const string& p_tokens, int p_maxItems) noexcept
{
	splitTokens(p_array, p_tokens, p_maxItems);
	for (int k = 0; k < p_maxItems; ++k)
		if (p_array[k] <= 0 || p_array[k] > 2000)
			p_array[k] = 10;
}

void WinUtil::splitTokens(int* p_array, const string& p_tokens, int p_maxItems) noexcept
{
	dcassert(p_maxItems > 0);
	const StringTokenizer<string> t(p_tokens, ',');
	const StringList& l = t.getTokens();
	int k = 0;
	for (auto i = l.cbegin(); i != l.cend() && k < p_maxItems; ++i, ++k)
	{
		p_array[k] = Util::toInt(*i);
	}
}

bool WinUtil::getUCParams(HWND parent, const UserCommand& uc, StringMap& sm) // TODO убрать parent
{
	string::size_type i = 0;
	StringMap done;
	
	while ((i = uc.getCommand().find("%[line:", i)) != string::npos)
	{
		i += 7;
		string::size_type j = uc.getCommand().find(']', i);
		if (j == string::npos)
			break;
			
		string name = uc.getCommand().substr(i, j - i);
		if (done.find(name) == done.end())
		{
			LineDlg dlg;
			dlg.title = Text::toT(Util::toString(" > ", uc.getDisplayName()));
			dlg.description = Text::toT(name);
			dlg.line = Text::toT(sm["line:" + name]);
			if (dlg.DoModal(parent) == IDOK)
			{
				const auto l_str = Text::fromT(dlg.line);
				sm["line:" + name] = l_str;
				done[name] = l_str;
			}
			else
			{
				return false;
			}
		}
		i = j + 1;
	}
	i = 0;
	while ((i = uc.getCommand().find("%[kickline:", i)) != string::npos)
	{
		i += 11;
		string::size_type j = uc.getCommand().find(']', i);
		if (j == string::npos)
			break;
			
		string name = uc.getCommand().substr(i, j - i);
		if (done.find(name) == done.end())
		{
			KickDlg dlg;
			dlg.title = Text::toT(Util::toString(" > ", uc.getDisplayName()));
			dlg.description = Text::toT(name);
			if (dlg.DoModal(parent) == IDOK)
			{
				const auto l_str = Text::fromT(dlg.line);
				sm["kickline:" + name] = l_str;
				done[name] = l_str;
			}
			else
			{
				return false;
			}
		}
		i = j + 1;
	}
	return true;
}

tstring WinUtil::getCommandsList()
{
	return _T("*** ") + TSTRING(CMD_FIRST_LINE) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/away, /a (message) \t\t\t") + TSTRING(CMD_AWAY_MSG) +
	       _T("\n/clear, /c \t\t\t\t") + TSTRING(CMD_CLEAR_CHAT) +
	       _T("\n/favshowjoins, /fsj \t\t\t") + TSTRING(CMD_FAV_JOINS) +
	       _T("\n/showjoins, /sj \t\t\t\t") + TSTRING(CMD_SHOW_JOINS) +
	       _T("\n/ts \t\t\t\t\t") + TSTRING(CMD_TIME_STAMP) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/slots, /sl # \t\t\t\t") + TSTRING(CMD_SLOTS) +
	       _T("\n/extraslots, /es # \t\t\t") + TSTRING(CMD_EXTRA_SLOTS) +
	       _T("\n/smallfilesize, /sfs # \t\t\t") + TSTRING(CMD_SMALL_FILES) +
	       _T("\n/refresh \t\t\t\t") + TSTRING(CMD_SHARE_REFRESH) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/join, /j # \t\t\t\t") + TSTRING(CMD_JOIN_HUB) +
	       _T("\n/close \t\t\t\t\t") + TSTRING(CMD_CLOSE_WND) +
	       _T("\n/favorite, /fav \t\t\t\t") + TSTRING(CMD_FAV_HUB) +
	       _T("\n/rmfavorite, /rf \t\t\t\t") + TSTRING(CMD_RM_FAV) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/userlist, /ul \t\t\t\t") + TSTRING(CMD_USERLIST) +
	       _T("\n/switch \t\t\t\t\t") + TSTRING(CMD_SWITCHPANELS) +
	       _T("\n/ignorelist, /il \t\t\t\t") + TSTRING(CMD_IGNORELIST) +
	       _T("\n/favuser, /fu # \t\t\t\t") + TSTRING(CMD_FAV_USER) +
	       _T("\n/pm (user) (message) \t\t\t") + TSTRING(CMD_SEND_PM) +
	       _T("\n/getlist, /gl (user) \t\t\t") + TSTRING(CMD_GETLIST) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/flylinkdc++, /fv \t\t\t") + TSTRING(CMD_FLYLINKDC) +
	       _T("\n/uptime, /ut \t\t\t\t") + TSTRING(CMD_UPTIME) +
	       _T("\n/connection, /con \t\t\t") + TSTRING(CMD_CONNECTION) +
	       _T("\n/connection pub, /con pub\t\t\t") + TSTRING(CMD_PUBLIC_CONNECTION) +
	       // AirDC++
	       _T("\n/speed, /speed pub \t\t\t") + TSTRING(AVERAGE_DOWNLOAD_UPLOAD) +
	       _T("\n/dsp, /dsp pub \t\t\t\t") + TSTRING(DISK_SPACE) +
	       _T("\n/disks, /disks pub \t\t\t\t") + TSTRING(DISKS_INFO) +
	       _T("\n/cpu, /cpu pub \t\t\t\t") + TSTRING(CPU_INFO) +
	       // AirDC++
	       _T("\n/stats \t\t\t\t\t") + TSTRING(CMD_STATS) +
	       _T("\n/stats pub\t\t\t\t") + TSTRING(CMD_PUBLIC_STATS) +
	       _T("\n/systeminfo, /sysinfo \t\t\t") + TSTRING(CMD_SYSTEM_INFO) +
	       _T("\n/systeminfo pub, /sysinfo pub\t\t") + TSTRING(CMD_PUBLIC_SYSTEM_INFO) +
	       _T("\n/u (url) \t\t\t\t\t") + TSTRING(CMD_URL) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/search, /s (string) \t\t\t") + TSTRING(CMD_DO_SEARCH) +
	       _T("\n/google, /g (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_GOOGLE) +
	       _T("\n/define (string) \t\t\t\t") + TSTRING(CMD_DO_SEARCH_GOOGLEDEFINE) +
	       _T("\n/yandex, /y (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_YANDEX) +
	       _T("\n/yahoo, /yh (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_YAHOO) +
	       _T("\n/wikipedia, /wiki (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_WIKI) +
	       _T("\n/imdb, /i (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_IMDB) +
	       _T("\n/kinopoisk, /kp, /k (string) \t\t") + TSTRING(CMD_DO_SEARCH_KINOPOISK) +
	       _T("\n/rutracker, /rt, /t (string) \t\t") + TSTRING(CMD_DO_SEARCH_RUTRACKER) +
	       _T("\n/thepirate, /tpb (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_THEPIRATE) +
	       _T("\n/vkontakte, /vk, /v (string) \t\t") + TSTRING(CMD_DO_SEARCH_VK) +
	       _T("\n/vkid, /vid (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_VKID) +
	       _T("\n/discogs, /ds (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_DISC) +
	       _T("\n/filext, /ext (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_EXT) +
	       _T("\n/blog, /b (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_BLOG) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/savequeue, /sq \t\t\t") + TSTRING(CMD_SAVE_QUEUE) +
	       _T("\n/shutdown \t\t\t\t") + TSTRING(CMD_SHUTDOWN) +
	       _T("\n/me \t\t\t\t\t") + TSTRING(CMD_ME) +
	       _T("\n/winamp, /w (/wmp, /itunes, /mpc, /ja) \t") + TSTRING(CMD_WINAMP) +
	       // AirDC++
	       //   _T("\n/spotify, /s \t\t\t") + TSTRING(CMD_SPOTIFY) +
	       _T("\n/n \t\t\t\t\t") + TSTRING(CMD_REPLACE_WITH_LAST_INSERTED_NICK) + // SSA_SAVE_LAST_NICK_MACROS
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------\n") +
	       TSTRING(CMD_HELP_INFO) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------\n")
	       ;
}
bool WinUtil::checkCommand(tstring& cmd, tstring& param, tstring& message, tstring& status, tstring& local_message)
{
	string::size_type i = cmd.find(' ');
	if (i != string::npos)
	{
		param = cmd.substr(i + 1);
		cmd = cmd.substr(1, i - 1);
	}
	else
	{
		cmd = cmd.substr(1);
	}
	if (stricmp(cmd.c_str(), _T("help")) == 0  || stricmp(cmd.c_str(), _T("h")) == 0)
	{
		local_message = getCommandsList();
	}
	else if (stricmp(cmd.c_str(), _T("stats")) == 0)
	{
		if (stricmp(param.c_str(), _T("pub")) == 0)
		{
			message = Text::toT(CompatibilityManager::generateProgramStats());
		}
		else
		{
			local_message = Text::toT(CompatibilityManager::generateProgramStats());
		}
	}
	else if (stricmp(cmd.c_str(), _T("log")) == 0)
	{
		if (stricmp(param.c_str(), _T("system")) == 0)
		{
			WinUtil::openFile(Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + "system.log")));
		}
		else if (stricmp(param.c_str(), _T("downloads")) == 0)
		{
			WinUtil::openFile(Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatTime(SETTING(LOG_FILE_DOWNLOAD), time(NULL)))));
		}
		else if (stricmp(param.c_str(), _T("uploads")) == 0)
		{
			WinUtil::openFile(Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatTime(SETTING(LOG_FILE_UPLOAD), time(NULL)))));
		}
		else
		{
			return false;
		}
	}
	else if (stricmp(cmd.c_str(), _T("refresh")) == 0)
	{
		try
		{
			ShareManager::getInstance()->setDirty();
			ShareManager::getInstance()->refresh_share(true);
		}
		catch (const ShareException& e)
		{
			status = Text::toT(e.getError());
		}
	}
	else if (stricmp(cmd.c_str(), _T("slots")) == 0)
	{
		int j = Util::toInt(param);
		if (j > 0)
		{
			SET_SETTING(SLOTS, j);
			status = TSTRING(SLOTS_SET);
			ClientManager::infoUpdated(); // Не звать если не меняется SLOTS_SET
		}
		else
		{
			status = TSTRING(INVALID_NUMBER_OF_SLOTS);
		}
	}
	else if (stricmp(cmd.c_str(), _T("extraslots")) == 0)
	{
		int j = Util::toInt(param);
		if (j > 0)
		{
			SET_SETTING(EXTRA_SLOTS, j);
			status = TSTRING(EXTRA_SLOTS_SET);
		}
		else
		{
			status = TSTRING(INVALID_NUMBER_OF_SLOTS);
		}
	}
	else if (stricmp(cmd.c_str(), _T("smallfilesize")) == 0)
	{
		int j = Util::toInt(param);
		if (j >= 64)
		{
			SET_SETTING(SET_MINISLOT_SIZE, j);
			status = TSTRING(SMALL_FILE_SIZE_SET);
		}
		else
		{
			status = TSTRING(INVALID_SIZE);
		}
	}
	else if (stricmp(cmd.c_str(), _T("search")) == 0)
	{
		if (!param.empty())
		{
			SearchFrame::openWindow(param);
		}
		else
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
	}
	else if (stricmp(cmd.c_str(), _T("flylinkdc++")) == 0 || stricmp(cmd.c_str(), _T("flylinkdc")) == 0 || stricmp(cmd.c_str(), _T("fv")) == 0)
	{
		message = _T("FlylinkDC++ version message (/fv, /flylinkdc++, /flylinkdc):\r\n -- ") _T(HOMEPAGERU) _T(" <FlylinkDC++ ") T_VERSIONSTRING _T(" / ") _T(DCVERSIONSTRING) _T(">");
	}
	// Lee's /uptime support, why can't he always ask this kind of easy things.
	else if (stricmp(cmd.c_str(), _T("uptime")) == 0 || stricmp(cmd.c_str(), _T("ut")) == 0)
	{
		message = Text::toT("+me Uptime: " + Util::formatTime(Util::getUpTime()) + ". System uptime: " + CompatibilityManager::getSysUptime());
	}
	else if (stricmp(cmd.c_str(), _T("systeminfo")) == 0 || stricmp(cmd.c_str(), _T("sysinfo")) == 0)
	{
		tstring tmp = _T("+me systeminfo: ") +
		              Text::toT(CompatibilityManager::generateFullSystemStatusMessage());
		if (stricmp(param.c_str(), _T("pub")) == 0)
		{
			message = tmp;
		}
		else
		{
			local_message = tmp;
		}
	}
	// AirDC++
	else if ((stricmp(cmd.c_str(), _T("speed")) == 0))
	{
		tstring tmp = _T("My Speed: ") + Text::toT(CompatibilityManager::Speedinfo());
		if (stricmp(param.c_str(), _T("pub")) == 0)
		{
			message = tmp;
		}
		else
		{
			local_message = tmp;
		}
	}
	else if ((stricmp(cmd.c_str(), _T("cpu")) == 0))
	{
		tstring tmp = _T("My CPU: \r\n\t-=[ ") + Text::toT(CompatibilityManager::CPUInfo()) + _T(" ]=-");
		if (stricmp(param.c_str(), _T("pub")) == 0)
		{
			message = tmp;
		}
		else
		{
			local_message = tmp;
		}
	}
	else if ((stricmp(cmd.c_str(), _T("dsp")) == 0))
	{
		tstring tmp = _T("My Disk Space: ") + Text::toT(CompatibilityManager::DiskSpaceInfo());
		if (stricmp(param.c_str(), _T("pub")) == 0)
		{
			message = tmp;
		}
		else
		{
			local_message = tmp;
		}
	}
	else if (stricmp(cmd.c_str(), _T("disks")) == 0 || (stricmp(cmd.c_str(), _T("di")) == 0))       //di
	{
		tstring tmp = _T("My Disks: ") + CompatibilityManager::diskInfo();
		if (stricmp(param.c_str(), _T("pub")) == 0)
		{
			message = tmp;
		}
		else
		{
			local_message = tmp;
		}
	}
	// AirDC++
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	// Lee's /ratio support, why can't he always ask this kind of easy things.
	else if (stricmp(cmd.c_str(), _T("ratio")) == 0 || stricmp(cmd.c_str(), _T("r")) == 0)
	{
		StringMap params;
		CFlylinkDBManager::getInstance()->load_global_ratio();
		params["ratio"] = Text::fromT(CFlylinkDBManager::getInstance()->get_ratioW());
		params["up"] = Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_upload());
		params["down"] = Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_download());
		message = Text::toT(Util::formatParams(SETTING(RATIO_TEMPLATE), params, false));
	}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	else if (stricmp(cmd.c_str(), _T("limit")) == 0)
	{
		MainFrame::getMainFrame()->onLimiter();
		status = BOOLSETTING(THROTTLE_ENABLE) ? TSTRING(LIMITER_ON) : TSTRING(LIMITER_OFF);
		// WMP9+ Support
	}
	else if (stricmp(cmd.c_str(), _T("wmp")) == 0)
	{
		string spam = Players::getWMPSpam(FindWindow(_T("WMPlayerApp"), NULL), g_mainWnd);
		if (!spam.empty())
		{
			if (spam != "no_media")
			{
				message = Text::toT(spam);
			}
			else
			{
				status = TSTRING(WMP_NOT_PLAY);
			}
		}
		else
		{
			status = TSTRING(WMP_NOT_RUN);
		}
	}
	else if (stricmp(cmd.c_str(), _T("itunes")) == 0)
	{
		string spam = Players::getItunesSpam(FindWindow(_T("iTunes"), _T("iTunes")));
		if (!spam.empty())
		{
			if (spam != "no_media")
			{
				message = Text::toT(spam);
			}
			else
			{
				status = TSTRING(ITUNES_NOT_PLAY);
			}
		}
		else
		{
			status = TSTRING(ITUNES_NOT_RUN);
		}
	}
	else if (stricmp(cmd.c_str(), _T("mpc")) == 0)
	{
		string spam = Players::getMPCSpam();
		if (!spam.empty())
		{
			message = Text::toT(spam);
		}
		else
		{
			status = TSTRING(MPC_NOT_RUN);
		}
	}
	else if (stricmp(cmd.c_str(), _T("ja")) == 0)
	{
		string spam = Players::getJASpam();
		if (!spam.empty())
		{
			message = Text::toT(spam);
		}
		else
		{
			status = TSTRING(JA_NOT_RUN);
		}
	}
	// AirDC++
	/*
	    else if ((stricmp(cmd.c_str(), _T("spotify")) == 0) || (stricmp(cmd.c_str(), _T("s")) == 0)) {
	        string spam = Players::getSpotifySpam(FindWindow(_T("SpotifyMainWindow"), NULL));
	        if (!spam.empty()) {
	            if (spam != "no_media") {
	                message = Text::toT(spam);
	            }
	            else {
	                status = _T("You have no media playing in Spotify");
	            }
	        }
	        else {
	            status = _T("Supported version of Spotify is not running");
	        }
	    }
	*/
	else if (stricmp(cmd.c_str(), _T("away")) == 0)
	{
		if (Util::getAway() && param.empty())
		{
			Util::setAway(false);
			MainFrame::setAwayButton(false);
			status = TSTRING(AWAY_MODE_OFF);
		}
		else
		{
			Util::setAway(true);
			MainFrame::setAwayButton(true);
			Util::setAwayMessage(Text::fromT(param));
			
			StringMap sm;
			status = TSTRING(AWAY_MODE_ON) + _T(' ') + Text::toT(Util::getAwayMessage(sm));
		}
	}
	else if (stricmp(cmd.c_str(), _T("u")) == 0)
	{
		if (!param.empty())
		{
			WinUtil::openLink(Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	else if (stricmp(cmd.c_str(), _T("rebuild")) == 0)
	{
		HashManager::getInstance()->rebuild();
	}
	else if (stricmp(cmd.c_str(), _T("shutdown")) == 0)
	{
		MainFrame::setShutDown(!(MainFrame::isShutDown()));
		if (MainFrame::isShutDown())
		{
			status = TSTRING(SHUTDOWN_ON);
		}
		else
		{
			status = TSTRING(SHUTDOWN_OFF);
		}
	}
	else if (stricmp(cmd.c_str(), _T("winamp")) == 0 || stricmp(cmd.c_str(), _T("w")) == 0 || stricmp(cmd.c_str(), _T("f")) == 0 || stricmp(cmd.c_str(), _T("foobar")) == 0 || stricmp(cmd.c_str(), _T("qcd")) == 0 || stricmp(cmd.c_str(), _T("q")) == 0)
	{
		string spam = Players::getWinampSpam(FindWindow(_T("PlayerCanvas"), NULL), 1);
		if (!spam.empty())
		{
			message = Text::toT(spam);
		}
		else
		{
			if (FindWindow(_T("Winamp v1.x"), NULL))
			{
				spam = Players::getWinampSpam(FindWindow(_T("Winamp v1.x"), NULL), 0);
				if (!spam.empty())
				{
					message = Text::toT(spam);
				}
			}
			else if (stricmp(cmd.c_str(), _T("f")) == 0 || stricmp(cmd.c_str(), _T("foobar")) == 0)
			{
				status = TSTRING(FOOBAR_ERROR);
			}
			else if (stricmp(cmd.c_str(), _T("qcd")) == 0 || stricmp(cmd.c_str(), _T("q")) == 0)
			{
				status = TSTRING(QCDQMP_NOT_RUNNING);
			}
			else
			{
				status = TSTRING(WINAMP_NOT_RUNNING);
			}
		}
	}
#ifdef IRAINMAN_ENABLE_MORE_CLIENT_COMMAND
	// Google.
	else if (stricmp(cmd.c_str(), _T("google")) == 0 || stricmp(cmd.c_str(), _T("g")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://www.google.com/search?q=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// Google defination search support.
	else if (stricmp(cmd.c_str(), _T("define")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://www.google.com/search?hl=en&q=define%3A+") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// Yandex.
	else if (stricmp(cmd.c_str(), _T("yandex")) == 0 || stricmp(cmd.c_str(), _T("y")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://yandex.ru/yandsearch?text=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// Yahoo.
	else if (stricmp(cmd.c_str(), _T("yahoo")) == 0 || stricmp(cmd.c_str(), _T("yh")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://search.yahoo.com/search?p=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
		
	}
	// Wikipedia.
	else if (stricmp(cmd.c_str(), _T("wikipedia")) == 0 || stricmp(cmd.c_str(), _T("wiki")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://ru.wikipedia.org/wiki/Special%3ASearch?search=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// IMDB.
	else if (stricmp(cmd.c_str(), _T("imdb")) == 0 || stricmp(cmd.c_str(), _T("i")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://www.imdb.com/find?q=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// КиноПоиск.Ru.
	else if (stricmp(cmd.c_str(), _T("kinopoisk")) == 0 || stricmp(cmd.c_str(), _T("kp")) == 0 || stricmp(cmd.c_str(), _T("k")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://www.kinopoisk.ru/index.php?first=no&kp_query=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// TPB
	else if (stricmp(cmd.c_str(), _T("thepirate")) == 0 || stricmp(cmd.c_str(), _T("tpb")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://thepiratebay.se/search/") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// Rutracker.org.
	else if (stricmp(cmd.c_str(), _T("rutracker")) == 0 || stricmp(cmd.c_str(), _T("rt")) == 0 || stricmp(cmd.c_str(), _T("t")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://rutracker.org/forum/tracker.php?nm=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// В Контакте.
	else if (stricmp(cmd.c_str(), _T("vkontakte")) == 0 || stricmp(cmd.c_str(), _T("vk")) == 0 || stricmp(cmd.c_str(), _T("v")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://vk.com/gsearch.php?q=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// В Контакте. Открываем страницу по id.
	else if (stricmp(cmd.c_str(), _T("vkid")) == 0 || stricmp(cmd.c_str(), _T("vid")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://vk.com/") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// Discogs is a user-built database containing information on artists, labels, and their recordings.
	else if (stricmp(cmd.c_str(), _T("discogs")) == 0 || stricmp(cmd.c_str(), _T("ds")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://www.discogs.com/search?type=all&q=") + Text::toT(Util::encodeURI(Text::fromT(param))) + _T("&btn=Search"));
		}
	}
	// FILExt. To find a description of the file extension / Для поиска описания расширения файла.
	else if (stricmp(cmd.c_str(), _T("filext")) == 0 || stricmp(cmd.c_str(), _T("ext")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://filext.com/file-extension/") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
	// Поиск по блогу FlylinkDC++.
	else if (stricmp(cmd.c_str(), _T("blog")) == 0 || stricmp(cmd.c_str(), _T("b")) == 0)
	{
		if (param.empty())
		{
			status = TSTRING(SPECIFY_SEARCH_STRING);
		}
		else
		{
			WinUtil::openLink(_T("http://www.flylinkdc.ru/search?q=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		}
	}
#endif // IRAINMAN_ENABLE_MORE_CLIENT_COMMAND
	else
	{
		return false;
	}
	
	return true;
}

void WinUtil::copyMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize)
{
	if (!aFile.empty())
	{
		setClipboard(Text::toT(Util::getMagnet(aHash, aFile, aSize)));
	}
}

void WinUtil::searchFile(const string& p_File)
{
	SearchFrame::openWindow(Text::toT(p_File), 0, Search::SIZE_DONTCARE, Search::TYPE_ANY);
}
void WinUtil::searchHash(const TTHValue& aHash)
{
	SearchFrame::openWindow(Text::toT(aHash.toBase32()), 0, Search::SIZE_DONTCARE, Search::TYPE_TTH);
}

void WinUtil::registerDchubHandler()
{
	HKEY hk = nullptr;
	LocalArray<TCHAR, 512> Buf;
	tstring app = _T('\"') + Util::getModuleFileName() + _T("\" /magnet \"%1\"");
	Buf[0] = 0;
	
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\dchub\\Shell\\Open\\Command"), 0, KEY_WRITE | KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD bufLen = Buf.size();
		DWORD type;
		::RegQueryValueEx(hk, NULL, 0, &type, (LPBYTE)Buf.data(), &bufLen);
		::RegCloseKey(hk);
	}
	
	if (stricmp(app.c_str(), Buf.data()) != 0)
	{
		if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\dchub"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
		{
			LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCHUB));
			return;
		}
		
		TCHAR* tmp = _T("URL:Direct Connect Protocol");
		::RegSetValueEx(hk, NULL, 0, REG_SZ, (LPBYTE)tmp, sizeof(TCHAR) * (_tcslen(tmp) + 1));
		::RegSetValueEx(hk, _T("URL Protocol"), 0, REG_SZ, (LPBYTE)_T(""), sizeof(TCHAR));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\dchub\\Shell\\Open\\Command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\dchub\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		app = Util::getModuleFileName();
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
	}
}
static void internalDeleteRegistryKey(const tstring& p_key)
{
	tstring l_key = _T("SOFTWARE\\Classes\\") + p_key;
	if (SHDeleteKey(HKEY_CURRENT_USER, l_key.c_str()) != ERROR_SUCCESS)
	{
		LogManager::message("Erorr Delete key " + Text::fromT(l_key) + " " + Util::translateError());
	}
	
}
void WinUtil::unRegisterDchubHandler()
{
	internalDeleteRegistryKey(_T("dchub"));
}

void WinUtil::registerNMDCSHandler()
{
	HKEY hk = nullptr;
	LocalArray<TCHAR, 512> Buf;
	tstring app = _T('\"') + Util::getModuleFileName() + _T("\" /magnet \"%1\"");
	Buf[0] = 0;
	
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\nmdcs\\Shell\\Open\\Command"), 0, KEY_WRITE | KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD bufLen = Buf.size();
		DWORD type;
		::RegQueryValueEx(hk, NULL, 0, &type, (LPBYTE)Buf.data(), &bufLen);
		::RegCloseKey(hk);
	}
	
	if (stricmp(app.c_str(), Buf.data()) != 0)
	{
		if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\nmdcs"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
		{
			LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCHUB));
			return;
		}
		
		TCHAR* tmp = _T("URL:Direct Connect Protocol");
		::RegSetValueEx(hk, NULL, 0, REG_SZ, (LPBYTE)tmp, sizeof(TCHAR) * (_tcslen(tmp) + 1));
		::RegSetValueEx(hk, _T("URL Protocol"), 0, REG_SZ, (LPBYTE)_T(""), sizeof(TCHAR));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\nmdcs\\Shell\\Open\\Command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\nmdcs\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		app = Util::getModuleFileName();
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
	}
}

void WinUtil::unRegisterNMDCSHandler()
{
	internalDeleteRegistryKey(_T("nmdcs"));
}

void WinUtil::registerADChubHandler()
{
	HKEY hk = nullptr;
	LocalArray<TCHAR, 512> Buf;
	tstring app = _T('\"') + Util::getModuleFileName() + _T("\" /magnet \"%1\"");
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adc\\Shell\\Open\\Command"), 0, KEY_WRITE | KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD bufLen = Buf.size();
		DWORD type;
		::RegQueryValueEx(hk, NULL, 0, &type, (LPBYTE)Buf.data(), &bufLen);
		::RegCloseKey(hk);
	}
	
	if (stricmp(app.c_str(), Buf.data()) != 0)
	{
		if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adc"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
		{
			LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_ADC));
			return;
		}
		
		TCHAR* tmp = _T("URL:Direct Connect Protocol");
		::RegSetValueEx(hk, NULL, 0, REG_SZ, (LPBYTE)tmp, sizeof(TCHAR) * (_tcslen(tmp) + 1));
		::RegSetValueEx(hk, _T("URL Protocol"), 0, REG_SZ, (LPBYTE)_T(""), sizeof(TCHAR));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adc\\Shell\\Open\\Command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adc\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		app = Util::getModuleFileName();
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
	}
}

void WinUtil::unRegisterADChubHandler()
{
	internalDeleteRegistryKey(_T("adc"));
}

void WinUtil::registerADCShubHandler()
{
	HKEY hk = nullptr;
	LocalArray<TCHAR, 512> Buf;
	tstring app = _T('\"') + Util::getModuleFileName() + _T("\" /magnet \"%1\"");
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adcs\\Shell\\Open\\Command"), 0, KEY_WRITE | KEY_READ, &hk) == ERROR_SUCCESS)
	{
		DWORD bufLen = Buf.size();
		DWORD type;
		::RegQueryValueEx(hk, NULL, 0, &type, (LPBYTE)Buf.data(), &bufLen);
		::RegCloseKey(hk);
	}
	
	if (stricmp(app.c_str(), Buf.data()) != 0)
	{
		if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adcs"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
		{
			LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_ADC));
			return;
		}
		
		TCHAR* tmp = _T("URL:Direct Connect Protocol");
		::RegSetValueEx(hk, NULL, 0, REG_SZ, (LPBYTE)tmp, sizeof(TCHAR) * (_tcslen(tmp) + 1));
		::RegSetValueEx(hk, _T("URL Protocol"), 0, REG_SZ, (LPBYTE)_T(""), sizeof(TCHAR));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adcs\\Shell\\Open\\Command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
		
		::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\adcs\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
		app = Util::getModuleFileName();
		::RegSetValueEx(hk, _T(""), 0, REG_SZ, (LPBYTE)app.c_str(), sizeof(TCHAR) * (app.length() + 1));
		::RegCloseKey(hk);
	}
}

void WinUtil::unRegisterADCShubHandler()
{
	internalDeleteRegistryKey(_T("adcs"));
}

void WinUtil::registerMagnetHandler()
{
	HKEY hk = nullptr;
	tstring openCmd;
	tstring appName = Util::getModuleFileName();
	const auto l_comman_key_path = _T("SOFTWARE\\Classes\\magnet\\shell\\open\\command");
	{
		LocalArray<TCHAR, 1024> l_buf;
		l_buf[0] = 0;
		
		// what command is set up to handle magnets right now?
		if (::RegOpenKeyEx(HKEY_CURRENT_USER, l_comman_key_path, 0, KEY_READ, &hk) == ERROR_SUCCESS)
		{
			DWORD l_bufLen = l_buf.size();
			::RegQueryValueEx(hk, NULL, NULL, NULL, (LPBYTE)l_buf.data(), &l_bufLen);
			::RegCloseKey(hk);
		}
		openCmd = l_buf.data();
	}
	
	// (re)register the handler if FlylinkDC.exe isn't the default, or if DC++ is handling it
	if (BOOLSETTING(MAGNET_REGISTER))
	{
		const tstring l_qAppName = _T('\"') + appName + _T("\"");
		if (openCmd.empty() || strnicmp(openCmd, l_qAppName, l_qAppName.size()) != 0)
		{
			internalDeleteRegistryKey(_T("magnet"));
			if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\magnet"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
			{
				LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_MAGNET));
				return;
			}
			const tstring l_MagnetShellDesc = CTSTRING(MAGNET_SHELL_DESC);
			if (!l_MagnetShellDesc.empty())
			{
				::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)l_MagnetShellDesc.c_str(), sizeof(TCHAR) * l_MagnetShellDesc.length() + 1);
			}
			::RegSetValueEx(hk, _T("URL Protocol"), NULL, REG_SZ, NULL, NULL);
			::RegCloseKey(hk);
			::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\magnet\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)appName.c_str(), sizeof(TCHAR) * appName.length() + 1);
			::RegCloseKey(hk);
			appName = l_qAppName + _T(" /magnet \"%1\"");
			::RegCreateKeyEx(HKEY_CURRENT_USER, l_comman_key_path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)appName.c_str(), sizeof(TCHAR) * appName.length() + 1);
			::RegCloseKey(hk);
		}
	}
}

void WinUtil::unRegisterMagnetHandler()
{
	internalDeleteRegistryKey(_T("magnet"));
}

void WinUtil::registerDclstHandler()
{
	// [HKEY_CURRENT_USER\Software\Classes\.dcls]
	// @="DCLST metafile"
	// [HKEY_CURRENT_USER\Software\Classes\.dclst]
	// @="DCLST metafile"
	
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile]
	// @="DCLST metafile download shortcut"
	//
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile\DefaultIcon]
	// @="\"C:\\Program Files\\FlylinkDC++\\FlylinkDC.exe\""
	//
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile\Shell]
	//
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile\Shell\Open]
	//
	// [HKEY_CURRENT_USER\Software\Classes\DCLST metafile\Shell\Open\Command]
	// @="\"C:\\Program Files\\FlylinkDC++\\FlylinkDC\" \"%1\""
	
	
	const tstring l_comman_key_path = _T("SOFTWARE\\Classes\\DCLST metafile\\shell\\open\\command");
	
	HKEY hk = nullptr;
	tstring openCmd;
	tstring appName = Util::getModuleFileName();
	
	{
		LocalArray<TCHAR, 1024> l_buf;
		l_buf[0] = 0;
		
		// what command is set up to handle magnets right now?
		if (::RegOpenKeyEx(HKEY_CURRENT_USER, l_comman_key_path.c_str(), 0, KEY_READ, &hk) == ERROR_SUCCESS)
		{
			DWORD l_bufLen = l_buf.size();
			::RegQueryValueEx(hk, NULL, NULL, NULL, (LPBYTE)l_buf.data(), &l_bufLen);
			::RegCloseKey(hk);
		}
		openCmd = l_buf.data();
	}
	
	// (re)register the handler if FlylinkDC.exe isn't the default, or if DC++ is handling it
	if (BOOLSETTING(DCLST_REGISTER))
	{
		const tstring l_qAppName = _T('\"') + appName + _T("\"");
		if (openCmd.empty() || strnicmp(openCmd, l_qAppName, l_qAppName.size()) != 0)
		{
			// Add Class Ext
			static const tstring dclstMetafile = _T("DCLST metafile");
			if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\.dcls"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
			{
				LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCLST));
				return;
			}
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)dclstMetafile.c_str(), sizeof(TCHAR) * (dclstMetafile.length() + 1));
			::RegCloseKey(hk);
			if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\.dclst"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
			{
				LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCLST));
				return;
			}
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)dclstMetafile.c_str(), sizeof(TCHAR) * (dclstMetafile.length() + 1));
			::RegCloseKey(hk);
			
			
			internalDeleteRegistryKey(_T("DCLST metafile"));
			
			if (::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\DCLST metafile"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
			{
				LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCLST));
				return;
			}
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)CTSTRING(DCLST_SHELL_DESC), sizeof(TCHAR) * TSTRING(MAGNET_SHELL_DESC).length() + 1);
			::RegCloseKey(hk);
			::RegCreateKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Classes\\DCLST metafile\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)appName.c_str(), sizeof(TCHAR) * appName.length() + 1);
			::RegCloseKey(hk);
			appName = l_qAppName + _T(" /open \"%1\"");
			::RegCreateKeyEx(HKEY_CURRENT_USER, l_comman_key_path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hk, NULL);
			::RegSetValueEx(hk, NULL, NULL, REG_SZ, (LPBYTE)appName.c_str(), sizeof(TCHAR) * appName.length() + 1);
			::RegCloseKey(hk);
		}
	}
}

void WinUtil::unRegisterDclstHandler()
{
	internalDeleteRegistryKey(_T("DCLST metafile"));
}

void WinUtil::openFile(const tstring& file)
{
	openFile(file.c_str());
}

void WinUtil::openFile(const TCHAR* file)
{
	::ShellExecute(NULL, _T("open"), file, NULL, NULL, SW_SHOWNORMAL);
}

bool WinUtil::openLink(const tstring& uri)
{

	if (parseMagnetUri(uri) || parseDchubUrl(uri))
	{
		return true;
	}
	else
	{
		static const Tags g_ExtLinks[] =
		{
			EXT_URL_LIST(),
		};
		for (size_t i = 0; i < _countof(g_ExtLinks); ++i)
		{
			if (strnicmp(uri, g_ExtLinks[i].tag, g_ExtLinks[i].tag.size()))
			{
				translateLinkToextProgramm(uri);
				return true;
			}
		}
	}
	return false;
	
}

void WinUtil::translateLinkToextProgramm(const tstring& url, const tstring& p_Extension /*= BaseUtil::emptyStringT*/, const tstring& p_openCmd /* = BaseUtil::emptyStringT*/)
{
	tstring x;
	if (p_openCmd.empty())
	{
		if (p_Extension.empty())
		{
			tstring::size_type i = url.find(_T("://"));
			if (i != string::npos)
			{
				x = url.substr(0, i);
			}
			else
			{
				x = _T("http");
			}
		}
		else
		{
			x = p_Extension;
		}
		x += _T("\\shell\\open\\command");
	}
#ifdef FLYLINKDC_USE_TORRENT
	if (url.find(_T("magnet:?xt=urn:btih")) != tstring::npos)
	{
		DownloadManager::getInstance()->add_torrent_file(_T(""), url);
	}
#endif
	
	::ShellExecute(NULL, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

bool WinUtil::parseDchubUrl(const tstring& aUrl)
{
	if (Util::isDcppHub(aUrl) || Util::isNmdcHub(aUrl))
	{
		uint16_t port;
		string proto, host, file, query, fragment;
		const string l_Url = Util::formatDchubUrl(Text::fromT(aUrl));
		Util::decodeUrl(l_Url, proto, host, port, file, query, fragment);
		const string l_url_rebuild = host + ":" + Util::toString(port);
		if (!host.empty())
		{
		
			RecentHubEntry r;
			r.setServer(l_Url);
			FavoriteManager::getInstance()->addRecent(r);
			
			HubFrame::openHubWindow(false, l_Url);
		}
		if (!file.empty())
		{
			if (file[0] == '/') // Remove any '/' in from of the file
				file = file.substr(1);
				
			string path;
			string nick;
			const string::size_type i = file.find('/', 0);
			if (i != string::npos)
			{
				path = file.substr(i);
				nick = file.substr(0, i);
			}
			if (!nick.empty())
			{
				const UserPtr user = ClientManager::findLegacyUser(nick, l_url_rebuild);
				if (user)
				{
					try
					{
						QueueManager::getInstance()->addList(user, QueueItem::FLAG_CLIENT_VIEW
						                                     , path
						                                    );
					}
					catch (const Exception&)
					{
						// Ignore for now...
					}
				}
				// @todo else report error
			}
		}
		return true;
	}
	return false;
}
bool WinUtil::parseMagnetUri(const tstring& aUrl, DefinedMagnetAction Action /* = MA_DEFAULT */
#ifdef SSA_VIDEO_PREVIEW_FEATURE
                             , bool viewMediaIfPossible /* = false */
#endif
                            )
{
	// official types that are of interest to us
	//  xt = exact topic
	//  xs = exact substitute
	//  as = acceptable substitute
	//  dn = display name
	//  xl = exact length
	//  kt = text for search
	if (Util::isMagnetLink(aUrl))
	{
#ifdef FLYLINKDC_USE_TORRENT
		if (Util::isTorrentLink(aUrl))
		{
			DownloadManager::getInstance()->add_torrent_file(_T(""), aUrl);
		}
		else
#endif
		{
			const string url = Text::fromT(aUrl);
			LogManager::message(STRING(MAGNET_DLG_TITLE) + ": " + url);
			string fname, fhash, type, param;
			
			const StringTokenizer<string> mag(url.substr(8), '&');
			typedef std::unordered_map<string, string> MagMap;
			MagMap hashes;
			
			int64_t fsize = 0;
			int64_t dirsize = 0;
			for (auto idx = mag.getTokens().cbegin(); idx != mag.getTokens().cend(); ++idx)
			{
				// break into pairs
				string::size_type pos = idx->find('=');
				if (pos != string::npos)
				{
					type = Text::toLower(Util::encodeURI(idx->substr(0, pos), true));
					param = Util::encodeURI(idx->substr(pos + 1), true);
				}
				else
				{
					type = Util::encodeURI(*idx, true);
					param.clear();
				}
				// extract what is of value
				if (param.length() == 85 && strnicmp(param.c_str(), "urn:bitprint:", 13) == 0)
				{
					hashes[type] = param.substr(46);
				}
				else if (param.length() == 54 && strnicmp(param.c_str(), "urn:tree:tiger:", 15) == 0)
				{
					hashes[type] = param.substr(15);
				}
				else if (param.length() == 53 && strnicmp(param.c_str(), "urn:tigertree:", 14) == 0)   // used by nextpeer :(
				{
					hashes[type] = param.substr(14);
				}
				else if (param.length() == 55 && strnicmp(param.c_str(), "urn:tree:tiger/:", 16) == 0)
				{
					hashes[type] = param.substr(16);
				}
				else if (param.length() == 59 && strnicmp(param.c_str(), "urn:tree:tiger/1024:", 20) == 0)
				{
					hashes[type] = param.substr(20);
				}
				// Short URN versions
				else if (param.length() == 81 && strnicmp(param.c_str(), "bitprint:", 9) == 0)
				{
					hashes[type] = param.substr(42);
				}
				else if (param.length() == 50 && strnicmp(param.c_str(), "tree:tiger:", 11) == 0)
				{
					hashes[type] = param.substr(11);
				}
				else if (param.length() == 49 && strnicmp(param.c_str(), "tigertree:", 10) == 0)   // used by nextpeer :(
				{
					hashes[type] = param.substr(10);
				}
				else if (param.length() == 51 && strnicmp(param.c_str(), "tree:tiger/:", 12) == 0)
				{
					hashes[type] = param.substr(12);
				}
				else if (param.length() == 55 && strnicmp(param.c_str(), "tree:tiger/1024:", 16) == 0)
				{
					hashes[type] = param.substr(16);
				}
				// File name and size
				else if (type.length() == 2 && strnicmp(type.c_str(), "dn", 2) == 0)
				{
					fname = param;
				}
				else if (type.length() == 2 && strnicmp(type.c_str(), "xl", 2) == 0)
				{
					fsize = Util::toInt64(param);
				}
				else if (type.length() == 2 && strnicmp(type.c_str(), "dl", 2) == 0)
				{
					dirsize = Util::toInt64(param);
				}
				else if (type.length() == 2 && strnicmp(type.c_str(), "kt", 2) == 0)
				{
					fname = param;
				}
				else if (type.length() == 2 && strnicmp(type.c_str(), "xs", 2) == 0)
				{
					WinUtil::parseDchubUrl(Text::toT(param));
				}
#ifdef SSA_VIDEO_PREVIEW_FEATURE
				else if (param.length() == 1 && (strnicmp(type.c_str(), "x.video", 7) == 0  || strnicmp(type.c_str(), "video", 5) == 0))
				{
					viewMediaIfPossible = param.c_str()[0] == L'1';
				}
#endif // SSA_VIDEO_PREVIEW_FEATURE
			}
			// pick the most authoritative hash out of all of them.
			if (hashes.find("as") != hashes.end())
			{
				fhash = hashes["as"];
			}
			if (hashes.find("xs") != hashes.end())
			{
				fhash = hashes["xs"];
			}
			if (hashes.find("xt") != hashes.end())
			{
				fhash = hashes["xt"];
			}
#ifdef SSA_VIDEO_PREVIEW_FEATURE
			const bool isViewMedia = viewMediaIfPossible ? Util::isStreamingVideoFile(fname) : false;
#endif
			const bool l_isDCLST = Util::isDclstFile(fname);
			if (!fhash.empty() && Encoder::isBase32(fhash.c_str()))
			{
				// ok, we have a hash, and maybe a filename.
				
				if (Action == MA_DEFAULT)
				{
					if (fsize <= 0 || fname.empty())
						Action = MA_ASK;
					else
					{
						if (!l_isDCLST)
						{
							if (BOOLSETTING(MAGNET_ASK))
								Action = MA_ASK;
							else
							{
								switch (SETTING(MAGNET_ACTION))
								{
									case SettingsManager::MAGNET_AUTO_DOWNLOAD:
										Action = MA_DOWNLOAD;
										break;
									case SettingsManager::MAGNET_AUTO_SEARCH:
										Action = MA_SEARCH;
										break;
									case SettingsManager::MAGNET_AUTO_DOWNLOAD_AND_OPEN:
										Action = MA_OPEN;
										break;
									default:
										Action = MA_ASK;
										break;
								}
							}
						}
						else
						{
							if (BOOLSETTING(DCLST_ASK))
								Action = MA_ASK;
							else
							{
								switch (SETTING(DCLST_ACTION))
								{
									case SettingsManager::MAGNET_AUTO_DOWNLOAD:
										Action = MA_DOWNLOAD;
										break;
									case SettingsManager::MAGNET_AUTO_SEARCH:
										Action = MA_SEARCH;
										break;
									case SettingsManager::MAGNET_AUTO_DOWNLOAD_AND_OPEN:
										Action = MA_OPEN;
										break;
									default:
										Action = MA_ASK;
										break;
								}
							}
						}
					}
				}
				
				switch (Action)
				{
					case MA_DOWNLOAD:
						try
						{
							QueueManager::getInstance()->add(0, fname, fsize, TTHValue(fhash), HintedUser(),
							                                 l_isDCLST ? QueueItem::FLAG_DCLST_LIST :
#ifdef SSA_VIDEO_PREVIEW_FEATURE
							                                 (isViewMedia ? QueueItem::FLAG_MEDIA_VIEW : 0)
#else
							                                 0
#endif
							                                );
						}
						catch (const Exception& e)
						{
							LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
						}
						break;
						
					case MA_SEARCH:
						SearchFrame::openWindow(Text::toT(fhash), 0, Search::SIZE_DONTCARE, Search::TYPE_TTH);
						break;
					case MA_OPEN:
					{
						try
						{
							QueueManager::getInstance()->add(0, fname, fsize, TTHValue(fhash), HintedUser(), QueueItem::FLAG_CLIENT_VIEW | (l_isDCLST ? QueueItem::FLAG_DCLST_LIST : 0));
						}
						catch (const Exception& e)
						{
							LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
						}
						
					}
					break;
					case MA_ASK:
					{
						MagnetDlg dlg(TTHValue(fhash), Text::toT(Text::toUtf8(fname)), fsize, dirsize, l_isDCLST
#ifdef SSA_VIDEO_PREVIEW_FEATURE
						              , isViewMedia
#endif
						             );
						dlg.DoModal(g_mainWnd);
					}
					break;
				};
			}
			else if (!fname.empty() && fhash.empty())
			{
				SearchFrame::openWindow(Text::toT(fname), fsize, (fsize == 0) ? Search::SIZE_DONTCARE : Search::SIZE_EXACT, Search::TYPE_ANY);
			}
			else
			{
				MessageBox(g_mainWnd, CTSTRING(MAGNET_DLG_TEXT_BAD), CTSTRING(MAGNET_DLG_TITLE), MB_OK | MB_ICONEXCLAMATION);
			}
		}
		return true;
	}
	return false;
}

void WinUtil::OpenFileList(const tstring& filename, DefinedMagnetAction Action /* = MA_DEFAULT */)
{
	const UserPtr u = DirectoryListing::getUserFromFilename(Text::fromT(filename));
	DirectoryListingFrame::openWindow(filename, BaseUtil::emptyStringT, HintedUser(u, BaseUtil::emptyString), 0, Util::isDclstFile(filename));
}

int WinUtil::textUnderCursor(POINT p, CEdit& ctrl, tstring& x)
{
	const int i = ctrl.CharFromPos(p);
	const int line = ctrl.LineFromChar(i);
	const int c = LOWORD(i) - ctrl.LineIndex(line);
	const int len = ctrl.LineLength(i) + 1;
	if (len < 3)
	{
		return 0;
	}
	
	x.resize(len + 1);
	x.resize(ctrl.GetLine(line, &x[0], len + 1));
	
	string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);
	if (start == string::npos)
		start = 0;
	else
		start++;
		
	return start;
}
bool WinUtil::parseDBLClick(const tstring& aString, string::size_type start, string::size_type end)
{
	const tstring l_URI = aString.substr(start, end - start);
	return openLink(l_URI);
}

void WinUtil::saveHeaderOrder(CListViewCtrl& ctrl, SettingsManager::StrSetting order,
                              SettingsManager::StrSetting widths, int n,
                              int* indexes, int* sizes) noexcept
{
	string tmp;
	
	ctrl.GetColumnOrderArray(n, indexes);
	int i;
	for (i = 0; i < n; ++i)
	{
		tmp += Util::toString(indexes[i]);
		tmp += ',';
	}
	tmp.erase(tmp.size() - 1, 1);
	SettingsManager::set(order, tmp);
	tmp.clear();
	const int nHeaderItemsCount = ctrl.GetHeader().GetItemCount();
	for (i = 0; i < n; ++i)
	{
		sizes[i] = ctrl.GetColumnWidth(i);
		if (i >= nHeaderItemsCount) // Not exist column
			sizes[i] = 0;
		tmp += Util::toString(sizes[i]);
		tmp += ',';
	}
	tmp.erase(tmp.size() - 1, 1);
	SettingsManager::set(widths, tmp);
}

string FileImage::getVirusIconIndex(const string& aFileName, int& p_icon_index)
{
	p_icon_index = 0;
	auto x = Text::toLower(Util::getFileExtWithoutDot(aFileName)); //TODO часто зовем
	if (x.compare(0, 3, "exe", 3) == 0)
	{
		// Проверка на двойные расширения
		string xx = Util::getFileName(aFileName);
		xx = Text::toLower(Util::getFileDoubleExtWithoutDot(xx));
		bool is_virus = (aFileName.size() > 13 && stricmp(aFileName.c_str() + aFileName.size() - 11, " dvdrip.exe") == 0);
		if (!xx.empty() || is_virus)
		{
			if (is_virus || CFlyServerConfig::isVirusExt(xx))
			{
				static int g_virus_exe_icon_index = 0;
				if (!g_virus_exe_icon_index)
				{
					m_images.AddIcon(WinUtil::g_hThermometerIcon);
					m_imageCount++;
					g_virus_exe_icon_index = m_imageCount - 1;
				}
				p_icon_index = g_virus_exe_icon_index;
				return x;
			}
			// Проверим медиа-расширение.exe
			const auto i = xx.rfind('.');
			dcassert(i != string::npos);
			if (i != string::npos)
			{
				const auto base_x = xx.substr(0, i);
				if (CFlyServerConfig::isMediainfoExt(base_x))
				{
					static int g_media_virus_exe_icon_index = 0;
					if (!g_media_virus_exe_icon_index)
					{
						m_images.AddIcon(WinUtil::g_hMedicalIcon);
						m_imageCount++;
						g_media_virus_exe_icon_index = m_imageCount - 1;
					}
					p_icon_index = g_media_virus_exe_icon_index; // g_virus_exe_icon_index;
					return x;
				}
			}
		}
	}
	return x;
}

int FileImage::getIconIndex(const string& aFileName)
{
	int p_icon_index = 0;
	string x = getVirusIconIndex(aFileName, p_icon_index);
	if (p_icon_index)
		return p_icon_index;
	if (BOOLSETTING(USE_SYSTEM_ICONS))
	{
#ifndef _DEBUG // В отладке тупит
		if (!x.empty())
		{
			//CFlyFastLock(m_cs);
			const auto j = m_indexis.find(x);
			if (j != m_indexis.end())
			{
				return j->second;
			}
		}
		x = string("x.") + x;
		SHFILEINFO fi = { 0 };
		if (SHGetFileInfo(Text::toT(x).c_str(), FILE_ATTRIBUTE_NORMAL, &fi, sizeof(fi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
		{
			//CFlyFastLock(m_cs);
			m_images.AddIcon(fi.hIcon);
			::DestroyIcon(fi.hIcon);
			m_indexis[x] = m_imageCount++;
			return m_imageCount - 1;
		}
		else
		{
			return DIR_FILE;
		}
#else
		return DIR_FILE;
#endif
	}
	else
	{
		return DIR_FILE;
	}
}

void FileImage::init()
{
#ifdef _DEBUG
	dcassert(m_imageCount == -1);
#endif
	/** @todo fix this so that the system icon is used for dirs as well (we need
	to mask it so that incomplete folders appear correct */
#if 0
	if (BOOLSETTING(USE_SYSTEM_ICONS))
	{
		SHFILEINFO fi = {0};
		g_fileImages.Create(16, 16, ILC_COLOR32 | ILC_MASK, 16, 16);
		::SHGetFileInfo(_T("."), FILE_ATTRIBUTE_DIRECTORY, &fi, sizeof(fi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
		g_fileImages.AddIcon(fi.hIcon);
		g_fileImages.AddIcon(ic);
		::DestroyIcon(fi.hIcon);
	}
	else
	{
		ResourceLoader::LoadImageList(IDR_FOLDERS, fileImages, 16, 16);
	}
#endif
	ResourceLoader::LoadImageList(IDR_FOLDERS, m_images, 16, 16, false);
	m_imageCount = DIR_IMAGE_LAST;
	dcassert(m_images.GetImageCount() == DIR_IMAGE_LAST);
}

double WinUtil::toBytes(TCHAR* aSize)
{
	double bytes = _tstof(aSize);
	
	if (_tcsstr(aSize, CTSTRING(YB)))
	{
		return bytes * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
	}
	else if (_tcsstr(aSize, CTSTRING(ZB)))
	{
		return bytes * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
	}
	else if (_tcsstr(aSize, CTSTRING(EB)))
	{
		return bytes * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
	}
	else if (_tcsstr(aSize, CTSTRING(PB)))
	{
		return bytes * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
	}
	else if (_tcsstr(aSize, CTSTRING(TB)))
	{
		return bytes * 1024.0 * 1024.0 * 1024.0 * 1024.0;
	}
	else if (_tcsstr(aSize, CTSTRING(GB)))
	{
		return bytes * 1024.0 * 1024.0 * 1024.0;
	}
	else if (_tcsstr(aSize, CTSTRING(MB)))
	{
		return bytes * 1024.0 * 1024.0;
	}
	else if (_tcsstr(aSize, CTSTRING(KB)))
	{
		return bytes * 1024.0;
	}
	else
	{
		return bytes;
	}
}

static pair<tstring, bool> formatHubNames(const StringList& hubs)
{
	if (hubs.empty())
	{
		return pair<tstring, bool>(TSTRING(OFFLINE), false);
	}
	else
	{
		return pair<tstring, bool>(Text::toT(Util::toString(hubs)), true);
	}
}

pair<tstring, bool> WinUtil::getHubNames(const CID& cid, const string& hintUrl)
{
	return formatHubNames(ClientManager::getHubNames(cid, hintUrl));
}

pair<tstring, bool> WinUtil::getHubNames(const UserPtr& u, const string& hintUrl)
{
	return getHubNames(u->getCID(), hintUrl);
}

pair<tstring, bool> WinUtil::getHubNames(const CID& cid, const string& hintUrl, bool priv)
{
	return formatHubNames(ClientManager::getHubNames(cid, hintUrl, priv));
}

pair<tstring, bool> WinUtil::getHubNames(const HintedUser& user)
{
	return getHubNames(user.user, user.hint);
}

void WinUtil::getContextMenuPos(CListViewCtrl& aList, POINT& aPt)
{
	int pos = aList.GetNextItem(-1, LVNI_SELECTED | LVNI_FOCUSED);
	if (pos >= 0)
	{
		CRect lrc;
		aList.GetItemRect(pos, &lrc, LVIR_LABEL);
		aPt.x = lrc.left;
		aPt.y = lrc.top + (lrc.Height() / 2);
	}
	else
	{
		aPt.x = aPt.y = 0;
	}
	aList.ClientToScreen(&aPt);
}

void WinUtil::getContextMenuPos(CTreeViewCtrl& aTree, POINT& aPt)
{
	CRect trc;
	HTREEITEM ht = aTree.GetSelectedItem();
	if (ht)
	{
		aTree.GetItemRect(ht, &trc, TRUE);
		aPt.x = trc.left;
		aPt.y = trc.top + (trc.Height() / 2);
	}
	else
	{
		aPt.x = aPt.y = 0;
	}
	aTree.ClientToScreen(&aPt);
}
void WinUtil::getContextMenuPos(CEdit& aEdit, POINT& aPt)
{
	CRect erc;
	aEdit.GetRect(&erc);
	aPt.x = erc.Width() / 2;
	aPt.y = erc.Height() / 2;
	aEdit.ClientToScreen(&aPt);
}

void WinUtil::openFolder(const tstring& file)
{
	/* TODO: needs test in last wine!
	if (CompatibilityManager::isWine())
	    ::ShellExecute(NULL, NULL, Text::toT(Util::getFilePath(ii->entry->getTarget())).c_str(), NULL, NULL, SW_SHOWNORMAL);
	*/
	if (File::isExist(file))
		::ShellExecute(NULL, NULL, _T("explorer.exe"), (_T("/e, /select, \"") + file + _T('\"')).c_str(), NULL, SW_SHOWNORMAL);
	else
		::ShellExecute(NULL, NULL, _T("explorer.exe"), (_T("/e, \"") + Util::getFilePath(file) + _T('\"')).c_str(), NULL, SW_SHOWNORMAL);
}

void WinUtil::openLog(const string& dir, const StringMap& params, const tstring& nologmessage)
{
	const auto file = Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatParams(dir, params, false)));
	if (File::isExist(file))
	{
		WinUtil::openFile(file);
	}
	else
	{
		const TCHAR* nlm = nologmessage.c_str();
		MessageBox(nullptr, nlm, nlm, MB_OK);
	}
}

void Preview::setupPreviewMenu(const string& target)
{
	dcassert(g_previewAppsSize == 0);
	g_previewAppsSize = 0;
	
	const auto targetLower = Text::toLower(target);
	
	const auto& lst = FavoriteManager::getPreviewApps();
	for (auto i = lst.cbegin(); i != lst.cend(); ++i)
	{
		const auto tok = Util::splitSettingAndLower((*i)->getExtension());
		if (tok.empty())
		{
			g_previewMenu.AppendMenu(MF_STRING, IDC_PREVIEW_APP + g_previewAppsSize, Text::toT((*i)->getName()).c_str());
			g_previewAppsSize++;
		}
		else
			for (auto si = tok.cbegin(); si != tok.cend(); ++si)
			{
				if (Util::isSameFileExt(targetLower, *si))
				{
					g_previewMenu.AppendMenu(MF_STRING, IDC_PREVIEW_APP + g_previewAppsSize, Text::toT((*i)->getName()).c_str());
					g_previewAppsSize++;
					break;
				}
			}
	}
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	if (Util::isStreamingVideoFile(targetLower))
	{
		g_previewMenu.AppendMenu(MF_STRING, IDC_PREVIEW_APP_INT, CTSTRING(INTERNAL_PREVIEW));
		g_previewAppsSize++;
	}
#endif
}

void Preview::runPreviewCommand(WORD wID, string file)
{
	wID -= IDC_PREVIEW_APP;
	const auto& lst = FavoriteManager::getPreviewApps();
	if (wID <= lst.size())
	{
		const auto& application = lst[wID]->getApplication();
		const auto& arguments = lst[wID]->getArguments();
		StringMap fileParams;
		
		auto dir = Util::getFilePath(file);
		AppendQuotsToPath(dir);
		fileParams["dir"] = dir;
		
		AppendQuotsToPath(file);
		fileParams["file"] = file;
		
		::ShellExecute(NULL, NULL, Text::toT(application).c_str(), Text::toT(Util::formatParams(arguments, fileParams, false)).c_str(), Text::toT(dir).c_str(), SW_SHOWNORMAL);
	}
}

int WinUtil::getFlagIndexByName(const char* countryName)
{
	// country codes are not sorted, use linear searching (it is not used so often)
	for (size_t i = 0; i < _countof(countryNames); ++i)
		if (_stricmp(countryName, countryNames[i]) == 0)
			return i + 1;
	return 0;
}

bool WinUtil::shutDown(int action)
{
	UINT iForceIfHung = 0x00000010;
	HANDLE hToken;
	OpenProcessToken(GetCurrentProcess(), (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY), &hToken);
	
	LUID luid;
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid);
	
	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tp.Privileges[0].Luid = luid;
	AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);
	CloseHandle(hToken);
	
	// Shutdown
	switch (action)
	{
		case 0:
		{
			action = EWX_POWEROFF;
			break;
		}
		case 1:
		{
			action = EWX_LOGOFF;
			break;
		}
		case 2:
		{
			action = EWX_REBOOT;
			break;
		}
		case 3:
		{
			SetSuspendState(false, false, false);
			return true;
		}
		case 4:
		{
			SetSuspendState(true, false, false);
			return true;
		}
		case 5:
		{
			typedef bool (CALLBACK * LPLockWorkStation)(void);
			LPLockWorkStation _d_LockWorkStation = (LPLockWorkStation)GetProcAddress(LoadLibrary(_T("user32")), "LockWorkStation");
			if (_d_LockWorkStation)
			{
				_d_LockWorkStation();
			}
			return true;
		}
	}
	
	if (ExitWindowsEx(action | iForceIfHung, 0) == 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

int WinUtil::setButtonPressed(int nID, bool bPressed /* = true */)
{
	if (nID == -1 || !MainFrame::getMainFrame()->getToolBar().IsWindow())
		return -1;
		
	MainFrame::getMainFrame()->getToolBar().CheckButton(nID, bPressed);
	return 0;
}


void WinUtil::activateMDIChild(HWND hWnd)
{
	::SendMessage(g_mdiClient, WM_SETREDRAW, FALSE, 0);
	::SendMessage(g_mdiClient, WM_MDIACTIVATE, (WPARAM)hWnd, 0);
	::SendMessage(g_mdiClient, WM_SETREDRAW, TRUE, 0);
	::RedrawWindow(g_mdiClient, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

#ifdef FLYLINKDC_USE_LIST_VIEW_WATER_MARK

bool WinUtil::setListCtrlWatermark(HWND hListCtrl, UINT nID, COLORREF clr, int width /*= 128*/, int height /*= 128*/)
{
	// Only version 6 and up can do watermarks
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (CompatibilityManager::getComCtlVersion() < MAKELONG(0, 6))
		return false;
		
		
	// Despite documentation LVBKIF_FLAG_ALPHABLEND works only with version 6.1 and up
	const bool supportsAlpha = CompatibilityManager::getComCtlVersion() >= MAKELONG(1, 6);
	
	// If there already is a watermark with alpha channel, stop here...
	if (supportsAlpha)
#endif // FLYLINKDC_SUPPORT_WIN_XP
	{
		LVBKIMAGE lvbk;
		lvbk.ulFlags = LVBKIF_TYPE_WATERMARK;
		ListView_GetBkImage(hListCtrl, &lvbk);
		if (lvbk.hbm != NULL) return false;
	}
	
	ExCImage image;
	if (!image.LoadFromResource(nID, _T("PNG")))
		return false;
		
	HBITMAP bmp = nullptr;
	const HDC screen_dev = ::GetDC(hListCtrl);
	
	if (screen_dev)
	{
		// Create a compatible DC
		const HDC dst_hdc = ::CreateCompatibleDC(screen_dev);
		if (dst_hdc)
		{
			// Create a new bitmap of icon size
			bmp = ::CreateCompatibleBitmap(screen_dev, width, height);
			if (bmp)
			{
				// Select it into the compatible DC
				HBITMAP old_dst_bmp = (HBITMAP)::SelectObject(dst_hdc, bmp);
#ifdef FLYLINKDC_SUPPORT_WIN_XP
				// Fill the background of the compatible DC with the given color
				if (!supportsAlpha)
				{
					::SetBkColor(dst_hdc, clr);
					RECT rc = { 0, 0, width, height };
					::ExtTextOut(dst_hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL); // TODO - тут выводится пустота?
				}
#endif // FLYLINKDC_SUPPORT_WIN_XP
				// Draw the icon into the compatible DC
				image.Draw(dst_hdc, 0, 0, width, height);
				::SelectObject(dst_hdc, old_dst_bmp);
			}
			::DeleteDC(dst_hdc);
		}
	}
	
	// Free stuff
	int l_res = ::ReleaseDC(hListCtrl, screen_dev);
	dcassert(l_res);
	image.Destroy();
	
	if (!bmp)
		return false;
		
	LVBKIMAGE lv = {0};
	lv.ulFlags = LVBKIF_TYPE_WATERMARK |
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	             (supportsAlpha ? LVBKIF_FLAG_ALPHABLEND : 0)
#else
	             LVBKIF_FLAG_ALPHABLEND
#endif
	             ;
	lv.hbm = bmp;
	lv.xOffsetPercent = 100;
	lv.yOffsetPercent = 100;
	ListView_SetBkImage(hListCtrl, &lv);
	return true;
}
#endif

/*
bool WinUtil::checkIsButtonPressed(int nID)
{
    if (nID == -1)
        return false;

    LPTBBUTTONINFO tbi = new TBBUTTONINFO;
//  tbi->fsState = TBSTATE_CHECKED;
    if (MainFrame::getMainFrame()->getToolBar().GetButtonInfo(nID, tbi) == -1)
    {
        delete tbi;
        return false;
    }
    else
    {
        delete tbi;
        return true;
    }
}
*/

tstring WinUtil::getNicks(const CID& cid, const string& hintUrl)
{
	const auto l_nicks = ClientManager::getNicks(cid, hintUrl);
	if (l_nicks.empty())
		return BaseUtil::emptyStringT;
	else
		return Text::toT(Util::toString(l_nicks));
}

tstring WinUtil::getNicks(const UserPtr& u, const string& hintUrl)
{
	dcassert(u);
	if (u)
	{
		return getNicks(u->getCID(), hintUrl);
	}
	else
		return BaseUtil::emptyStringT;
}

tstring WinUtil::getNicks(const CID& cid, const string& hintUrl, bool priv)
{
	const auto l_nicks = ClientManager::getNicks(cid, hintUrl, priv);
	if (l_nicks.empty())
		return BaseUtil::emptyStringT;
	else
		return Text::toT(Util::toString(l_nicks));
}

tstring WinUtil::getNicks(const HintedUser& user)
{
	return getNicks(user.user, user.hint);
}

#ifdef FLYLINKDC_USE_PROVIDER_RESOURCES
#ifdef FLYLINKDC_USE_CUSTOM_MENU
bool WinUtil::FillCustomMenu(CMenuHandle &menu, string& menuName)
{
	const CustomMenuManager* l_instance = CustomMenuManager::getInstance();
	if (!l_instance)
		return false;
		
	const auto& l_MenuList = l_instance->getMenuList();
	if (l_MenuList.empty())
		return false;
		
	BOOL bRet = FALSE;
	if (!menu.m_hMenu)
		menu.CreatePopupMenu();
		
	menuName = l_instance->getTitle();
	
	CMenuHandle currentMenu = menu;
	deque<CMenuHandle> menuHandels;
	menuHandels.clear();
	
	const size_t l_count = l_MenuList.size();
	for (size_t i = 0; i < l_count; i++)
	{
		const CustomMenuItem& item = l_MenuList[i];
		switch (item.getType())
		{
			case 1: // Add Item to current menu;
			{
				bRet |= currentMenu.AppendMenu(MF_STRING, IDC_CUSTOM_MENU + item.getID(), Text::toT(item.getTitle()).c_str());
			}
			break;
			case 0: // Add Separator
			{
				bRet |= currentMenu.AppendMenu(MF_SEPARATOR);
			}
			break;
			case 2:
			{
				menuHandels.push_back(currentMenu);
				CMenuHandle subMenu;
				subMenu.CreatePopupMenu();
				currentMenu = subMenu;
			}
			break;
			case 3:
			{
				CMenuHandle lastParent = menuHandels.back();
				menuHandels.pop_back();
				lastParent.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)currentMenu, Text::toT(item.getTitle()).c_str());
				currentMenu = lastParent;
			}
			break;
			default:
				dcassert(0);
		};
	}
	
	return bRet != FALSE;
}
#endif
#endif // FLYLINKDC_USE_PROVIDER_RESOURCES



tstring WinUtil::getAddresses(CComboBox& BindCombo)
{
	std::unordered_set<string> l_unique_ip;
	tstring l_result_tool_tip;
	BindCombo.AddString(_T("0.0.0.0"));
	l_unique_ip.insert("0.0.0.0");
	IP_ADAPTER_INFO* AdapterInfo = nullptr;
	DWORD dwBufLen = NULL;
	
	DWORD dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);
	if (dwStatus == ERROR_BUFFER_OVERFLOW)
	{
		AdapterInfo = (IP_ADAPTER_INFO*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufLen);
		dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);
	}
	
	if (dwStatus == ERROR_SUCCESS)
	{
		PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;
		while (pAdapterInfo)
		{
			IP_ADDR_STRING* pIpList = &pAdapterInfo->IpAddressList;
			while (pIpList)
			{
				const string l_ip_and_desc = pIpList->IpAddress.String;
				l_result_tool_tip += Text::toT(l_ip_and_desc) + _T("\t");
				l_result_tool_tip += Text::toT(pAdapterInfo->Description);
				l_result_tool_tip += _T("\r\n");
				const auto l_res = l_unique_ip.insert(l_ip_and_desc);
				if (l_res.second == true)
				{
					if (Util::isPrivateIp(l_ip_and_desc))
					{
						BindCombo.AddString(Text::toT(l_ip_and_desc).c_str());
					}
					else
					{
						dcassert(0);
					}
				}
				else
				{
					//dcassert(0);
				}
				pIpList = pIpList->Next;
			}
			pAdapterInfo = pAdapterInfo->Next;
		}
	}
//+BugMaster: memory leak
	if (AdapterInfo)
		HeapFree(GetProcessHeap(), 0, AdapterInfo);
//-BugMaster: memory leak
	return l_result_tool_tip;
}


void WinUtil::GetTimeValues(CComboBox& p_ComboBox)
{
	const bool use12hrsFormat = BOOLSETTING(USE_12_HOUR_FORMAT);
	p_ComboBox.AddString(CTSTRING(MIDNIGHT));
	
	if (use12hrsFormat)
		for (int i = 1; i < 12; ++i)
			p_ComboBox.AddString((Util::toStringW(i) + _T(" AM")).c_str());
			
	else
		for (int i = 1; i < 12; ++i)
			p_ComboBox.AddString((Util::toStringW(i) + _T(":00")).c_str());
			
	p_ComboBox.AddString(CTSTRING(NOON));
	
	if (use12hrsFormat)
		for (int i = 1; i < 12; ++i)
			p_ComboBox.AddString((Util::toStringW(i) + _T(" PM")).c_str());
			
	else
		for (int i = 13; i < 24; ++i)
			p_ComboBox.AddString((Util::toStringW(i) + _T(":00")).c_str());
}

TCHAR WinUtil::CharTranscode(const TCHAR msg)
{

	static const TCHAR Lat[] = L"`qwertyuiop[]asdfghjkl;'zxcvbnm,./~!@#$%^&*()_+|QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>?";
	static const TCHAR Rus[] = L"ёйцукенгшщзхъфывапролджэячсмитьбю.Ё!\"№;%:?*()_+/ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ,";
	for (size_t i = 0; i < _countof(Lat); i++)
	{
		if (msg == Lat[i])
			return Rus[i];
		else if (msg == Rus[i])
			return Lat[i];
	}
	return msg;
}

void WinUtil::TextTranscode(CEdit& ctrlMessage)
{
	const int len1 = static_cast<int>(ctrlMessage.GetWindowTextLength());
	if (len1 == 0)
		return;
	AutoArray<TCHAR> message(len1 + 1);
	ctrlMessage.GetWindowText(message, len1 + 1);
	
	int nStartSel = 0;
	int nEndSel = 0;
	ctrlMessage.GetSel(nStartSel, nEndSel);
	for (int i = 0; i < len1; i++)
	{
		if (nStartSel >= nEndSel ||
		        (i >= nStartSel && i < nEndSel))
		{
			message[i] = CharTranscode(message[i]);
		}
	}
	ctrlMessage.SetWindowText(message);
	//ctrlMessage.SetSel(ctrlMessage.GetWindowTextLength(), ctrlMessage.GetWindowTextLength());
	ctrlMessage.SetSel(nStartSel, nEndSel);
	ctrlMessage.SetFocus();
}

void WinUtil::SetBBCodeForCEdit(CEdit& ctrlMessage, WORD wID)
{
#ifdef IRAINMAN_USE_BB_CODES
#ifdef SCALOLAZ_BB_COLOR_BUTTON
	tstring startTag;
	tstring  endTag;
#else // SCALOLAZ_BB_COLOR_BUTTON
	TCHAR* startTag = nullptr;
	TCHAR* endTag = nullptr;
#endif // SCALOLAZ_BB_COLOR_BUTTON
	switch (wID)
	{
		case IDC_BOLD:
			startTag = _T("[b]");
			endTag = _T("[/b]");
			break;
		case IDC_ITALIC:
			startTag = _T("[i]");
			endTag = _T("[/i]");
			break;
		case IDC_UNDERLINE:
			startTag = _T("[u]");
			endTag = _T("[/u]");
			break;
		case IDC_STRIKE:
			startTag = _T("[s]");
			endTag = _T("[/s]");
			break;
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		case IDC_COLOR:
		{
			CColorDialog dlg(SETTING(TEXT_GENERAL_FORE_COLOR), 0, ctrlMessage.m_hWnd /*mainWnd*/);
			if (dlg.DoModal(ctrlMessage.m_hWnd) == IDOK)
			{
				const string hexString = RGB2HTMLHEX(dlg.GetColor());
				tstring tcolor = _T("[color=#") + (Text::toT(hexString)) + _T("]");
				startTag = tcolor;
				endTag = _T("[/color]");
			}
			break;
		}
#endif // SCALOLAZ_BB_COLOR_BUTTON
		
		//  case IDC_WIKI:
		//      startTag = _T("[ruwiki]");
		//      endTag = _T("[/ruwiki]");
		//      break;
	
		default:
			dcassert(0);
	}
	
	tstring setString;
	int nStartSel = 0;
	int nEndSel = 0;
	ctrlMessage.GetSel(nStartSel, nEndSel);
	tstring s;
	WinUtil::GetWindowText(s, ctrlMessage);
	tstring startString = s.substr(0, nStartSel);
	tstring middleString = s.substr(nStartSel, nEndSel - nStartSel);
	tstring endString = s.substr(nEndSel, s.length() - nEndSel);
	setString = startString;
	
	if ((nEndSel - nStartSel) > 0) //Что-то выделено, обрамляем тэгом, курсор ставим в конце
	{
		setString += startTag;
		setString += middleString;
		setString += endTag;
		setString += endString;
		if (!setString.empty())
		{
			ctrlMessage.SetWindowText(setString.c_str());
			int newPosition = setString.length() - endString.length();
			ctrlMessage.SetSel(newPosition, newPosition);
		}
	}
	else    // Ничего не выбрано, ставим тэги, курсор между ними
	{
		setString += startTag;
		const int newPosition = setString.length();
		setString += endTag;
		setString += endString;
		if (!setString.empty())
		{
			ctrlMessage.SetWindowText(setString.c_str());
			ctrlMessage.SetSel(newPosition, newPosition);
		}
	}
	ctrlMessage.SetFocus();
#endif
	
}

BOOL
WinUtil::FillRichEditFromString(HWND hwndEditControl, const string& rtfFile)
{
	// https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=88785
	// Посомтреть дампы и починить
	// TODO please refactoring this function to use unicode!
	if (hwndEditControl == NULL)
		return FALSE;
		
	userStreamIterator it;
	it.data = unique_ptr<uint8_t[]>(new uint8_t[rtfFile.length() + 1]);
	memcpy(it.data.get(), rtfFile.c_str(), rtfFile.length());
	it.data.get()[rtfFile.length()] = 0;
	it.length = rtfFile.length();
	
	EDITSTREAM es = { 0 };
	es.pfnCallback = EditStreamCallback;
	es.dwCookie = (DWORD_PTR)(&it);
	if (SendMessage(hwndEditControl, EM_STREAMIN,  SF_RTF | SFF_SELECTION, (LPARAM)&es)  && es.dwError == 0)
	{
		return TRUE;
	}
	
	return FALSE;
}

DWORD CALLBACK WinUtil::EditStreamCallback(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb)
{
	userStreamIterator *it = (userStreamIterator*)dwCookie;
	if (it->position >= it->length) return (DWORD) - 1;
	*pcb = cb > (it->length - it->position) ? (it->length - it->position) : cb;
	memcpy(lpBuff, it->data.get() + it->position, *pcb);
	it->position += *pcb;
	return 0;
}

tstring WinUtil::getFilenameFromString(const tstring& filename)
{
	tstring strRet;
	tstring::size_type i = 0;
	for (; i < filename.length(); i++)
	{
		const tstring strtest = filename.substr(i, 1);
		const wchar_t* testLabel = strtest.c_str();
		if (testLabel[0] != _T(' ') && testLabel[0] != _T('\"'))
			break;
	}
	if (i < filename.length())
	{
		strRet = filename.substr(i);
		
		tstring::size_type j2Comma = strRet.find(_T('\"'), 0);
		if (j2Comma != string::npos && j2Comma > 0)
			strRet = strRet.substr(0, j2Comma);
			
		//tstring::size_type jSpace = strRet.find(_T(' '), 0);
		//if (jSpace != tstring::npos && jSpace > 0)
		//  strRet = strRet.substr(0, jSpace - 1);
	}
	
	return strRet;
}

#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
LRESULT Colors::alternationonCustomDraw(LPNMHDR pnmh, BOOL& bHandled)
{
	if (!BOOLSETTING(USE_CUSTOM_LIST_BACKGROUND))
	{
		bHandled = FALSE;
		return 0;
	}
	
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			alternationBkColorAlways(cd);
			return CDRF_DODEFAULT;
		}
		
		default:
			return CDRF_DODEFAULT;
	}
}
#endif

int UserInfoGuiTraits::getCtrlIdBySpeedLimit(const int limit)
{
	int id = IDC_SPEED_NORMAL;
	switch (limit)
	{
		case FavoriteUser::UL_SU:
			id = IDC_SPEED_SUPER;
			break;
		case FavoriteUser::UL_BAN:
			id = IDC_SPEED_BAN;
			break;
		case FavoriteUser::UL_NONE:
			id = IDC_SPEED_NORMAL;
			break;
		case 128:
			id = IDC_SPEED_128K;
			break;
		case 512:
			id = IDC_SPEED_512K;
			break;
		case 1024:
			id = IDC_SPEED_1024K;
			break;
		case 2048:
			id = IDC_SPEED_2048K;
			break;
		case 4096:
			id = IDC_SPEED_4096K;
			break;
		case 10240:
			id = IDC_SPEED_10240K;
			break;
		default:
			id = IDC_SPEED_MANUAL;
			break;
	}
	
	return id;
}

int UserInfoGuiTraits::getSpeedLimitByCtrlId(WORD wID, int lim)
{
	switch (wID)
	{
		case IDC_SPEED_NORMAL:
			lim = FavoriteUser::UL_NONE;
			break;
		case IDC_SPEED_SUPER:
			lim = FavoriteUser::UL_SU;
			break;
		case IDC_SPEED_BAN:
			lim = FavoriteUser::UL_BAN;
			break;
		case IDC_SPEED_128K:
			lim = 128;
			break;
		case IDC_SPEED_512K:
			lim = 512;
			break;
		case IDC_SPEED_1024K:
			lim = 1024;
			break;
		case IDC_SPEED_2048K:
			lim = 2048;
			break;
		case IDC_SPEED_4096K:
			lim = 4096;
			break;
		case IDC_SPEED_10240K:
			lim = 10240;
			break;
		case IDC_SPEED_MANUAL:
		{
			if (lim < 0)
				lim = 0;
			LimitEditDlg dlg(lim);
			if (dlg.DoModal() == IDOK)
			{
				lim = dlg.GetLimit();
			}
		}
		break;
		default:
			dcassert(0);
	}
	
	return lim;
}
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
int VideoImage::getMediaVideoIcon(const tstring& p_col)
{
//    PROFILE_THREAD_SCOPED()
	int l_size_result = -1;
	if (!p_col.empty())
	{
		const auto l_pos = p_col.find(_T('x'));
		if (l_pos != tstring::npos)
		{
			const int x_size = _wtoi(p_col.c_str());
			const int y_size = _wtoi(p_col.c_str() + l_pos + 1);
			
			// Uncomment this, if you want open all video size formats
			if (x_size >= 200 && x_size <= 640 && y_size >= 140 && y_size <= 480)
				l_size_result = 3; //Mobile or VHS
			else if (x_size >= 640 && x_size <= 1080 && y_size >= 240 && y_size <= 720)
				l_size_result = 2; //SD
				
			if (x_size >= 1080 && x_size <= 1920 && y_size >= 500 && y_size <= 1080)
				l_size_result = 1; //HD
				
			if (x_size >= 1920 && y_size >= 1080)
				l_size_result = 0; //Full HD
		}
	}
	return l_size_result;
}
#endif

bool WinUtil::runElevated(
    HWND    hwnd,
    LPCTSTR pszPath,
    LPCTSTR pszParameters, //   = NULL,
    LPCTSTR pszDirectory)  //   = NULL );
{

	SHELLEXECUTEINFO shex = { 0 };
	
	shex.cbSize         = sizeof(SHELLEXECUTEINFO);
	shex.fMask          = SEE_MASK_NOCLOSEPROCESS; // 0;
	shex.hwnd           = hwnd;
	shex.lpVerb         = _T("runas");
	shex.lpFile         = pszPath;
	shex.lpParameters   = pszParameters;
	shex.lpDirectory    = pszDirectory;
	shex.nShow          = SW_NORMAL;
	
	bool bRet = ::ShellExecuteEx(&shex) != FALSE;
	
	//if (shex.hProcess)
	//{
	//  BOOL result = FALSE;
	//  while (1)
	//  {
	//      if (::IsProcessInJob(shex.hProcess, NULL, &result))
	//      {
	//          if (!result)
	//              break;
	//          ::Sleep(10);
	//      }else
	//          break;
	//  }
	//}
	::Sleep(1000);
	return bRet;
}


/*
-------------------------------------------------------------------
Description:
  Creates the actual 'lnk' file (assumes COM has been initialized).

Parameters:
  pszTargetfile    - File name of the link's target.
  pszTargetargs    - Command line arguments passed to link's target.
  pszLinkfile      - File name of the actual link file being created.
  pszDescription   - Description of the linked item.
  iShowmode        - ShowWindow() constant for the link's target.
  pszCurdir        - Working directory of the active link.
  pszIconfile      - File name of the icon file used for the link.
  iIconindex       - Index of the icon in the icon file.

Returns:
  HRESULT value >= 0 for success, < 0 for failure.
--------------------------------------------------------------------
*/
bool WinUtil::CreateShortCut(const tstring& pszTargetfile, const tstring& pszTargetargs,
                             const tstring& pszLinkfile, const tstring& pszDescription,
                             int iShowmode, const tstring& pszCurdir,
                             const tstring& pszIconfile, int iIconindex)
{
	HRESULT       hRes;                  /* Returned COM result code */
	IShellLink*   pShellLink;            /* IShellLink object pointer */
	IPersistFile* pPersistFile;          /* IPersistFile object pointer */
	
	hRes = E_INVALIDARG;
	if (pszTargetfile.length() > 0 && pszLinkfile.length() > 0
	
	        /*
	                && (pszTargetargs.length() > 0)
	                && (pszDescription.length() > 0)
	                && (iShowmode >= 0)
	                && (pszCurdir.length() > 0)
	                && (pszIconfile.length() > 0 )
	                && (iIconindex >= 0)
	        */
	   )
	{
		hRes = CoCreateInstance(
		           CLSID_ShellLink,     /* pre-defined CLSID of the IShellLink object */
		           NULL,                 /* pointer to parent interface if part of aggregate */
		           CLSCTX_INPROC_SERVER, /* caller and called code are in same process */
		           IID_IShellLink,      /* pre-defined interface of the IShellLink object */
		           (LPVOID*)&pShellLink);         /* Returns a pointer to the IShellLink object */
		if (SUCCEEDED(hRes))
		{
			/* Set the fields in the IShellLink object */
			// [!] PVS V519 The 'hRes' variable is assigned values twice successively. Perhaps this is a mistake. Check lines: 4536, 4537.
			hRes |= pShellLink->SetPath(pszTargetfile.c_str());
			hRes |= pShellLink->SetArguments(pszTargetargs.c_str());
			if (pszDescription.length() > 0)
				hRes |= pShellLink->SetDescription(pszDescription.c_str());
			if (iShowmode > 0)
				hRes |= pShellLink->SetShowCmd(iShowmode);
			if (pszCurdir.length() > 0)
				hRes |= pShellLink->SetWorkingDirectory(pszCurdir.c_str());
			if (pszIconfile.length() > 0 && iIconindex >= 0)
				hRes |= pShellLink->SetIconLocation(pszIconfile.c_str(), iIconindex);
				
			/* Use the IPersistFile object to save the shell link */
			hRes |= pShellLink->QueryInterface( /* [!] PVS thanks! */
			            IID_IPersistFile,         /* pre-defined interface of the IPersistFile object */
			            (LPVOID*)&pPersistFile);            /* returns a pointer to the IPersistFile object */
			if (SUCCEEDED(hRes))
			{
				hRes = pPersistFile->Save(pszLinkfile.c_str(), TRUE);
				safe_release(pPersistFile);
			}
			safe_release(pShellLink);
		}
		
	}
	return SUCCEEDED(hRes);
}

bool WinUtil::AutoRunShortCut(bool bCreate)
{
	if (bCreate)
	{
		// Create
		if (!IsAutoRunShortCutExists())
		{
			const std::wstring targetF = Util::getModuleFileName();
			std::wstring pszCurdir =  Util::getFilePath(targetF);
			std::wstring pszDescr = getFlylinkDCAppCaptionT();
			pszDescr  += L' ' + T_VERSIONSTRING;
			AppendPathSeparator(pszCurdir);
			return CreateShortCut(targetF, L"", GetAutoRunShortCutName(), pszDescr, 0, pszCurdir, targetF, 0);
		}
	}
	else
	{
		// Remove
		if (IsAutoRunShortCutExists())
		{
			return File::deleteFileT(GetAutoRunShortCutName());
		}
	}
	
	return true;
}

bool WinUtil::IsAutoRunShortCutExists()
{
	return File::isExist(GetAutoRunShortCutName());
}

tstring WinUtil::GetAutoRunShortCutName()
{
	// Name: {userstartup}\FlylinkDC++{code:Postfix| }; Filename: {app}\FlylinkDC{code:Postfix|_}.exe; Tasks: startup; WorkingDir: {app}
	// CSIDL_STARTUP
	TCHAR startupPath[MAX_PATH];
	if (!SHGetSpecialFolderPath(NULL, startupPath, CSIDL_STARTUP, TRUE))
		return BaseUtil::emptyStringT;
		
	tstring autoRunShortCut = startupPath;
	AppendPathSeparator(autoRunShortCut);
	autoRunShortCut += getFlylinkDCAppCaptionT();
#if defined(_WIN64)
	autoRunShortCut += L"_x64";
#endif
	autoRunShortCut += L".lnk";
	
	return autoRunShortCut;
}

#ifdef SSA_VIDEO_PREVIEW_FEATURE
void WinUtil::StartPreviewClient()
{
	if (BOOLSETTING(INT_PREVIEW_START_CLIENT))
	{
		const wstring URLVideoClient = Text::toT(SETTING(INT_PREVIEW_CLIENT_PATH));
		const wstring vcParameter = L"http://localhost:" + Util::toStringW(SETTING(INT_PREVIEW_SERVER_PORT));
		::ShellExecute(NULL, _T("open"), URLVideoClient.c_str(), vcParameter.c_str(), NULL, SW_SHOWNORMAL);
	}
}
#endif // SSA_VIDEO_PREVIEW_FEATURE

bool WinUtil::GetDlgItemText(HWND p_Dlg, int p_ID, tstring& p_str)
{
	const auto l_id = GetDlgItem(p_Dlg, p_ID);
	if (l_id == NULL)
	{
		dcassert(0);
		CFlyServerJSON::pushError(64, "error WinUtil::GetDlgItemText p_ID = " + Util::toString(p_ID));
		return false; // fix https://drdump.com/Problem.aspx?ProblemID=226877
	}
	else
	{
		const int l_size = ::GetWindowTextLength(l_id);
		p_str.clear();
		if (l_size > 0)
		{
			p_str.resize(l_size + 1);
			p_str[0] = 0;
			const auto l_size_text = ::GetDlgItemText(p_Dlg, p_ID, &p_str[0], l_size + 1);
			p_str.resize(l_size_text);
			return true;
		}
	}
	return false;
}

bool Colors::getColorFromString(const tstring& colorText, COLORREF& color)
{

//#define USE_CRUTH_FOR_GET_HTML_COLOR

	// TODO - add background color!!!
	int r = 0;
	int g = 0;
	int b = 0;
	tstring colorTextLower = Text::toLower(colorText);
#ifdef USE_CRUTH_FOR_GET_HTML_COLOR
	Text::trim(colorTextLower); // Crutch.
#endif
	if (colorTextLower.empty())
	{
		return false;
	}
	else if (colorTextLower[0]  == L'#') // #FF0000
	{
#ifdef USE_CRUTH_FOR_GET_HTML_COLOR
		if (colorTextLower.length() > 7)
			colorTextLower = colorTextLower.substr(0, 7); // Crutch.
		else for (int i = colorTextLower.length(); i < 7; i++)
				colorTextLower += _T('0');
#else
		if (colorTextLower.size() != 7)
			return false;
#endif
		// TODO: rewrite without copy of string.
		const tstring s1(colorTextLower, 1, 2);
		const tstring s2(colorTextLower, 3, 2);
		const tstring s3(colorTextLower, 5, 2);
		try
		{
			r = stoi(s1, NULL, 16);
			g = stoi(s2, NULL, 16);
			b = stoi(s3, NULL, 16);
		}
		catch (const std::invalid_argument& /*e*/)
		{
			return false;
		}
		color = RGB(r, g, b);
		return true;
	}
	else
	{
		// Add constant colors http://www.computerhope.com/htmcolor.htm
		for (size_t i = 0; i < _countof(g_htmlColors); i++)
		{
			if (colorTextLower == g_htmlColors[i].tag)
			{
				color = g_htmlColors[i].color;
				return true;
			}
		}
		return false;
	}
}
bool WinUtil::isUseExplorerTheme()
{
	return BOOLSETTING(USE_EXPLORER_THEME);
}
void WinUtil::SetWindowThemeExplorer(HWND p_hWnd)
{
	if (isUseExplorerTheme())
	{
		SetWindowTheme(p_hWnd, L"explorer", NULL);
	}
}

#ifdef IRAINMAN_ENABLE_WHOIS
void WinUtil::CheckOnWhoisIP(WORD wID, const tstring& whoisIP)
{
	if (!whoisIP.empty())
	{
		tstring m_link;
		switch (wID)
		{
			case IDC_WHOIS_IP:
				m_link = _T("http://www.ripe.net/perl/whois?form_type=simple&full_query_string=&searchtext=") + whoisIP;
				break;
			case IDC_WHOIS_IP2:
				m_link = _T("http://bgp.he.net/ip/") + whoisIP + _T("#_whois");
				break;
		}
		if (!m_link.empty())
			WinUtil::openLink(m_link);
	}
}
void WinUtil::AppendMenuOnWhoisIP(CMenu& p_menuname, const tstring& p_IP, bool p_inSubmenu)
{
	p_menuname.AppendMenu(MF_STRING, IDC_WHOIS_IP, (TSTRING(WHO_IS) + _T(" Ripe.net  ") + p_IP).c_str());
	p_menuname.AppendMenu(MF_STRING, IDC_WHOIS_IP2, (TSTRING(WHO_IS) + _T(" Bgp.He  ") + p_IP).c_str());
	p_menuname.AppendMenu(MF_STRING, IDC_WHOIS_IP4_INFO, tstring(_T(" IP v4 Info ") + p_IP).c_str());
	//p_menu.AppendMenu(MF_SEPARATOR);
}
#endif

void Preview::startMediaPreview(WORD wID, const QueueItemPtr& qi)
{
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	if (wID == IDC_PREVIEW_APP_INT)
	{
		runInternalPreview(qi);
	}
	else
#endif
	{
		const auto fileName = !qi->getTempTarget().empty() ? qi->getTempTarget() : qi->getTargetFileName();
		runPreviewCommand(wID, fileName);
	}
}

void Preview::startMediaPreview(WORD wID, const TTHValue& tth
#ifdef SSA_VIDEO_PREVIEW_FEATURE
                                , const int64_t& size
#endif
                               )
{
	startMediaPreview(wID, ShareManager::toRealPath(tth)
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	                  , size
#endif
	                 );
}

void Preview::startMediaPreview(WORD wID, const string& target
#ifdef SSA_VIDEO_PREVIEW_FEATURE
                                , const int64_t& size
#endif
                               )
{
	if (!target.empty())
	{
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		if (wID == IDC_PREVIEW_APP_INT)
		{
			runInternalPreview(target, size);
		}
		else
#endif
		{
			runPreviewCommand(wID, target);
		}
	}
}

void Preview::clearPreviewMenu()
{
	g_previewMenu.ClearMenu();
	dcdrun(_debugIsClean = true; _debugIsActivated = false; g_previewAppsSize = 0;)
}

UINT Preview::getPreviewMenuIndex()
{
	return (UINT)(HMENU)g_previewMenu;
}

