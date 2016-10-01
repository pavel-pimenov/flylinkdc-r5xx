/*
 * Copyright (C) 2011-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#if !defined(CFLY_SERVER_NAVIGATOR_DLG_H)
#define CFLY_SERVER_NAVIGATOR_DLG_H

#include "PropertyList.h"

#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER

// #include "MiniHTML.h" TODO - не пашет. разобраться со скроллером т.к. он не появляется.
typedef std::vector< std::pair<tstring, tstring> > TStringPairArray;

//#define USE_FLY_SERVER_USE_IE_EXPLORER // Пока глючит

class CFlyServerDialogNavigator :
#ifdef USE_FLY_SERVER_USE_IE_EXPLORER
	public CAxDialogImpl<CFlyServerDialogNavigator>
	public CWinDataExchange<CFlyServerDialogNavigator>,
	public IDispEventImpl<IDC_FLY_SERVER_EXPLORER, CFlyServerDialogNavigator>
#else
	public CDialogImpl<CFlyServerDialogNavigator>,
	public CDialogResize<CFlyServerDialogNavigator>
#endif
{
	public:
		// TODO завернуть это в мапу?
		TStringPairArray m_MediaInfo;
		TStringPairArray m_FileInfo;
		TStringPairArray m_MIInform;
		
	private:
		CPropertyListCtrl m_ctlList;
		//CMiniHtmlCtrl m_ctrlLabel;
		CEdit m_ctrlLabel;
		
#ifdef USE_FLY_SERVER_USE_IE_EXPLORER
		CComQIPtr<IWebBrowser2> m_pWebBrowser;
		IUnknown *m_pUnknown;
#endif
		
	public:
		enum { IDD = IDD_FLY_SERVER_DIALOG };
		
		BEGIN_MSG_MAP(CNavigatorDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		// NOTIFY_HANDLER(IDC_FLY_SERVER_LISTBOX, PIN_BROWSE, OnBrowse)
		
#ifdef USE_FLY_SERVER_USE_IE_EXPLORER
//		COMMAND_HANDLER(IDC_FLY_SERVER_BACK, BN_CLICKED, OnBack)
//		COMMAND_HANDLER(IDC_FLY_SERVER_FORWARD, BN_CLICKED, OnForward)
//		COMMAND_HANDLER(IDC_FLY_SERVER_STOP, BN_CLICKED, OnStop)
		COMMAND_HANDLER(IDC_FLY_SERVER_REFRESH, BN_CLICKED, OnRefresh)
//		COMMAND_HANDLER(IDC_GO, BN_CLICKED, OnGo)
#endif
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		REFLECT_NOTIFICATIONS()
		CHAIN_MSG_MAP(CDialogResize<CFlyServerDialogNavigator>)
		END_MSG_MAP()
		
		BEGIN_DLGRESIZE_MAP(CFlyServerDialogNavigator)
		DLGRESIZE_CONTROL(IDC_FLY_SERVER_LISTBOX, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_MEDIAINFORM_STATIC, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		
		END_DLGRESIZE_MAP()
	private:
	
#ifdef USE_FLY_SERVER_USE_IE_EXPLORER
	
		BEGIN_SINK_MAP(CFlyServerDialogNavigator)
		SINK_ENTRY(IDC_FLY_SERVER_EXPLORER, 0x69, OnCommandStateChangeExplorer)
		SINK_ENTRY(IDC_FLY_SERVER_EXPLORER, 0xfa, OnBeforeNavigate2Explorer)
		END_SINK_MAP()
#endif
		void addArray(const TCHAR* p_name, const TStringPairArray& p_array)
		{
			if (!p_array.empty())
			{
				m_ctlList.AddItem(PropCreateCategory(p_name));
				for (auto i = p_array.begin(); i != p_array.end(); ++i)
				{
					if (!i->second.empty())
						m_ctlList.AddItem(PropCreateSimple(i->first.c_str(), i->second.c_str()));
				}
			}
		}
		
		LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
		{
			CenterWindow();
//	   m_ctlList.AddItem(new CProperty(PROP_GROUP,_T("General"),new CComVariant (L"Item-1"),true));
//	   m_ctlList.AddItem(new CProperty(PROP_ITEM,_T("Attr-1"),new CComVariant (L"Value-1"),true));

			m_ctlList.SubclassWindow(GetDlgItem(IDC_FLY_SERVER_LISTBOX));
			m_ctlList.SetExtendedListStyle(PLS_EX_CATEGORIZED);
			m_ctlList.SetColumnWidth(100);
			//  m_ctlList.m_bReadOnly = TRUE;
			addArray(_T("FileInfo"), m_FileInfo);
			addArray(_T("MediaInfo"), m_MediaInfo);
			/*
			// TODO - убрать копипаст
			      if(!m_FileInfo.empty())
			      {
			      m_ctlList.AddItem( PropCreateCategory(_T("FileInfo")) );
			      for (auto i = m_FileInfo.begin(); i!=m_FileInfo.end(); ++i)
			      {
			          if(!i->second.empty())
			            m_ctlList.AddItem(PropCreateSimple(i->first.c_str(),i->second.c_str()));
			      }
			      }
			      // TODO - убрать копипаст
			      if(!m_MediaInfo.empty())
			      {
			      m_ctlList.AddItem( PropCreateCategory(_T("MediaInfo")) );
			      for (auto i = m_MediaInfo.begin(); i!=m_MediaInfo.end(); ++i)
			      {
			          if(!i->second.empty())
			             m_ctlList.AddItem(PropCreateSimple(i->first.c_str(),i->second.c_str()));
			      }
			      }
			      */
			// m_ctlList.SetReadOnly(TRUE);
			// m_ctrlLabel.SubclassWindow(GetDlgItem(IDC_MEDIAINFORM_STATIC));
			m_ctrlLabel.Attach(GetDlgItem(IDC_MEDIAINFORM_STATIC));
			m_ctrlLabel.SetReadOnly(TRUE);
			if (!m_MIInform.empty())
				m_ctrlLabel.SetWindowText(m_MIInform[0].second.c_str());
			/*     m_ctlList.AddItem( PropCreateSimple(_T("Name"), _T("bjarke")) );
			      m_ctlList.AddItem( PropCreateSimple(_T("X\r\nX-2\r\nX-3\r\nX-4\r\n"), 123L) );
			      m_ctlList.AddItem( PropCreateSimple(_T("Y"), _T("1\r\n\2\n3")) );
			
			      m_ctlList.AddItem( PropCreateCategory(_T("General")) );
			      m_ctlList.AddItem( PropCreateSimple(_T("Enabled"), false) );
			      m_ctlList.AddItem( PropCreateFileName(_T("Picture"), _T("C:\\Temp\\Test.bmp")) );
			*/
			/*
			       m_ctlList.SetExtendedListStyle(PLS_EX_CATEGORIZED);
			       HPROPERTY hAppearance = m_ctlList.AddItem( PropCreateCategory(_T("Appearance"), 1234) );
			       HPROPERTY hName = m_ctlList.AddItem( PropCreateSimple(_T("Name"), _T("bjarke")) );
			       m_ctlList.AddItem( PropCreateSimple(_T("X"), 123L) );
			       m_ctlList.AddItem( PropCreateSimple(_T("Y"), 456L) );
			       m_ctlList.AddItem( PropCreateCategory(_T("Behaviour")) );
			       m_ctlList.AddItem( PropCreateSimple(_T("Enabled"), false) );
			       m_ctlList.AddItem( PropCreateFileName(_T("Picture"), _T("C:\\Temp\\Test.bmp")) );
			*/
