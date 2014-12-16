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

#if !defined(WIN_UTIL_H)
#define WIN_UTIL_H

#pragma once

#include <Richedit.h>
#include <boost/bind.hpp>
#include <atlctrls.h>

#include "resource.h"
#include "../client/DCPlusPlus.h"
#include "../client/Util.h"
#include "../client/SettingsManager.h"
#include "../client/MerkleTree.h"
#include "../client/HintedUser.h"
#include "../client/CompatibilityManager.h"
#include "../client/ShareManager.h"
#include "UserInfoSimple.h"
#include "OMenu.h"
#include "HIconWrapper.h"
#include "wtl_flylinkdc.h"

// [+] IRainman copy-past fix.
#define SHOW_POPUP(popup_key, msg, title) { if (POPUP_ENABLED(popup_key)) MainFrame::getMainFrame()->ShowBalloonTip((msg).c_str(), (title).c_str()); }
#define SHOW_POPUPF(popup_key, msg, title, flags) { if (POPUP_ENABLED(popup_key)) MainFrame::getMainFrame()->ShowBalloonTip((msg).c_str(), (title).c_str(), flags); }
#define SHOW_POPUP_EXT(popup_key, msg, popup_ext_key, ext_msg, ext_len, title) { if (POPUP_ENABLED(popup_key)) if (BOOLSETTING(popup_ext_key)) MainFrame::getMainFrame()->ShowBalloonTip((msg + ext_msg.substr(0, ext_len)).c_str(), (title).c_str()); }

#define SET_EXTENDENT_LIST_VIEW_STYLE(ctrlList)            ctrlList.SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | (BOOLSETTING(VIEW_GRIDCONTROLS) ? LVS_EX_GRIDLINES /*TODO LVS_OWNERDRAWFIXED*/ : 0))
#define SET_EXTENDENT_LIST_VIEW_STYLE_PTR(ctrlList)            ctrlList->SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | (BOOLSETTING(VIEW_GRIDCONTROLS) ? LVS_EX_GRIDLINES /*TODO LVS_OWNERDRAWFIXED*/ : 0))
#define SET_EXTENDENT_LIST_VIEW_STYLE_WITH_CHECK(ctrlList) ctrlList.SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | (BOOLSETTING(VIEW_GRIDCONTROLS) ? LVS_EX_GRIDLINES /*TODO LVS_OWNERDRAWFIXED*/ : 0))
#define SET_LIST_COLOR(ctrlList) { ctrlList.SetBkColor(Colors::bgColor);ctrlList.SetTextBkColor(Colors::bgColor);ctrlList.SetTextColor(Colors::textColor); }
#define SET_LIST_COLOR_PTR(ctrlList) { ctrlList->SetBkColor(Colors::bgColor);ctrlList->SetTextBkColor(Colors::bgColor);ctrlList->SetTextColor(Colors::textColor); }
#ifdef USE_SET_LIST_COLOR_IN_SETTINGS
#define SET_LIST_COLOR_IN_SETTING(ctrlList) SET_LIST_COLOR(ctrlList)
#else
#define SET_LIST_COLOR_IN_SETTING(ctrlList)
#endif

#define SET_MIN_MAX(x, y, z) \
	updown.Attach(GetDlgItem(x)); \
	updown.SetRange32(y, z); \
	updown.Detach();
// [~] IRainman copy-past fix.

// Some utilities for handling HLS colors, taken from Jean-Michel LE FOL's codeproject
// article on WTL OfficeXP Menus
typedef DWORD HLSCOLOR;
#define HLS(h,l,s) ((HLSCOLOR)(((BYTE)(h)|((WORD)((BYTE)(l))<<8))|(((DWORD)(BYTE)(s))<<16)))
#define HLS_H(hls) ((BYTE)(hls))
#define HLS_L(hls) ((BYTE)(((WORD)(hls)) >> 8))
#define HLS_S(hls) ((BYTE)((hls)>>16))

HLSCOLOR RGB2HLS(COLORREF rgb);
COLORREF HLS2RGB(HLSCOLOR hls);

COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S);

static const TCHAR FILE_LIST_TYPE_T[] = L"All Lists\0*.xml.bz2;*.dcls;*.dclst\0FileLists\0*.xml.bz2\0DCLST metafiles\0*.dcls;*.dclst\0All Files\0*.*\0\0"; // [+] SSA dclst support. TODO translate.

struct Tags// [+] IRainman struct for links and BB codes
{
	Tags(const TCHAR* _tag) : tag(_tag) { }
	const tstring tag;
};

#define EXT_URL_LIST() \
	Tags(_T("http://")), \
	Tags(_T("https://")), \
	Tags(_T("ftp://")), \
	Tags(_T("irc://")), \
	Tags(_T("file://")), \
	Tags(_T("skype:")), \
	Tags(_T("ed2k://")), \
	Tags(_T("mms://")), \
	Tags(_T("xmpp://")), \
	Tags(_T("nfs://")), \
	Tags(_T("mailto:")), \
	Tags(_T("www.")), \
	Tags(_T("mtasa://")), \
	Tags(_T("samp://")), \
	Tags(_T("steam://"))
/*[!] IRainman: "www." - this record is possible because function WinUtil::translateLinkToextProgramm
    automatically generates the type of protocol as http before transfer to browser*/

template <class T> inline void safe_unsubclass_window(T* p)
{
	dcassert(p->IsWindow());
	if (p != nullptr && p->IsWindow())
	{
		p->UnsubclassWindow();
	}
}
template <class T> inline void safe_destroy_window(T* p)
{
	if (p)
	{
		if (p->IsWindow())
		{
			p->DestroyWindow();
			p->Detach();
		}
		else
		{
			dcassert(0);
		}
	}
}


#ifdef RIP_USE_SKIN
class ITabCtrl;
#else
class FlatTabCtrl;
#endif
class UserCommand;


class Preview // [+] IRainman fix.
{
	public:
		static void init()
		{
			g_previewMenu.CreatePopupMenu();
		}
		static bool isPreviewMenu(const HMENU& handle)
		{
			return g_previewMenu.m_hMenu == handle;
		}
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		static void runInternalPreview(const QueueItemPtr& qi);
		static void runInternalPreview(const string& previewFile, const int64_t& previewFileSize);
#endif
	protected:
		static void startMediaPreview(WORD wID, const QueueItemPtr& qi)
		{
#ifdef SSA_VIDEO_PREVIEW_FEATURE
			if (wID == IDC_PREVIEW_APP_INT)
			{
				runInternalPreview(qi);
			}
			else
#endif
			{
				const auto& fileName = !qi->getTempTarget().empty() ? qi->getTempTarget() : qi->getTargetFileName();
				runPreviewCommand(wID, fileName);
			}
		}
		
		static void startMediaPreview(WORD wID, const TTHValue& tth
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		                              , const int64_t& size
#endif
		                             )
		{
			startMediaPreview(wID, ShareManager::getInstance()->toRealPath(tth)
#ifdef SSA_VIDEO_PREVIEW_FEATURE
			                  , size
#endif
			                 );
		}
		
		static void startMediaPreview(WORD wID, const string& target
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
		
		static void clearPreviewMenu()
		{
			g_previewMenu.ClearMenu();
			dcdrun(_debugIsClean = true; _debugIsActivated = false; g_previewAppsSize = 0;)
		}
		
		static UINT getPreviewMenuIndex()
		{
			return (UINT)(HMENU)g_previewMenu;
		}
		
		static void setupPreviewMenu(const string& target);
		
		static void runPreviewCommand(WORD wID, string target);
		
		
		static int g_previewAppsSize;
		static OMenu g_previewMenu;
		
		dcdrun(static bool _debugIsClean; static bool _debugIsActivated;)
};

template<class T, bool OPEN_EXISTING_FILE = false>
class PreviewBaseHandler : public Preview // [+] IRainman fix.
{
		/*
		1) Create a method onPreviewCommand in your class, which will call startMediaPreview for a necessary data.
		2) clearPreviewMenu()
		3) appendPreviewItems(yourMenu)
		4) setupPreviewMenu(yourMenu)
		5) activatePreviewItems(yourMenu)
		6) Before you destroy the menu in your class you will definitely need to call WinUtil::unlinkStaticMenus(yourMenu)
		*/
	protected:
		BEGIN_MSG_MAP(PreviewBaseHandler)
		COMMAND_RANGE_HANDLER(IDC_PREVIEW_APP, IDC_PREVIEW_APP + g_previewAppsSize, onPreviewCommand)
		COMMAND_ID_HANDLER(IDC_PREVIEW_APP_INT, onPreviewCommand)
		COMMAND_ID_HANDLER(IDC_STARTVIEW_EXISTING_FILE, onPreviewCommand)
		END_MSG_MAP()
		
