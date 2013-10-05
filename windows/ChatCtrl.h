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

#include <AtlCrack.h>

class UserInfo;
typedef pair<int, tstring> TURLPair;
typedef list<TURLPair> TURLMap;

class ChatCtrl: public CWindowImpl<ChatCtrl, CRichEditCtrl>
#ifdef IRAINMAN_INCLUDE_SMILE
	, public IRichEditOleCallback
#endif
#ifdef _DEBUG
	, virtual NonDerivable<ChatCtrl>, boost::noncopyable // [+] IRainman fix.
#endif
{
		typedef ChatCtrl thisClass;
	protected:
		// TypedListViewCtrl<UserInfo, IDC_USERS> *m_pUsers;
		string sHubHint; // !SMT!-S [!] IRainman fix TODO.
		bool isOnline(const Client* client, const tstring& aNick) const; // !SMT!-S [!] IRainman opt: add client!
		
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
		LRESULT onWhoisIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
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
		void AppendText(const Identity& id, const bool bMyMess, const bool bThirdPerson, const tstring& sExtra, const tstring& sMsg, const CHARFORMAT2& cf
#ifdef IRAINMAN_INCLUDE_SMILE
		                , bool bUseEmo = true
#endif
		               );
		void AppendTextOnly(const tstring& sText, const tstring& sAuthor, const bool bMyMess, const bool bRealUser, const CHARFORMAT2& cf);
		
		void GoToEnd();
		bool GetAutoScroll() const
		{
			return m_boAutoScroll;
		}
		void SetAutoScroll(bool boAutoScroll);
		//void SetUsers(TypedListViewCtrl<UserInfo, IDC_USERS> *pUsers); // [!] IRainman fix: NO! :)
		
		void setHubParam(const string& sUrl, const string& sNick)
		{
			sMyNickLower = WinUtil::toAtlString(sNick);
			sMyNickLower.MakeLower();
			sHubHint = sUrl;    // !SMT!-S
		}
		const string& getHubHint() const// [+] IRainman fix.
		{
			return sHubHint;
		}
	public:
		// [~] IRainman fix, todo replace to tstring?
		void Clear() // [+] IRainman fix.
		{
			SetWindowText(Util::emptyStringT.c_str());
			lURLMap.clear();
		}
		void SetTextStyleMyNick(const CHARFORMAT2& ts)
		{
			Colors::g_TextStyleMyNick = ts;
		}
		LRESULT OnNotify(int idCtrl, ENLINK pnmh);
		
		static tstring sSelectedLine;
		static tstring sSelectedText;
		static tstring sSelectedIP;
		static tstring sSelectedUserName;
		static tstring sSelectedURL;
	private:
		CAtlString sMyNickLower; // [+] IRainman fix, todo replace to tstring?
		TURLMap lURLMap;
		const tstring& get_URL(ENLINK* p_EL) const
		{
			return get_URL(p_EL->chrg.cpMin/*, p_EL->chrg.cpMax*/);
		}
		const tstring& get_URL(const long lBegin/*, const long lEnd*/) const
		{
			for (auto i = lURLMap.cbegin(); i != lURLMap.cend(); ++i)
				if (i->first == lBegin)
					return i->second;
					
			return Util::emptyStringT;
		}
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
