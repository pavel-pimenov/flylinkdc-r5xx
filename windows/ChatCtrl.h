/*
 *
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

#ifndef CHAT_CTRL_H
#define CHAT_CTRL_H

#pragma once

#include "atlstr.h"
#include "TypedListViewCtrl.h"
#include "ImageDataObject.h"
#include "UserInfoSimple.h"
#ifdef IRAINMAN_INCLUDE_SMILE
#define WM_UPDATE_SMILE WM_USER + 1
#endif
#ifndef _RICHEDIT_VER
# define _RICHEDIT_VER 0x0300
#endif
#ifdef IRAINMAN_ENABLE_WHOIS
#include "../client/Util.h"
#endif
#include <AtlCrack.h>

class UserInfo;
typedef pair<int, tstring> TURLPair;
typedef list<TURLPair> TURLMap;

class ChatCtrl: public CWindowImpl<ChatCtrl, CRichEditCtrl>
#ifdef IRAINMAN_INCLUDE_SMILE
	, public IRichEditOleCallback
#endif
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
		typedef ChatCtrl thisClass;
	protected:
		string m_HubHint; // !SMT!-S [!] IRainman fix TODO.
		static bool isOnline(const Client* client, const tstring& aNick); // !SMT!-S [!] IRainman opt: add client!
		
		bool m_boAutoScroll;
#ifdef IRAINMAN_INCLUDE_SMILE
		IStorage *m_pStorage;
		IRichEditOle* m_pRichEditOle;
		LPLOCKBYTES m_lpLockBytes;
#endif // IRAINMAN_INCLUDE_SMILE
	public:
		ChatCtrl();
		~ChatCtrl();
		
		BEGIN_MSG_MAP(thisClass)
#ifdef IRAINMAN_INCLUDE_SMILE
		MESSAGE_HANDLER(WM_UPDATE_SMILE, onUpdateSmile)
#endif
		MESSAGE_HANDLER(WM_MOUSEWHEEL, onMouseWheel)
		NOTIFY_HANDLER(IDC_CLIENT, EN_LINK, onEnLink)
		COMMAND_ID_HANDLER(IDC_COPY_ACTUAL_LINE, onCopyActualLine)
		COMMAND_ID_HANDLER(IDC_COPY_URL, onCopyURL)
#ifdef IRAINMAN_ENABLE_WHOIS
		COMMAND_ID_HANDLER(IDC_WHOIS_IP, onWhoisIP)
		COMMAND_ID_HANDLER(IDC_WHOIS_IP2, onWhoisIP)
		COMMAND_ID_HANDLER(IDC_WHOIS_URL, onWhoisURL)
#endif
		COMMAND_ID_HANDLER(ID_EDIT_COPY, onEditCopy)
		COMMAND_ID_HANDLER(ID_EDIT_SELECT_ALL, onEditSelectAll)
		COMMAND_ID_HANDLER(ID_EDIT_CLEAR_ALL, onEditClearAll)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		END_MSG_MAP()
		
#ifdef IRAINMAN_INCLUDE_SMILE
		LRESULT onUpdateSmile(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
#endif
		LRESULT onMouseWheel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onEnLink(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
#ifdef IRAINMAN_ENABLE_WHOIS
		LRESULT onWhoisIP(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onWhoisURL(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditSelectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyActualLine(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyURL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		void Initialize();
		
		LRESULT OnRButtonDown(POINT pt, const UserPtr& user = nullptr);
		bool HitNick(const POINT& p, tstring& sNick, int& piBegin, int& piEnd, const UserPtr& user);
		bool HitIP(const POINT& p, tstring& sIP, int& piBegin, int& piEnd);
		bool HitURL(tstring& sURL, const long& lSelBegin/*, const long& lSelEnd*/) // [!] PVS V669 The 'lSelBegin', 'lSelEnd' arguments are non-constant references. The analyzer is unable to determine the position at which this argument is being modified. It is possible that the function contains an error. chatctrl.h 103
		{
			sURL = get_URL(lSelBegin/*, lSelEnd*/);
			return !sURL.empty();
		}
		bool HitText(tstring& sText, int lSelBegin, int lSelEnd)
		{
			sText.resize(static_cast<size_t>(lSelEnd - lSelBegin));
			GetTextRange(lSelBegin, lSelEnd, &sText[0]);
			return !sText.empty();
		}
		
		tstring LineFromPos(const POINT& p) const;
		
		void AdjustTextSize();
		
		struct CFlyChatCacheTextOnly
		{
			tstring m_Msg;
			tstring m_Nick;
			CHARFORMAT2 m_cf;
			bool m_bMyMess;
			bool m_isRealUser;
			CFlyChatCacheTextOnly(const tstring& p_nick,
			                      const bool p_is_my_mess,
			                      const bool p_is_real_user,
			                      const tstring& p_msg,
			                      const CHARFORMAT2& p_cf);
		};
		struct CFlyChatCache : public CFlyChatCacheTextOnly
		{
			tstring m_Extra;
			bool m_bThirdPerson;
			bool m_bUseEmo;
			bool m_is_url;
			bool m_isFavorite;
			bool m_is_ban;
			bool m_is_op;
			bool m_is_disable_style;
			CFlyChatCache(const Identity& p_id, const bool bMyMess, const bool bThirdPerson,
			              const tstring& sExtra, const tstring& sMsg, const CHARFORMAT2& cf, bool bUseEmo, bool p_is_remove_rn = true);
			size_t length() const
			{
				return m_Extra.size() + m_Msg.size() + m_Nick.size() + 30;
			}
		};
		
		void AppendText(const CFlyChatCache& p_message, bool p_is_lock_redraw);
		void AppendTextOnly(const tstring& sText, const CFlyChatCacheTextOnly& p_message);
		void AppendTextParseBB(CAtlString& sMsgLower, const CFlyChatCacheTextOnly& p_message, const LONG& lSelBegin);
		void AppendTextParseURL(CAtlString& sMsgLower, const CFlyChatCacheTextOnly& p_message, const LONG& lSelBegin);
		
	private:
		std::list<CFlyChatCache> m_chat_cache; // вектор нельзя - список пополняется
		size_t m_chat_cache_length;
		bool m_is_cache_chat_empty;
		bool m_is_out_of_memory_for_smile;
		FastCriticalSection m_fcs_chat_cache;
		void insertAndFormat(const tstring & text, CHARFORMAT2 cf, bool p_is_disable_style, LONG& p_begin, LONG& p_end);
	public:
		void disable_chat_cache()
		{
			m_is_cache_chat_empty = true;
		}
		void restore_chat_cache();
		
		void GoToEnd(bool p_force);
		void GoToEnd(POINT& p_scroll_pos, bool p_force);
		bool GetAutoScroll() const
		{
			return m_boAutoScroll;
		}
		void invertAutoScroll();
		void SetAutoScroll(bool boAutoScroll);
		
		void setHubParam(const string& sUrl, const string& sNick);
		const string& getHubHint() const// [+] IRainman fix.
		{
			return m_HubHint;
		}
	public:
		// [~] IRainman fix, todo replace to tstring?
		void Clear() // [+] IRainman fix.
		{
			SetWindowText(Util::emptyStringT.c_str());
			m_URLMap.clear();
		}
		void SetTextStyleMyNick(const CHARFORMAT2& ts)
		{
			Colors::g_TextStyleMyNick = ts;
		}
		LRESULT OnNotify(int idCtrl, ENLINK pnmh);
		
		static tstring g_sSelectedLine;
		static tstring g_sSelectedText;
		static tstring g_sSelectedIP;
		static tstring g_sSelectedUserName;
		static tstring g_sSelectedURL;
	private:
		CAtlString m_MyNickLower; // [+] IRainman fix, todo replace to tstring?
		TURLMap m_URLMap;
		const tstring& get_URL(ENLINK* p_EL) const;
		const tstring& get_URL(const long lBegin/*, const long lEnd*/) const;
		tstring get_URL_RichEdit(ENLINK* p_EL) const;
	public:
		static bool isGoodNickBorderSymbol(TCHAR ch); // [+] SSA
	protected:
#ifdef IRAINMAN_INCLUDE_SMILE
		volatile LONG m_Ref;
		// IRichEditOleCallback implementation
		
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE QueryInterface(THIS_ REFIID riid, LPVOID FAR * lplpObj);
		COM_DECLSPEC_NOTHROW ULONG STDMETHODCALLTYPE AddRef(THIS);
		COM_DECLSPEC_NOTHROW ULONG STDMETHODCALLTYPE Release(THIS);
		
		// *** IRichEditOleCallback methods ***
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetNewStorage(THIS_ LPSTORAGE FAR * lplpstg);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetInPlaceContext(THIS_ LPOLEINPLACEFRAME FAR * lplpFrame,
		                                                                 LPOLEINPLACEUIWINDOW FAR * lplpDoc,
		                                                                 LPOLEINPLACEFRAMEINFO lpFrameInfo);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ShowContainerUI(THIS_ BOOL fShow);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE QueryInsertObject(THIS_ LPCLSID lpclsid, LPSTORAGE lpstg,
		                                                                 LONG cp);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE DeleteObject(THIS_ LPOLEOBJECT lpoleobj);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE QueryAcceptData(THIS_ LPDATAOBJECT lpdataobj,
		                                                               CLIPFORMAT FAR * lpcfFormat, DWORD reco,
		                                                               BOOL fReally, HGLOBAL hMetaPict);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(THIS_ BOOL fEnterMode);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetClipboardData(THIS_ CHARRANGE FAR * lpchrg, DWORD reco,
		                                                                LPDATAOBJECT FAR * lplpdataobj);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetDragDropEffect(THIS_ BOOL fDrag, DWORD grfKeyState,
		                                                                 LPDWORD pdwEffect);
		COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetContextMenu(THIS_ WORD seltype, LPOLEOBJECT lpoleobj,
		                                                              CHARRANGE FAR * lpchrg,
		                                                              HMENU FAR * lphmenu);
#endif // IRAINMAN_INCLUDE_SMILE
};


#endif //!defined(AFX_CHAT_CTRL_H__595F1372_081B_11D1_890D_00A0244AB9FD__INCLUDED_)