		virtual LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;
		
		static void appendPreviewItems(OMenu& p_menu)
		{
			dcassert(_debugIsClean);
			dcdrun(_debugIsClean = false;)
			
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)g_previewMenu, CTSTRING(PREVIEW_MENU));
			if (OPEN_EXISTING_FILE)
			{
				p_menu.AppendMenu(MF_STRING, IDC_STARTVIEW_EXISTING_FILE, CTSTRING(STARTVIEW_EXISTING_FILE));
			}
		}
		
		static void activatePreviewItems(OMenu& p_menu, const bool existingFile = false)
		{
			dcassert(!_debugIsActivated);
			dcdrun(_debugIsActivated = true;)
			
			p_menu.EnableMenuItem(getPreviewMenuIndex(), g_previewMenu.GetMenuItemCount() > 0 ? MFS_ENABLED : MFS_DISABLED);
			if (OPEN_EXISTING_FILE)
			{
				p_menu.EnableMenuItem(IDC_STARTVIEW_EXISTING_FILE, existingFile ? MFS_ENABLED : MFS_DISABLED);
			}
		}
};

template<class T>
class InternetSearchBaseHandler // [+] IRainman fix.
{
		/*
		1) Create a method onSearchFileInInternet in its class, which will call searchFileInInternet for a necessary data.
		2) appendInternetSearchItems(yourMenu)
		*/
	protected:
		BEGIN_MSG_MAP(InternetSearchBaseHandler)
		COMMAND_ID_HANDLER(IDC_SEARCH_FILE_IN_GOOGLE, onSearchFileInInternet)
		COMMAND_ID_HANDLER(IDC_SEARCH_FILE_IN_YANDEX, onSearchFileInInternet)
		END_MSG_MAP()
		
		virtual LRESULT onSearchFileInInternet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;
		
		void appendInternetSearchItems(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_STRING, IDC_SEARCH_FILE_IN_GOOGLE, CTSTRING(SEARCH_FILE_IN_GOOGLE));
			p_menu.AppendMenu(MF_STRING, IDC_SEARCH_FILE_IN_YANDEX, CTSTRING(SEARCH_FILE_IN_YANDEX));
		}
		
		static void searchFileInInternet(const WORD wID, const tstring& file)
		{
			tstring url;
			switch (wID)
			{
				case IDC_SEARCH_FILE_IN_GOOGLE:
					url += _T("https://www.google.com/search?hl=") + WinUtil::GetLang() + _T("&q=");
					break;
				case IDC_SEARCH_FILE_IN_YANDEX:
					url += _T("http://yandex.ru/yandsearch?text=");
					break;
					//  case IDC_SEARCH_FILE_IN_YAHOO:
					//      url += +T("http://") + GetLang() +_T(".yahoo.com/search?p=")
				default:
					dcassert(0);
					return;
			}
			url += file;
			WinUtil::openFile(url);
		}
		static void searchFileInInternet(const WORD wID, const string& file)
		{
			searchFileInInternet(wID, Text::toT(file));
		}
};

// [+] IRainman opt: use static object.
struct UserInfoGuiTraits // technical class, please do not use it directly!
{
	public:
		static void init(); // only for WinUtil!
		static void uninit(); // only for WinUtil!
		static bool isUserInfoMenus(const HMENU& handle) // only for WinUtil!
		{
			return
			    handle == userSummaryMenu.m_hMenu ||
			    handle == copyUserMenu.m_hMenu ||
			    handle == grantMenu.m_hMenu ||
			    handle == speedMenu.m_hMenu ||
			    handle == privateMenu.m_hMenu
			    ;
		}
		enum Options
		{
			DEFAULT = 0,
			
			NO_SEND_PM = 1,
			NO_CONNECT_FAV_HUB = 2,
			NO_COPY = 4, // Please disable if your want custom copy menu, for user items please use copyUserMenu.
			NO_FILE_LIST = 8,
			
			NICK_TO_CHAT = 16,
			USER_LOG = 32,
			
			INLINE_CONTACT_LIST = 64,
		};
	protected:
		static int getCtrlIdBySpeedLimit(int limit);
		static int getSpeedLimitByCtrlId(WORD wID, int lim);
		
		static bool ENABLE(int HANDLERS, Options FLAG) // TODO constexpr
		{
			return (HANDLERS & FLAG) != 0;
		}
		static bool DISABLE(int HANDLERS, Options FLAG) // TODO constexpr
		{
			return (HANDLERS & FLAG) == 0;
		}
		static string g_hubHint;
		
		static OMenu copyUserMenu; // [+] IRainman fix.
		static OMenu grantMenu;
		static OMenu speedMenu; // !SMT!-S
		static OMenu privateMenu; // !SMT!-PSW
		
		static OMenu userSummaryMenu; // !SMT!-UI
		
		friend class UserInfoSimple;
};

template<class T>
struct UserInfoBaseHandlerTraitsUser // technical class, please do not use it directly!
{
	protected:
		static T g_user;
};
// [~] IRainman opt.

template<class T, const int options = UserInfoGuiTraits::DEFAULT, class T2 = UserPtr>
class UserInfoBaseHandler : UserInfoBaseHandlerTraitsUser<T2>, public UserInfoGuiTraits
{
		/*
		1) If you want to get the features USER_LOG and (or) NICK_TO_CHAT you need to create methods, respectively onOpenUserLog and (or) onAddNickToChat in its class.
		2) clearUserMenu()
		3) reinitUserMenu(user, hint)
		4) appendAndActivateUserItems(yourMenu)
		5) Before you destroy the menu in your class you will definitely need to call WinUtil::unlinkStaticMenus(yourMenu)
		*/
	public:
		UserInfoBaseHandler() : m_selectedHint(UserInfoGuiTraits::g_hubHint), m_selectedUser(UserInfoBaseHandlerTraitsUser::g_user)
#ifdef _DEBUG
			, _debugIsClean(true)
#endif
		{
		}
		
