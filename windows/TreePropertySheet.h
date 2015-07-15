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

#if !defined(TREE_PROPERTY_SHEET_H)
#define TREE_PROPERTY_SHEET_H

#include "resource.h"
#include "../client/SettingsManager.h"

#ifdef SCALOLAZ_PROPPAGE_HELPLINK
#include <atldlgs.h>
#endif

#include "wtl_flylinkdc.h"

//#define SCALOLAZ_PROPPAGE_CAMSHOOT

#ifdef SCALOLAZ_PROPPAGE_CAMSHOOT
#include "ResourceLoader.h"
#include "HIconWrapper.h"
#endif

class TreePropertySheet : public CPropertySheetImpl<TreePropertySheet>,
	protected CFlyTimerAdapter
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		virtual ~TreePropertySheet()
		{
			tree_icons.Destroy();
		}
		enum { WM_USER_INITDIALOG = WM_APP + 501 };
		enum { TAB_MESSAGE_MAP = 13 };
		TreePropertySheet(ATL::_U_STRINGorID title = (LPCTSTR)NULL, UINT uStartPage = 0, HWND hWndParent = NULL) :
			CPropertySheetImpl<TreePropertySheet>(title, uStartPage, hWndParent)
			, tabContainer(WC_TABCONTROL, this, TAB_MESSAGE_MAP)
			, CFlyTimerAdapter(m_hWnd)
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
			, m_SliderPos(255)
#endif
			, m_offset(0)
		{
		
			m_psh.pfnCallback = &PropSheetProc;
			m_psh.dwFlags |= PSH_RTLREADING;
		}
		virtual void onTimerSec()
		{
		}
		typedef CPropertySheetImpl<TreePropertySheet> baseClass;
		BEGIN_MSG_MAP(TreePropertySheet)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_COMMAND, baseClass::OnCommand)
		MESSAGE_HANDLER(WM_USER_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_NOTIFYFORMAT, onNotifyFormat)
		NOTIFY_HANDLER(IDC_PAGE, TVN_SELCHANGED, onSelChanged)
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
		MESSAGE_HANDLER(WM_HSCROLL, onTranspChanged)    // TrackBar control
#endif
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(TAB_MESSAGE_MAP)
		MESSAGE_HANDLER(TCM_SETCURSEL, onSetCurSel)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSetCurSel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onSelChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
		LRESULT onTranspChanged(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#endif
		LRESULT onNotifyFormat(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
#ifdef _UNICODE
			return NFR_UNICODE;
#else
			return NFR_ANSI;
#endif
		}
		
		static int CALLBACK PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam);
		int  m_offset;
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
		void addTransparency();
		void setTransp(int p_Layered);
		CTrackBarCtrl m_Slider;
		CFlyToolTipCtrl m_tooltip;  // [+] SCALOlaz: add tooltips
		uint8_t m_SliderPos;
#endif
#ifdef SCALOLAZ_PROPPAGE_CAMSHOOT
		void addCam();
		void doCamShoot();
		LRESULT onCamShoot(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
		{
			doCamShoot();
			return 0;
		}
		CButton* m_Cam;
		ExCImage g_CamPNG;
		CFlyToolTipCtrl* m_Camtooltip;
#endif
#ifdef SCALOLAZ_PROPPAGE_HELPLINK
		void addHelp();
		void genHelpLink(int p_page);
		tstring genPropPageName(int p_page);
		CHyperLink m_Help;
#endif
	private:
	
		enum
		{
			SPACE_MID = 1, // [~] JhaoDa
			SPACE_TOP = 10,
			SPACE_BOTTOM = 1, // [~] JhaoDa
			SPACE_LEFT = 10,
			SPACE_RIGHT = 6, // [~] JhaoDa
			TREE_WIDTH = 245
		};
		
		static const int MAX_NAME_LENGTH = 256;
		
		void hideTab();
		void addTree();
		void fillTree();
		
		HTREEITEM createTree(const tstring& str, HTREEITEM parent, int page);
		HTREEITEM findItem(const tstring& str, HTREEITEM start);
		HTREEITEM findItem(int page, HTREEITEM start);
		
		CImageList tree_icons;
		CTreeViewCtrl ctrlTree;
		CContainedWindow tabContainer;
};

#endif // !defined(TREE_PROPERTY_SHEET_H)

/**
* @file
* $Id: TreePropertySheet.h,v 1.10 2006/05/08 08:36:20 bigmuscle Exp $
*/