#ifdef USE_FLY_SERVER_USE_IE_EXPLORER
			
			CWindow w = GetDlgItem(IDC_FLY_SERVER_EXPLORER);
			
			// Получаем указатель на интерфейс IWebBrowser2 и сохраняем его.
			AtlAxGetControl(w, &m_pUnknown);
			m_pWebBrowser = m_pUnknown;
			
			// Подключаемся к событиям броузера.
			AtlAdviseSinkMap(this, true);
			
			// Отправляемся на стартовую страницу.
			m_pWebBrowser->GoHome();
#endif
			DlgResize_Init();
			return 0;
		}
		
#ifdef USE_FLY_SERVER_USE_IE_EXPLORER
		
		LRESULT OnBack(WORD, WORD, HWND, BOOL&)
		{
			m_pWebBrowser->GoBack();
			return 0;
		}
		
		LRESULT OnForward(WORD, WORD, HWND, BOOL&)
		{
			m_pWebBrowser->GoForward();
			return 0;
		}
		
		LRESULT OnStop(WORD, WORD, HWND, BOOL&)
		{
			m_pWebBrowser->Stop();
			return 0;
		}
		
		LRESULT OnRefresh(WORD, WORD, HWND, BOOL&)
		{
			m_pWebBrowser->Refresh();
			return 0;
		}
		
		LRESULT OnGo(WORD, WORD, HWND, BOOL&)
		{
			DoDataExchange(TRUE);
			CComVariant vtEmpty;
			
			m_pWebBrowser->Navigate(m_bstrURL, &vtEmpty, &vtEmpty, &vtEmpty, &vtEmpty);
			return 0;
		}
		VOID __stdcall OnCommandStateChangeExplorer(LONG Command, VARIANT_BOOL Enable)
		{
			CWindow w = NULL;
			
			switch (Command)
			{
				case CSC_NAVIGATEFORWARD:
//			w = GetDlgItem(IDC_FLY_SERVER_FORWARD);
					break;
				case CSC_NAVIGATEBACK:
//			w = GetDlgItem(IDC_FLY_SERVER_BACK);
					break;
			}
			
			if (w != NULL)
				w.EnableWindow(Enable == VARIANT_TRUE);
		}
		
		VOID __stdcall OnBeforeNavigate2Explorer(IDispatch * pDisp, VARIANT * URL, VARIANT * Flags, VARIANT * TargetFrameName, VARIANT * PostData, VARIANT * Headers, VARIANT_BOOL * Cancel)
		{
			IUnknown *pUnk;
			pDisp->QueryInterface(IID_IUnknown, (void**)&pUnk);
			
			if (m_pUnknown == pUnk)
			{
				m_bstrURL = URL->bstrVal;
				//DoDataExchange(FALSE);
			}
			
			pUnk->Release();
		}
#endif
		LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			EndDialog(IDOK);
			return 0;
		}
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& bHandled)
		{
			if (wID == IDOK)
			{
			}
			EndDialog(wID);
			return 0;
		}
		
		
};

#endif // FLYLINKDC_USE_MEDIAINFO_SERVER

#endif // !defined(CFLY_SERVER_NAVIGATOR_DLG_H)