		BEGIN_MSG_MAP(UserInfoBaseHandler)
		COMMAND_ID_HANDLER(IDC_GETLIST, onGetList)
		COMMAND_ID_HANDLER(IDC_BROWSELIST, onBrowseList)
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		COMMAND_ID_HANDLER(IDC_CHECKLIST, onCheckList)
#endif
		COMMAND_ID_HANDLER(IDC_GET_USER_RESPONSES, onGetUserResponses)
		COMMAND_ID_HANDLER(IDC_MATCH_QUEUE, onMatchQueue)
		COMMAND_ID_HANDLER(IDC_PRIVATE_MESSAGE, onPrivateMessage)
		COMMAND_ID_HANDLER(IDC_ADD_NICK_TO_CHAT, onAddNickToChat)
		COMMAND_ID_HANDLER(IDC_OPEN_USER_LOG, onOpenUserLog)
		COMMAND_ID_HANDLER(IDC_ADD_TO_FAVORITES, onAddToFavorites)
		COMMAND_ID_HANDLER(IDC_REMOVE_FROM_FAVORITES, onRemoveFromFavorites)
		COMMAND_ID_HANDLER(IDC_IGNORE_BY_NAME, onIgnoreOrUnignoreUserByName) // [!] IRainman moved from HubFrame and clean.
		COMMAND_RANGE_HANDLER(IDC_GRANTSLOT, IDC_UNGRANTSLOT, onGrantSlot)
		COMMAND_RANGE_HANDLER(IDC_PM_NORMAL, IDC_PM_FREE, onPrivateAcces)
		COMMAND_RANGE_HANDLER(IDC_SPEED_NORMAL, IDC_SPEED_MANUAL, onSetUserLimit)
		COMMAND_ID_HANDLER(IDC_REMOVEALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_REPORT, onReport)
		COMMAND_ID_HANDLER(IDC_CONNECT, onConnectFav)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopyUserInfo)
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		COMMAND_ID_HANDLER(IDC_COPY_EXACT_SHARE, onCopyUserInfo)
#endif
		COMMAND_ID_HANDLER(IDC_COPY_ANTIVIRUS_DB_INFO, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_APPLICATION, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_TAG, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_CID, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_DESCRIPTION, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_EMAIL_ADDRESS, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_GEO_LOCATION, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_NICK_IP, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_IP, onCopyUserInfo)
		COMMAND_ID_HANDLER(IDC_COPY_ALL, onCopyUserInfo)
		END_MSG_MAP()
		
		virtual LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(0);
			return 0;
		}
		
		virtual LRESULT onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(0);
			return 0;
		}
		
		virtual LRESULT onCopyUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(0);
			/* TODO https://code.google.com/p/flylinkdc/issues/detail?id=659
			// !SMT!-S
			const auto& su = getSelectedUser();
			if (su)
			{
			    __if_exists(T2::getIdentity)
			    {
			        FFFFF
			        const auto& id = su->getIdentity();
			        const auto& su = su->getUser();
			    }
			    __if_exists(T2::getLastNick)
			    {
			        FFFFF
			        const auto& u = su;
			    }
			
			    string sCopy;
			    switch (wID)
			    {
			        case IDC_COPY_NICK:
			            __if_exists(T2::getIdentity)
			            {
			                iiii
			                sCopy += id.getNick();
			            }
			            __if_exists(T2::getLastNick)
			            {
			                iiii
			                sCopy += su->getLastNick();
			            }
			            break;
			        case IDC_COPY_IP:
			            __if_exists(T2::getIp)
			            {
			                iiii
			                sCopy += su->getIp();
			            }
			            __if_exists(T2::getIP)
			            {
			                iiii
			                sCopy += su->getIP();
			            }
			            break;
			        case IDC_COPY_NICK_IP:
			        {
			            // TODO translate
			            sCopy += "User Info:\r\n";
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n";
			            }
			            __if_exists(T2::getLastNick)
			            {
			                sCopy += "\t" + STRING(NICK) + ": " + su->getLastNick() + "\r\n";
			            }
			            sCopy += "\tIP: ";
			            __if_exists(T2::getIp)
			            {
			                sCopy += Identity::formatIpString(su->getIp());
			            }
			            __if_exists(T2::getIP)
			            {
			                sCopy += Identity::formatIpString(su->getIP());
			            }
			            break;
			        }
			        case IDC_COPY_ALL:
			        {
			            // TODO translate
			            sCopy += "User info:\r\n";
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n";
			            }
			            __if_exists(T2::getLastNick)
			            {
			                sCopy += "\t" + STRING(NICK) + ": " + su->getLastNick() + "\r\n";
			            }
			            sCopy += "\tNicks: " + Util::toString(ClientManager::getNicks(u->getCID(), Util::emptyString)) + "\r\n" +
			                     "\t" + STRING(HUBS) + ": " + Util::toString(ClientManager::getHubs(u->getCID(), Util::emptyString)) + "\r\n" +
			                     "\t" + STRING(SHARED) + ": " + Identity::formatShareBytes(u->getBytesShared());
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += (u->isNMDC() ? Util::emptyString : "(" + STRING(SHARED_FILES) + ": " + Util::toString(id.getSharedFiles()) + ")");
			            }
			            sCopy += "\r\n";
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += "\t" + STRING(DESCRIPTION) + ": " + id.getDescription() + "\r\n" +
			                         "\t" + STRING(APPLICATION) + ": " + id.getApplication() + "\r\n";
			                const auto con = Identity::formatSpeedLimit(id.getDownloadSpeed());
			                if (!con.empty())
			                {
			                    sCopy += "\t";
			                    sCopy += (u->isNMDC() ? STRING(CONNECTION) : "Download speed");
			                    sCopy += ": " + con + "\r\n";
			                }
			            }
			            const auto lim = Identity::formatSpeedLimit(u->getLimit());
			            if (!lim.empty())
			            {
			                sCopy += "\tUpload limit: " + lim + "\r\n";
			            }
			            __if_exists(T2::getIdentity)
			            {
			                sCopy += "\tE-Mail: " + id.getEmail() + "\r\n" +
			                         "\tClient Type: " + Util::toString(id.getClientType()) + "\r\n" +
			                         "\tMode: " + (id.isTcpActive(&id.getClient()) ? 'A' : 'P') + "\r\n";
			            }
			            sCopy += "\t" + STRING(SLOTS) + ": " + Util::toString(su->getSlots()) + "\r\n";
			            __if_exists(T2::getIp)
			            {
			                sCopy += "\tIP: " + Identity::formatIpString(id.getIp()) + "\r\n";
			            }
			            __if_exists(T2::getIP)
			            {
			                sCopy += "\tIP: " + Identity::formatIpString(id.getIP()) + "\r\n";
			            }
			            __if_exists(T2::getIdentity)
			            {
			                const auto su = id.getSupports();
			                if (!su.empty())
			                {
			                    sCopy += "\tKnown supports: " + su;
			                }
			            }
			            break;
			        }
			        default:
			            __if_exists(T2::getIdentity)
			            {
			                switch (wID)
			                {
			                    case IDC_COPY_EXACT_SHARE:
			                        sCopy += Identity::formatShareBytes(id.getBytesShared());
			                        break;
			                    case IDC_COPY_DESCRIPTION:
			                        sCopy += id.getDescription();
			                        break;
			                    case IDC_COPY_APPLICATION:
			                        sCopy += id.getApplication();
			                        break;
			                    case IDC_COPY_TAG:
			                        sCopy += id.getTag();
			                        break;
			                    case IDC_COPY_CID:
			                        sCopy += id.getCID();
			                        break;
			                    case IDC_COPY_EMAIL_ADDRESS:
			                        sCopy += id.getEmail();
			                        break;
			                    default:
			                        dcdebug("THISFRAME DON'T GO HERE\n");
			                        return 0;
			                }
			                break;
			            }
			            dcdebug("THISFRAME DON'T GO HERE\n");
			            return 0;
			    }
			    if (!sCopy.empty())
			    {
			        WinUtil::setClipboard(sCopy);
			    }
			}
			*/
			return 0;
		}
		
		LRESULT onMatchQueue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::matchQueue);
			return 0;
		}
		LRESULT onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::getList);
			return 0;
		}
		LRESULT onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::browseList);
			return 0;
		}
		LRESULT onReport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::doReport, m_selectedHint);
			return 0;
		}
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		LRESULT onCheckList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::checkList);
			return 0;
		}
#endif
		LRESULT onGetUserResponses(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::getUserResponses);
			return 0;
		}
		
		LRESULT onAddToFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::addFav);
			return 0;
		}
		LRESULT onRemoveFromFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::delFav);
			return 0;
		}
		LRESULT onSetUserLimit(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			MENUINFO menuInfo = {0};
			menuInfo.cbSize = sizeof(MENUINFO);
			menuInfo.fMask = MIM_MENUDATA;
			speedMenu.GetMenuInfo(&menuInfo);
			const int iLimit = menuInfo.dwMenuData;
			
			const int lim = getSpeedLimitByCtrlId(wID, iLimit);
			
			doAction(&UserInfoBase::setUploadLimit, lim);
			return 0;
		}
		LRESULT onPrivateAcces(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			switch (wID)
			{
				case IDC_PM_IGNORED:
					doAction(&UserInfoBase::setIgnorePM);
					break;
				case IDC_PM_FREE:
					doAction(&UserInfoBase::setFreePM);
					break;
				case IDC_PM_NORMAL:
					doAction(&UserInfoBase::setNormalPM);
					break;
			};
			return 0;
		}
		LRESULT onIgnoreOrUnignoreUserByName(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::ignoreOrUnignoreUserByName);
			return 0;
		}
		
		LRESULT onPrivateMessage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).pm)(m_selectedHint);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					// !SMT!-S
					if (((T*)this)->getUserList().getSelectedCount() > 1)
					{
						const tstring l_message = UserInfoSimple::getBroadcastPrivateMessage();
						if (!l_message.empty()) // [+] SCALOlaz: support for abolition and prohibition to send a blank line https://code.google.com/p/flylinkdc/issues/detail?id=1034
							((T*)this)->getUserList().forEachSelectedParam(&UserInfoBase::pm_msg, m_selectedHint, l_message);
					}
					else
					{
						((T*)this)->getUserList().forEachSelectedT(boost::bind(&UserInfoBase::pm, _1, m_selectedHint));
					}
					// !SMT!-S end
				}
			}
			return 0;
		}
		
		LRESULT onConnectFav(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::connectFav);
			return 0;
		}
		LRESULT onGrantSlot(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			switch (wID)
			{
				case IDC_GRANTSLOT:
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, 600);
					break;
				case IDC_GRANTSLOT_HOUR:
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, 3600);
					break;
				case IDC_GRANTSLOT_DAY:
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, 24 * 3600);
					break;
				case IDC_GRANTSLOT_WEEK:
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, 7 * 24 * 3600);
					break;
				case IDC_GRANTSLOT_PERIOD:
				{
					const uint64_t slotTime = UserInfoSimple::inputSlotTime();
					doAction(&UserInfoBase::grantSlotPeriod, m_selectedHint, slotTime);
				}
				break;
				case IDC_UNGRANTSLOT:
					doAction(&UserInfoBase::ungrantSlot, m_selectedHint);
					break;
			}
			return 0;
		}
		LRESULT onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			doAction(&UserInfoBase::removeAll);
			return 0;
		}
		
		// [+] IRainman fix.
		const T2& getSelectedUser() const
		{
			return m_selectedUser;
		}
		const string& getSelectedHint() const
		{
			return m_selectedHint;
		}
		
		void clearUserMenu()
		{
			m_selectedHint.clear();
			m_selectedUser = nullptr;
			userSummaryMenu.ClearMenu(); // !SMT!-UI
			
			for (int j = 0; j < speedMenu.GetMenuItemCount(); ++j) // !SMT!-S
			{
				speedMenu.CheckMenuItem(j, MF_BYPOSITION | MF_UNCHECKED);
			}
			
			// !SMT!-S
			privateMenu.CheckMenuItem(IDC_PM_NORMAL, MF_BYCOMMAND | MF_UNCHECKED);
			privateMenu.CheckMenuItem(IDC_PM_IGNORED, MF_BYCOMMAND | MF_UNCHECKED);
			privateMenu.CheckMenuItem(IDC_PM_FREE, MF_BYCOMMAND | MF_UNCHECKED);
			
			dcdrun(_debugIsClean = true;)
		}
		
		void reinitUserMenu(const T2& user, const string& hint)
		{
			dcassert(_debugIsClean);
			
			m_selectedHint = hint;
			m_selectedUser = user;
		}
		// [~] IRainman fix.
		void appendAndActivateUserItems(CMenu& p_menu)
		{
			dcassert(_debugIsClean);
			dcdrun(_debugIsClean = false;)
			
			if (m_selectedUser)
			{
				appendAndActivateUserItemsForSingleUser(p_menu, m_selectedHint);
			}
			else
			{
				__if_exists(T::getUserList) // ??
				{
					doAction(&UserInfoBase::createSummaryInfo, m_selectedHint);
					FavUserTraits traits; // empty
					
					appendSendAutoMessageItems(p_menu);
					appendFileListItems(p_menu);
					
					appendContactListMenu(p_menu, traits);
					appendFavPrivateMenu(p_menu);
					appendIgnoreByNameItem(p_menu, traits);
					appendGrantItems(p_menu);
					appendSpeedLimitMenu(p_menu);
					appendSeparator(p_menu);
					
					appendRemoveAllItems(p_menu);
					appendSeparator(p_menu);
					
					appenUserSummaryMenu(p_menu);
				}
			}
		}
		
	private:
	
		void appendAndActivateUserItemsForSingleUser(CMenu& p_menu, const string& p_selectedHint) // [+] IRainman fix.
		{
			dcassert(m_selectedUser);
			
			UserInfoSimple ui(m_selectedUser, p_selectedHint);
			FavUserTraits traits;
			traits.init(ui);
			ui.createSummaryInfo(p_selectedHint);
			activateFavPrivateMenuForSingleUser(p_menu, traits);
			activateSpeedLimitMenuForSingleUser(p_menu, traits);
			if (ENABLE(options, NICK_TO_CHAT))
			{
				p_menu.AppendMenu(MF_STRING, IDC_ADD_NICK_TO_CHAT, CTSTRING(ADD_NICK_TO_CHAT));
			}
			if (ENABLE(options, USER_LOG))
			{
				p_menu.AppendMenu(MF_STRING | (!BOOLSETTING(LOG_PRIVATE_CHAT) ? MF_DISABLED : 0), IDC_OPEN_USER_LOG, CTSTRING(OPEN_USER_LOG));
				p_menu.AppendMenu(MF_SEPARATOR);
			}
			appendSendAutoMessageItems(p_menu);
			appendCopyMenuForSingleUser(p_menu);
			appendFileListItems(p_menu);
			appendContactListMenu(p_menu, traits);
			appendFavPrivateMenu(p_menu);
			appendIgnoreByNameItem(p_menu, traits);
			appendGrantItems(p_menu);
			appendSpeedLimitMenu(p_menu);
			appendSeparator(p_menu);
			appendRemoveAllItems(p_menu);
			appendSeparator(p_menu);
			appenUserSummaryMenu(p_menu);
		}
		
		void appendSeparator(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_SEPARATOR);
		}
		
		void appenUserSummaryMenu(CMenu& p_menu)
		{
			//const bool notEmpty = userSummaryMenu.GetMenuItemCount() > 1;
			
			//p_menu.AppendMenu(MF_POPUP, notEmpty ? (UINT_PTR)(HMENU)userSummaryMenu : 0, CTSTRING(USER_SUMMARY));
			//p_menu.EnableMenuItem(p_menu.GetMenuItemCount() - 1, notEmpty ? MFS_ENABLED : MFS_DISABLED);
			
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)userSummaryMenu, CTSTRING(USER_SUMMARY));
		}
		
		void appendFavPrivateMenu(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)privateMenu, CTSTRING(ACCES_TO_PERSONAL_MESSAGES));
		}
		
		void activateFavPrivateMenuForSingleUser(CMenu& p_menu, const FavUserTraits& traits)
		{
			dcassert(m_selectedUser);
			if (traits.isIgnoredPm)
			{
				privateMenu.CheckMenuItem(IDC_PM_IGNORED, MF_BYCOMMAND | MF_CHECKED);
			}
			else if (traits.isFreePm)
			{
				privateMenu.CheckMenuItem(IDC_PM_FREE, MF_BYCOMMAND | MF_CHECKED);
			}
			else
			{
				privateMenu.CheckMenuItem(IDC_PM_NORMAL, MF_BYCOMMAND | MF_CHECKED);
			}
		}
		
		void appendSpeedLimitMenu(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)speedMenu, CTSTRING(UPLOAD_SPEED_LIMIT));
		}
		
		void activateSpeedLimitMenuForSingleUser(CMenu& p_menu, const FavUserTraits& traits) // !SMT!-S
		{
			dcassert(m_selectedUser);
			const int id = getCtrlIdBySpeedLimit(traits.uploadLimit);
			speedMenu.CheckMenuItem(id, MF_BYCOMMAND | MF_CHECKED);
			MENUINFO menuInfo = {0};
			menuInfo.cbSize = sizeof(MENUINFO);
			menuInfo.fMask = MIM_MENUDATA;
			menuInfo.dwMenuData = traits.uploadLimit;
			p_menu.SetMenuInfo(&menuInfo);
		}
		
		void appendIgnoreByNameItem(CMenu& p_menu, const FavUserTraits& traits)
		{
			p_menu.AppendMenu(MF_STRING, IDC_IGNORE_BY_NAME, traits.isIgnoredByName ? CTSTRING(UNIGNORE_USER_BY_NAME) : CTSTRING(IGNORE_USER_BY_NAME));
		}
		
		template <typename T>
		void internal_appendContactListItems(T& p_menu, const FavUserTraits& traits)
		{
#ifndef IRAINMAN_ALLOW_ALL_CLIENT_FEATURES_ON_NMDC
			if (traits.adcOnly) // TODO
			{
			}
#endif
			if (traits.isEmpty || !traits.isFav)
			{
				p_menu.AppendMenu(MF_STRING, IDC_ADD_TO_FAVORITES, CTSTRING(ADD_TO_FAVORITES));
			}
			if (traits.isEmpty || traits.isFav)
			{
				p_menu.AppendMenu(MF_STRING, IDC_REMOVE_FROM_FAVORITES, CTSTRING(REMOVE_FROM_FAVORITES));
			}
			
			if (DISABLE(options, NO_CONNECT_FAV_HUB))
			{
				if (traits.isFav)
				{
					p_menu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT_FAVUSER_HUB));
				}
			}
		}
		
		void appendContactListMenu(CMenu& p_menu, const FavUserTraits& traits)
		{
			if (ENABLE(options, INLINE_CONTACT_LIST))
			{
				internal_appendContactListItems(p_menu, traits);
			}
			else
			{
				CMenuHandle favMenu;
				favMenu.CreatePopupMenu();
				internal_appendContactListItems(favMenu, traits);
				p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)favMenu, CTSTRING(CONTACT_LIST_MENU));
			}
		}
		
		void appendCopyMenuForSingleUser(CMenu& p_menu)
		{
			dcassert(m_selectedUser);
			
			if (DISABLE(options, NO_COPY))
			{
				p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyUserMenu, CTSTRING(COPY));
				appendSeparator(p_menu);
			}
		}
		
		void appendSendAutoMessageItems(CMenu& p_menu/*, const int count*/)
		{
			if (DISABLE(options, NO_SEND_PM))
			{
				if (m_selectedUser)
				{
					p_menu.AppendMenu(MF_STRING, IDC_PRIVATE_MESSAGE, CTSTRING(SEND_PRIVATE_MESSAGE));
					p_menu.AppendMenu(MF_SEPARATOR);
				}
			}
		}
		
		
		void appendFileListItems(CMenu& p_menu)
		{
			if (DISABLE(options, NO_FILE_LIST))
			{
				p_menu.AppendMenu(MF_STRING, IDC_GETLIST, CTSTRING(GET_FILE_LIST));
				p_menu.AppendMenu(MF_STRING, IDC_BROWSELIST, CTSTRING(BROWSE_FILE_LIST));
				p_menu.AppendMenu(MF_STRING, IDC_MATCH_QUEUE, CTSTRING(MATCH_QUEUE));
				appendSeparator(p_menu);
			}
		}
		
		void appendRemoveAllItems(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_STRING, IDC_REMOVEALL, CTSTRING(REMOVE_FROM_ALL));
		}
		
		void appendGrantItems(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)grantMenu, CTSTRING(GRANT_SLOTS_MENU));
		}
		
		string& m_selectedHint; // [!] IRainman opt.
		T2& m_selectedUser; // [!] IRainman fix: use online user here!
		
	protected:
	
		void doAction(void (UserInfoBase::*func)(const int data), const int data) // [+] IRainman.
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)(data);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					((T*)this)->getUserList().forEachSelectedT(boost::bind(func, _1, data));
				}
			}
		}
		
		void doAction(void (UserInfoBase::*func)(const string &hubHint, const tstring& data), const string &hubHint, const tstring& data) // [+] IRainman.
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)(hubHint, data);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					((T*)this)->getUserList().forEachSelectedParam(func, hubHint, data);
				}
			}
		}
		void doAction(void (UserInfoBase::*func)(const string &hubHint, const uint64_t data), const string &hubHint, const uint64_t data)
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)(hubHint, data);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					((T*)this)->getUserList().forEachSelectedParam(func, hubHint, data);
				}
			}
		}
		
		// !SMT!-S
		void doAction(void (UserInfoBase::*func)(const string &hubHint), const string &hubHint)
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)(hubHint);
			}
			else
			{
				__if_exists(T::getUserList)
				{
					((T*)this)->getUserList().forEachSelectedT(boost::bind(func, _1, hubHint));
				}
			}
		}
		
		// !SMT!-S
		void doAction(void (UserInfoBase::*func)())
		{
			if (m_selectedUser)
			{
				(UserInfoSimple(m_selectedUser, m_selectedHint).*func)();
			}
			else
			{
				__if_exists(T::getUserList)
				{
					((T*)this)->getUserList().forEachSelected(func);
				}
			}
		}
	private:
		dcdrun(bool _debugIsClean;)
};


template < class T, int title, int ID = -1 >
class StaticFrame
{
	public:
		virtual ~StaticFrame()
		{
			frame = nullptr;
		}
		
		static T* frame;
		static void openWindow()
		{
			if (frame == nullptr)
			{
				frame = new T();
				frame->CreateEx(WinUtil::mdiClient, frame->rcDefault, CTSTRING_I(ResourceManager::Strings(title)));
				WinUtil::setButtonPressed(ID, true);
			}
			else
			{
				// match the behavior of MainFrame::onSelected()
				HWND hWnd = frame->m_hWnd;
				if (isMDIChildActive(hWnd))
				{
					::PostMessage(hWnd, WM_CLOSE, NULL, NULL);
				}
				else if (frame->MDIGetActive() != hWnd)
				{
					MainFrame::getMainFrame()->MDIActivate(hWnd);
					WinUtil::setButtonPressed(ID, true);
				}
				else if (BOOLSETTING(TOGGLE_ACTIVE_WINDOW))
				{
					::SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
					frame->MDINext(hWnd);
					hWnd = frame->MDIGetActive();
					WinUtil::setButtonPressed(ID, true);
				}
				if (::IsIconic(hWnd))
					::ShowWindow(hWnd, SW_RESTORE);
			}
		}
		static bool isMDIChildActive(HWND hWnd)
		{
			HWND wnd = MainFrame::getMainFrame()->MDIGetActive();
			dcassert(wnd != NULL);
			return (hWnd == wnd);
		}
		// !SMT!-S
		static void closeWindow()
		{
			if (frame) ::PostMessage(frame->m_hWnd, WM_CLOSE, NULL, NULL);
		}
};

template<class T, int title, int ID>
T* StaticFrame<T, title, ID>::frame = NULL;

struct toolbarButton
{
	int id, image;
	bool check;
	ResourceManager::Strings tooltip;
};

struct menuImage
{
	int id;
	int image;
};

// [!] brain-ripper
// In order to correct work of small images for toolbar's menu
// toolbarButton::image MUST be in order without gaps.
extern const toolbarButton g_ToolbarButtons[];
// [+] BRAIN_RIPPER
// Images ids MUST be synchronized with icons number in Toolbar-mini.png.
// This is second path, first is described in ToolbarButtons.
// So in this array images id continues after last image in ToolbarButtons array.
extern const menuImage g_MenuImages[];
extern const toolbarButton g_WinampToolbarButtons[];
extern const int g_cout_of_ToolbarButtons;
extern const int g_cout_of_WinampToolbarButtons;

class BaseImageList
{
	public:
		CImageList& getIconList()
		{
			return m_images;
		}
		virtual void Draw(HDC hDC, int nImage, POINT pt)
		{
			m_images.Draw(hDC, nImage, pt, LVSIL_SMALL);
		}
		void uninit()
		{
			m_images.Destroy();
		}
		virtual ~BaseImageList()
		{
		}
	protected:
		CImageList m_images;
};

struct FileImage : public BaseImageList
{
	public:
		enum TypeDirectoryImages
		{
			DIR_ICON,
			DIR_MASKED,
			DIR_FILE,
			DIR_DSLCT,
			DIR_DVD,
			DIR_BD,
			DIR_IMAGE_LAST
		};
		
		int getIconIndex(const string& aFileName);
		
		static bool isBdFolder(const string& nameDir)
		{
			// Папка содержащая подпапки VIDEO_TS или AUDIO_TS, является DVD папкой
			static const string g_bdmvDir = "BDMV";
			return nameDir == g_bdmvDir;
		}
		
		static bool isDvdFolder(const string& nameDir)
		{
			// Папка содержащая подпапки VIDEO_TS или AUDIO_TS, является DVD папкой
			static const string g_audioTsDir = "AUDIO_TS";
			static const string g_videoTsDir = "VIDEO_TS";
			return nameDir == g_audioTsDir  || nameDir == g_videoTsDir;
		}
		
		// Метод по названию файла определяет, является ли файл частью dvd
		static bool isDvdFile(const string& nameFile)
		{
			// Имена файлов dvd удовлетворяют правилу (8 символов – название файла, 3 – его расширение)
			if (nameFile.length() == 12)
			{
				static const string video_ts = "VIDEO_TS";
				if (nameFile.compare(0, video_ts.size(), video_ts) == 0) // имя файла
				{
					static const string bup = "BUP";
					static const string ifo = "IFO";
					static const string vob = "VOB";
					
					if (nameFile.compare(9, 3, bup) == 0 || nameFile.compare(9, 3, ifo) == 0 || nameFile.compare(9, 3, vob) == 0) // расширение файла
					{
						return true;
					}
					
					// Разбираем имя вида "VTS_03_1"
					static const string vts = "VTS";
					if (nameFile.compare(0, 3, vts) == 0 && isdigit(nameFile[4]) && isdigit(nameFile[5]) && isdigit(nameFile[7]))
					{
						return true;
					}
				}
			}
			
			return false;
		}
		
		FileImage()
#ifdef _DEBUG
			: m_imageCount(-1)
#endif
		{
		}
		void init();
	private:
		typedef boost::unordered_map<string, int> ImageMap;
		ImageMap m_indexis;
		int m_imageCount;
		//FastCriticalSection m_cs;// [!] IRainman opt: use spin lock here. [!] Delete this after move getIconIndex to draw function.
};

extern FileImage g_fileImage;

class UserImage : public BaseImageList
{
	public:
		void init();
		void reinit() // User Icon Begin / User Icon End
		{
			uninit();
			init();
		}
};
class UserStateImage : public BaseImageList
{
	public:
		void init();
};

extern UserImage g_userImage;
extern UserStateImage g_userStateImage;

class ISPImage : public BaseImageList
{
	public:
		uint8_t m_flagImageCount;
		ISPImage() : m_flagImageCount(0)
		{
		}
		void init();
};
extern ISPImage g_ISPImage;

class FlagImage : public BaseImageList
{
	public:
		uint8_t m_flagImageCount;
		FlagImage() : m_flagImageCount(0)
		{
		}
		void init();
		using BaseImageList::Draw;
#ifdef FLYLINKDC_USE_GEO_IP
		void DrawCountry(HDC p_DC, const Util::CustomNetworkIndex& p_country, const POINT& p_pt)
		{
			if (p_country.getCountryIndex() > 0)
			{
				Draw(p_DC, p_country.getCountryIndex(), p_pt);
			}
		}
#endif
		void DrawLocation(HDC p_DC, const Util::CustomNetworkIndex& p_location, const POINT& p_pt)
		{
			Draw(p_DC, p_location.getFlagIndex() + m_flagImageCount, p_pt);
		}
};

extern FlagImage g_flagImage;

#ifdef SCALOLAZ_MEDIAVIDEO_ICO
class VideoImage : public BaseImageList
{
	public:
		void init();
		static int getMediaVideoIcon(const tstring& p_col);
};

extern VideoImage g_videoImage;
#endif

struct Colors
{
	static void init();
	static void uninit()
	{
		::DeleteObject(bgBrush);
	}
	static void getUserColor(const UserPtr& user, COLORREF &fg, COLORREF &bg, const OnlineUserPtr& onlineUser = nullptr); // !SMT!-UI
	
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
	// [+] IRainman The alternation of the background color in lists.
	static void alternationBkColor(LPNMLVCUSTOMDRAW cd)
	{
		if (BOOLSETTING(USE_CUSTOM_LIST_BACKGROUND))
		{
			alternationBkColorAlways(cd);
		}
	}
	
	static LRESULT alternationonCustomDraw(LPNMHDR pnmh, BOOL& bHandled);
	static void alternationBkColorAlways(LPNMLVCUSTOMDRAW cd)
	{
		if (cd->nmcd.dwItemSpec % 2 != 0)
		{
			cd->clrTextBk = HLS_TRANSFORM(((cd->clrTextBk == CLR_DEFAULT) ? ::GetSysColor(COLOR_WINDOW) : cd->clrTextBk), -9, 0);
		}
		/*
		Error #315: UNINITIALIZED READ: reading 0x0025db48-0x0025db4c 4 byte(s)
		# 0 Colors::alternationBkColorAlways                                           [c:\vc10\r5xx\windows\winutil.h:1347]
		# 1 Colors::alternationonCustomDraw                                            [c:\vc10\r5xx\windows\winutil.cpp:4293]
		# 2 TypedListViewCtrl<SearchFrame::HubInfo,1181>::onCustomDraw                 [c:\vc10\r5xx\windows\typedlistviewctrl.h:719]
		# 3 SearchFrame::ProcessWindowMessage                                          [c:\vc10\r5xx\windows\searchfrm.h:83]
		# 4 ATL::CWindowImplBaseT<WTL::CMDIWindow,ATL::CWinTraits<1456406528,64> >::WindowProc [c:\program files (x86)\microsoft visual studio 10.0\vc\atlmfc\include\atlwin.h:3503]
		# 5 USER32.dll!gapfnScSendMessage                                             +0x331    (0x766b62fa <USER32.dll+0x162fa>)
		# 6 USER32.dll!GetThreadDesktop                                               +0xd6     (0x766b6d3a <USER32.dll+0x16d3a>)
		# 7 USER32.dll!GetWindow                                                      +0x3ef    (0x766b965e <USER32.dll+0x1965e>)
		# 8 USER32.dll!SendMessageW                                                   +0x4b     (0x766b96c5 <USER32.dll+0x196c5>)
		# 9 COMCTL32.dll!DetachScrollBars                                             +0x1fb    (0x71ebc05d <COMCTL32.dll+0x2c05d>)
		#10 COMCTL32.dll!ImageList_CoCreateInstance                                   +0x770    (0x71eb5eba <COMCTL32.dll+0x25eba>)
		#11 COMCTL32.dll!Ordinal395                                                   +0x40d    (0x71eb2a3b <COMCTL32.dll+0x22a3b>)
		Note: @0:01:22.093 in thread 6916
		Note: instruction: cmp    0x34(%edx) $0xff000000
		*/
	}
	
	static COLORREF getAlternativBkColor(LPNMLVCUSTOMDRAW cd)
	{
		return ((BOOLSETTING(USE_CUSTOM_LIST_BACKGROUND) &&
		         cd->nmcd.dwItemSpec % 2 != 0) ? HLS_TRANSFORM(Colors::bgColor, -9, 0) : Colors::bgColor);
	}
	// [~] IRainman
#else
	inline static COLORREF getAlternativBkColor(LPNMLVCUSTOMDRAW cd)
	{
		return Colors::bgColor;
	}
	
#endif
	
	// [+] SSA
	static bool getColorFromString(const tstring& colorText, COLORREF& color);
	
	static CHARFORMAT2 g_TextStyleTimestamp;
	static CHARFORMAT2 g_ChatTextGeneral;
	static CHARFORMAT2 g_ChatTextOldHistory;
	static CHARFORMAT2 g_TextStyleMyNick;
	static CHARFORMAT2 g_ChatTextMyOwn;
	static CHARFORMAT2 g_ChatTextServer;
	static CHARFORMAT2 g_ChatTextSystem;
	static CHARFORMAT2 g_TextStyleBold;
	static CHARFORMAT2 g_TextStyleFavUsers;
	static CHARFORMAT2 g_TextStyleFavUsersBan;
	static CHARFORMAT2 g_TextStyleOPs;
	static CHARFORMAT2 g_TextStyleURL;
	static CHARFORMAT2 g_ChatTextPrivate;
	static CHARFORMAT2 g_ChatTextLog;
	
	static COLORREF textColor;
	static COLORREF bgColor;
	
	static HBRUSH bgBrush;
};

struct Fonts
{
	static void init();
	static void uninit()
	{
		::DeleteObject(g_font);
		g_font = nullptr;
		::DeleteObject(g_boldFont);
		g_boldFont = nullptr;
		::DeleteObject(g_halfFont);
		g_halfFont = nullptr;
		::DeleteObject(g_systemFont);
		g_systemFont = nullptr;
	}
	
	static void decodeFont(const tstring& setting, LOGFONT &dest);
	
	static int g_fontHeight;
	static HFONT g_font;
	static HFONT g_boldFont;
	static HFONT g_systemFont;
	static HFONT g_halfFont;
};

class LastDir
{
	public:
		static const TStringList& get()
		{
			return g_dirs;
		}
		static void add(const tstring& dir)
		{
			if (find(g_dirs.begin(), g_dirs.end(), dir) != g_dirs.end())
			{
				return;
			}
			if (g_dirs.size() == 10)
			{
				g_dirs.erase(g_dirs.begin());
			}
			g_dirs.push_back(dir);
		}
		static void appendItem(OMenu& p_menu, int& p_num)
		{
			if (!g_dirs.empty())
			{
				p_menu.InsertSeparatorLast(TSTRING(PREVIOUS_FOLDERS));
				for (auto i = g_dirs.cbegin(); i != g_dirs.cend(); ++i)
				{
					p_menu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TARGET + (++p_num), Text::toLabel(*i).c_str());
				}
			}
		}
	private:
		static TStringList g_dirs;
};

class WinUtil
{
	public:
		// !SMT!-UI search user by exact share size
		//typedef std::unordered_multimap<uint64_t, UserPtr> ShareMap;
//		typedef ShareMap::iterator ShareIter;
		//static ShareMap UsersShare;
		
		struct TextItem
		{
			WORD itemID;
			ResourceManager::Strings translatedString;
		};
		
		static CMenu mainMenu;
		static OMenu copyHubMenu; // [+] IRainman fix.
		
		static HIconWrapper g_banIconOnline; // !SMT!-UI
		static HIconWrapper g_banIconOffline; // !SMT!-UI
		static HIconWrapper g_hMedicalIcon;
		static HIconWrapper g_hThermometerIcon;
		//static HIconWrapper g_hCrutchIcon;
		static HIconWrapper g_hFirewallIcon;
		
		static std::unique_ptr<HIconWrapper> g_HubOnIcon;
		static std::unique_ptr<HIconWrapper> g_HubOffIcon;
		static std::unique_ptr<HIconWrapper> g_HubFlylinkDCIcon;
		
		static void initThemeIcons()
		{
			g_HubOnIcon  = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_HUB));
			g_HubOffIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_HUB_OFF));
			g_HubFlylinkDCIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_MAINFRAME));
		}
		static HWND mainWnd;
		static HWND mdiClient;
#ifdef RIP_USE_SKIN
		static ITabCtrl* g_tabCtrl;
#else
		static FlatTabCtrl* g_tabCtrl;
#endif
		static HHOOK g_hook;
		static bool isAppActive;
		static bool mutesounds;
		
		static void init(HWND hWnd);
		static void uninit();
		
		static tstring getAppName()
		{
			LocalArray<TCHAR, MAX_PATH> buf;
			const DWORD x = GetModuleFileName(NULL, buf.data(), MAX_PATH);
			return tstring(buf.data(), x);
		}
		
		static int GetMenuItemPosition(const CMenu &p_menu, UINT_PTR p_IDItem = 0); // [+] SCALOlaz
		
		/**
		 * Check if this is a common /-command.
		 * @param cmd The whole text string, will be updated to contain only the command.
		 * @param param Set to any parameters.
		 * @param message Message that should be sent to the chat.
		 * @param status Message that should be shown in the status line.
		 * @return True if the command was processed, false otherwise.
		 */
		static bool checkCommand(tstring& cmd, tstring& param, tstring& message, tstring& status, tstring& local_message);
		
		static LONG getTextWidth(const tstring& str, HWND hWnd)
		{
			LONG sz = 0;
			dcassert(str.length());
			if (str.length())
			{
				const HDC dc = ::GetDC(hWnd);
				sz = getTextWidth(str, dc);
				const int l_res = ::ReleaseDC(mainWnd, dc);
				dcassert(l_res);
			}
			return sz;
		}
		static LONG getTextWidth(const tstring& str, HDC dc)
		{
			SIZE sz = { 0, 0 };
			dcassert(str.length());
			if (str.length())
			{
				::GetTextExtentPoint32(dc, str.c_str(), str.length(), &sz); //-V107
			}
			return sz.cx;
		}
		
		static LONG getTextHeight(HWND wnd, HFONT fnt)
		{
			const HDC dc = ::GetDC(wnd);
			const LONG h = getTextHeight(dc, fnt);
			const int l_res = ::ReleaseDC(wnd, dc);
			dcassert(l_res);
			return h;
		}
		
		static LONG getTextHeight(HDC dc, HFONT fnt)
		{
			const HGDIOBJ old = ::SelectObject(dc, fnt);
			const LONG h = getTextHeight(dc);
			::SelectObject(dc, old);
			return h;
		}
		
		static LONG getTextHeight(HDC dc)
		{
			TEXTMETRIC tm;
			dcdrun(const BOOL l_res =) ::GetTextMetrics(dc, &tm);
			dcassert(l_res);
			return tm.tmHeight;
		}
		
		static void setClipboard(const tstring& str);
		static void setClipboard(const string& str)
		{
			setClipboard(Text::toT(str));
		}
		
		static uint32_t percent(int32_t x, uint8_t percent)
		{
			return x * percent / 100;
		}
		static void unlinkStaticMenus(CMenu &menu); // !SMT!-UI
		
	private:
		dcdrun(static bool g_staticMenuUnlinked;)
	public:
	
		static tstring encodeFont(const LOGFONT& font);
		
		static bool browseFile(tstring& target, HWND owner = NULL, bool save = true, const tstring& initialDir = Util::emptyStringT, const TCHAR* types = NULL, const TCHAR* defExt = NULL);
		static bool browseDirectory(tstring& target, HWND owner = NULL);
		
		// Hash related
#ifdef PPA_INCLUDE_BITZI_LOOKUP
		static void bitziLink(const TTHValue& /*aHash*/);
#endif
		static void copyMagnet(const TTHValue& /*aHash*/, const string& /*aFile*/, int64_t);
		
		static void searchHash(const TTHValue& /*aHash*/);
		static void searchFile(const string& p_File);
		
		enum DefinedMagnetAction { MA_DEFAULT, MA_ASK, MA_DOWNLOAD, MA_SEARCH, MA_OPEN };
		
		// URL related
		static void registerDchubHandler();
		static void registerNMDCSHandler();
		static void registerADChubHandler();
		static void registerADCShubHandler();
		static void registerMagnetHandler();
		static void registerDclstHandler();// [+] IRainman dclst support
		static void unRegisterDchubHandler();
		static void unRegisterNMDCSHandler();
		static void unRegisterADChubHandler();
		static void unRegisterADCShubHandler();
		static void unRegisterMagnetHandler();
		static void unRegisterDclstHandler();// [+] IRainman dclst support
		static bool parseDchubUrl(const tstring& aUrl);// [!] IRainman stop copy-past!
		//static void parseADChubUrl(const tstring& /*aUrl*/, bool secure);[-] IRainman stop copy-past!
		static bool parseMagnetUri(const tstring& aUrl, DefinedMagnetAction Action = MA_DEFAULT
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		                                                                             , bool viewMediaIfPossible = false
#endif
		                          ); // [!] IRainman opt: return status.
		static void OpenFileList(const tstring& filename, DefinedMagnetAction Action = MA_DEFAULT); // [+] IRainman dclst support
		static bool parseDBLClick(const tstring& /*aString*/, string::size_type start, string::size_type end);
		static bool urlDcADCRegistered;
		static bool urlMagnetRegistered;
		static bool DclstRegistered;
		static int textUnderCursor(POINT p, CEdit& ctrl, tstring& x);
		static void openBitTorrent(const tstring& p_magnetURI);//[+]IRainman
		static void translateLinkToextProgramm(const tstring& url, const tstring& p_Extension = Util::emptyStringT, const tstring& p_openCmd = Util::emptyStringT);//[+]FlylinkDC
		static bool openLink(const tstring& url); // [!] IRainman opt: return status.
		static void openFile(const tstring& file);
		static void openFile(const TCHAR* file);
		static void openLog(const string& dir, const StringMap& params, const tstring& nologmessage); // [+] IRainman copy-past fix.
		
		static void openFolder(const tstring& file);
		
		static double toBytes(TCHAR* aSize);
		
		//returns the position where the context menu should be
		//opened if it was invoked from the keyboard.
		//aPt is relative to the screen not the control.
		static void getContextMenuPos(CListViewCtrl& aList, POINT& aPt);
		static void getContextMenuPos(CTreeViewCtrl& aTree, POINT& aPt);
		static void getContextMenuPos(CEdit& aEdit,         POINT& aPt);
		
		static bool getUCParams(HWND parent, const UserCommand& cmd, StringMap& sm);
		
		/** @return Pair of hubnames as a string and a bool representing the user's online status */
		static pair<tstring, bool> getHubNames(const CID& cid, const string& hintUrl);
		static pair<tstring, bool> getHubNames(const UserPtr& u, const string& hintUrl);
		static pair<tstring, bool> getHubNames(const CID& cid, const string& hintUrl, bool priv);
		static pair<tstring, bool> getHubNames(const HintedUser& user);
		static void splitTokens(int* p_array, const string& p_tokens, int p_maxItems) noexcept;
		static void splitTokensWidth(int* p_array, const string& p_tokens, int p_maxItems) noexcept; //[+] FlylinkDC++ Team
		static void saveHeaderOrder(CListViewCtrl& ctrl, SettingsManager::StrSetting order,
		                            SettingsManager::StrSetting widths, int n, int* indexes, int* sizes) noexcept; // !SMT!-UI todo: disable - this routine does not save column visibility
		static bool isShift()
		{
			return (GetKeyState(VK_SHIFT) & 0x8000) > 0;
		}
		static bool isAlt()
		{
			return (GetKeyState(VK_MENU) & 0x8000) > 0;
		}
		static bool isCtrl()
		{
			return (GetKeyState(VK_CONTROL) & 0x8000) > 0;
		}
		static bool isCtrlOrAlt()
		{
			return isCtrl() || isAlt();
		}
		
		static tstring escapeMenu(tstring str)
		{
			string::size_type i = 0;
			while ((i = str.find(_T('&'), i)) != string::npos)
			{
				str.insert(str.begin() + i, 1, _T('&'));
				i += 2;
			}
			return str;
		}
		template<class T> static HWND hiddenCreateEx(T& p) noexcept
		{
			HWND active = (HWND)::SendMessage(mdiClient, WM_MDIGETACTIVE, 0, 0);
			CFlyLockWindowUpdate l(mdiClient);
			HWND ret = p.CreateEx(mdiClient);
			if (active && ::IsWindow(active))
				::SendMessage(mdiClient, WM_MDIACTIVATE, (WPARAM)active, 0);
			return ret;
		}
		template<class T> static HWND hiddenCreateEx(T* p) noexcept
		{
			return hiddenCreateEx(*p);
		}
		
		static void translate(HWND page, TextItem* textItems)
		{
			if (textItems != NULL)
			{
				for (size_t i = 0; textItems[i].itemID != 0; i++)
				{
					::SetDlgItemText(page, textItems[i].itemID,
					                 Text::toT(ResourceManager::getString(textItems[i].translatedString)).c_str());
				}
			}
		}
		
//[-]PPA    static string disableCzChars(string message);
		static bool shutDown(int action);
		static int setButtonPressed(int nID, bool bPressed = true);
//TODO      static bool checkIsButtonPressed(int nID);// [+] IRainman

#ifdef FLYLINKDC_USE_LIST_VIEW_WATER_MARK
		static bool setListCtrlWatermark(HWND hListCtrl, UINT nID, COLORREF clr, int width = 128, int height = 128); // [+] InfinitySky. PNG Support from Apex 1.3.8.
#endif
		static string getWMPSpam(HWND playerWnd = NULL);
		static string getItunesSpam(HWND playerWnd = NULL);
		static string getMPCSpam();
		static string getWinampSpam(HWND playerWnd = NULL, int playerType = 0);
		static string getJASpam();
		
		
// FDM extension
		static tstring getNicks(const CID& cid, const string& hintUrl);
		static tstring getNicks(const UserPtr& u, const string& hintUrl);
		static tstring getNicks(const CID& cid, const string& hintUrl, bool priv);
		static tstring getNicks(const HintedUser& user);
		
		static int getFlagIndexByName(const char* countryName);
		
		// [+] IRainman optimize.
		static int& GetTabsPosition()
		{
			return g_tabPos;
		}
		static void SetTabsPosition(int p_NewTabsPosition)
		{
			g_tabPos = p_NewTabsPosition;
		}
		// [~] IRainman optimize.
		static bool isUseExplorerTheme();
		static void SetWindowThemeExplorer(HWND p_hWnd);
		
		static tstring getAddresses(CComboBox& BindCombo); // [<-] IRainman moved from Network Page.
		
		static void GetTimeValues(CComboBox& p_ComboBox); // [+] InfinitySky.
		
		static void TextTranscode(CEdit& ctrlMessage); // [+] Drakon
		
		static void SetBBCodeForCEdit(CEdit& ctrlMessage, WORD wID); // [+] SSA
		static tstring getFilenameFromString(const tstring& filename);
		
		static time_t  filetime_to_timet(const FILETIME & ft)
		{
			ULARGE_INTEGER ull;
			ull.LowPart = ft.dwLowDateTime;
			ull.HighPart = ft.dwHighDateTime;
			return ull.QuadPart / 10000000ULL - 11644473600ULL;
		}
		
		static BOOL FillRichEditFromString(HWND hwndEditControl, const string& rtfFile);  // [+] SSA
		
		struct userStreamIterator
		{
			userStreamIterator() : position(0), length(0) {}
			
			unique_ptr<uint8_t[]> data;
			int position;
			int length;
		};
		
#ifdef SSA_SHELL_INTEGRATION
		static wstring getShellExtDllPath();
		static bool makeShellIntegration(bool isNeedUnregistred);
#endif
		static bool runElevated(HWND    hwnd, LPCTSTR pszPath, LPCTSTR pszParameters = NULL, LPCTSTR pszDirectory = NULL);
		
		template<class M>
		static string getDataFromMap(int p_ComboIndex, const M& p_Map)
		{
			if (p_ComboIndex >= 0)
			{
				int j = 0;
				for (auto i = p_Map.cbegin(); i != p_Map.cend(); ++i, ++j)
					if (p_ComboIndex == j)
						return i->second;
			}
			return Util::emptyString;
		}
		
		template<class M, typename T>
		static int getIndexFromMap(const M& p_Map, const T& p_Data)
		{
			int j = 0;
			for (auto i = p_Map.cbegin(); i != p_Map.cend(); ++i, ++j)
				if (p_Data == i->second)
					return j;
			return -1;
		}
		
		static void safe_sh_free(void* p_ptr)
		{
			IMalloc * l_imalloc = nullptr;
			if (SUCCEEDED(SHGetMalloc(&l_imalloc)))
			{
				l_imalloc->Free(p_ptr);
				safe_release(l_imalloc);
			}
		}
		
		static bool AutoRunShortCut(bool bCreate);
		static bool IsAutoRunShortCutExists();
		static tstring GetAutoRunShortCutName();
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		static void StartPreviewClient();
#endif
		static tstring GetLang()
		{
			return Text::toT(Util::getLang());
		}
		static tstring GetWikiLink()
		{
			return Text::toT(Util::getWikiLink());
		}
		
		// [+] IRainman fix.
		template<class xxString>
		static size_t GetWindowText(xxString& text, const HWND hWndCtl)
		{
			text.resize(::GetWindowTextLength(hWndCtl));
			if (text.size() > 0)
				::GetWindowText(hWndCtl, &text[0], text.size() + 1);
				
			return text.size();
		}
		template<class xxString, class xxComponent>
		static size_t GetWindowText(xxString& text, const xxComponent& element)
		{
			text.resize(element.GetWindowTextLength());
			if (text.size() > 0)
			{
				element.GetWindowText(&text[0], text.size() + 1);
			}
			return text.size();
		}
		static bool GetDlgItemText(HWND p_Dlg, int p_ID, tstring& p_str);
		
		static tstring fromAtlString(const CAtlString& str)
		{
			return tstring(str, str.GetLength());
		}
		static CAtlString toAtlString(const tstring& str)
		{
			return CAtlString(str.c_str(), str.size());
		}
		static CAtlString toAtlString(const string& str)
		{
			return toAtlString(Text::toT(str));
		}
		// [~] IRainman fix.
		
		static tstring getCommandsList();
	private:
		static int CALLBACK browseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lp*/, LPARAM pData);
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
		static bool FillCustomMenu(CMenuHandle &menu, string& menuName); // [+]  SSA: Custom menu support.
#endif
		static inline TCHAR CharTranscode(const TCHAR msg); // [+] Drakon. Transcoding text between Russian & English
		static bool   CreateShortCut(const tstring& pszTargetfile, const tstring& pszTargetargs, const tstring& pszLinkfile, const tstring& pszDescription, int iShowmode, const tstring& pszCurdir, const tstring& pszIconfile, int iIconindex);
		
		static DWORD CALLBACK EditStreamCallback(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb); // [+] SSA
		
		static int g_tabPos;
};

#endif // !defined(WIN_UTIL_H