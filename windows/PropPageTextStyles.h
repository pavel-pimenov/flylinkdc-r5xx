#ifndef _PROP_PAGE_TEXT_STYLES_H_
#define _PROP_PAGE_TEXT_STYLES_H_


#pragma once


#include <atlcrack.h>
#include "PropPage.h"
#include "ChatCtrl.h"

class PropPageTextStyles: public CPropertyPage<IDD_TEXT_STYLES_PAGE>, public PropPage
{
	public:
		explicit PropPageTextStyles() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TEXT_STYLES))
		{
			fg = 0;
			bg = 0;
			m_BackColor = 0;
			m_ForeColor = 0;
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
			m_maincolor_changed = false;
		}
		~PropPageTextStyles()
		{
		}
		
		BEGIN_MSG_MAP_EX(PropPageTextStyles)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, onExport)
		COMMAND_HANDLER(IDC_BACK_COLOR, BN_CLICKED, onEditBackColor)
		COMMAND_HANDLER(IDC_TEXT_COLOR, BN_CLICKED, onEditForeColor)
		COMMAND_HANDLER(IDC_TEXT_STYLE, BN_CLICKED, onEditTextStyle)
		COMMAND_HANDLER(IDC_DEFAULT_STYLES, BN_CLICKED, onDefaultStyles)
		COMMAND_HANDLER(IDC_BLACK_AND_WHITE, BN_CLICKED, onBlackAndWhite)
		//COMMAND_HANDLER(IDC_SELWINCOLOR, BN_CLICKED, onEditBackground)
		//COMMAND_HANDLER(IDC_ERROR_COLOR, BN_CLICKED, onEditError)
		//COMMAND_HANDLER(IDC_ALTERNATE_COLOR, BN_CLICKED, onEditAlternate)
		COMMAND_ID_HANDLER(IDC_SELTEXT, onClickedText)
		
		COMMAND_HANDLER(IDC_TABCOLOR_LIST, LBN_SELCHANGE, onTabListChange)
		COMMAND_HANDLER(IDC_RESET_TAB_COLOR, BN_CLICKED, onClickedResetTabColor)
		COMMAND_HANDLER(IDC_SELECT_TAB_COLOR, BN_CLICKED, onClientSelectTabColor)
		COMMAND_HANDLER(IDC_THEME_COMBO2, CBN_SELCHANGE, onImport)
		COMMAND_ID_HANDLER(IDC_BAN_COLOR, onSelectColor)
		COMMAND_ID_HANDLER(IDC_DUPE_COLOR, onSelectColor)
		COMMAND_ID_HANDLER(IDC_DUPE_EX1, onSelectColor)
		COMMAND_ID_HANDLER(IDC_DUPE_EX2, onSelectColor)
		COMMAND_ID_HANDLER(IDC_DUPE_EX3, onSelectColor)
		
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCtlColor(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onExport(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditBackColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditForeColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditTextStyle(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onDefaultStyles(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onBlackAndWhite(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onClickedText(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		//LRESULT onEditBackground(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		//LRESULT onEditError(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		//LRESULT onEditAlternate(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onTabListChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedResetTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClientSelectTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSelectColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		void onResetColor(int i);
		
		void setForeColor(CEdit& cs, const COLORREF& cr)
		{
			HBRUSH hBr = CreateSolidBrush(cr);
			SetProp(cs.m_hWnd, _T("fillcolor"), hBr);
			cs.Invalidate();
			cs.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASENOW | RDW_UPDATENOW | RDW_FRAME);
		}
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel();
		bool m_maincolor_changed;
		tstring m_tempfile;
	private:
		void RefreshPreview();
		
		class TextStyleSettings: public CHARFORMAT2
		{
			public:
				TextStyleSettings() : m_pParent(nullptr) { }
				~TextStyleSettings() { }
				
				void Init(PropPageTextStyles *pParent,
				          LPCSTR sText, LPCSTR sPreviewText,
				          SettingsManager::IntSetting iBack, SettingsManager::IntSetting iFore,
				          SettingsManager::IntSetting iBold, SettingsManager::IntSetting iItalic);
				void LoadSettings();
				void SaveSettings();
				void EditBackColor();
				void EditForeColor();
				void EditTextStyle();
				
				string m_sText;
				string m_sPreviewText;
				
				PropPageTextStyles *m_pParent;
				SettingsManager::IntSetting m_iBackColor;
				SettingsManager::IntSetting m_iForeColor;
				SettingsManager::IntSetting m_iBold;
				SettingsManager::IntSetting m_iItalic;
		};
		
	protected:
		static Item items[];
		static TextItem texts[];
		enum TextStyles
		{
			TS_GENERAL, TS_MYNICK, TS_MYMSG, TS_PRIVATE, TS_SYSTEM, TS_SERVER, TS_TIMESTAMP, TS_URL, TS_FAVORITE, TS_FAV_ENEMY, TS_OP,
			TS_LAST
		};
		
		struct clrs
		{
			ResourceManager::Strings name;
			int setting;
			COLORREF value;
		};
		
		static clrs colours[];
		
		
		TextStyleSettings TextStyles[ TS_LAST ];
		CListBox m_lsbList;
		ChatCtrl m_Preview;
		LOGFONT m_Font;
		COLORREF m_BackColor;
		COLORREF m_ForeColor;
		COLORREF fg, bg;//, err, alt;
		
		CListBox ctrlTabList;
		
		CButton cmdResetTab;
		CButton cmdSetTabColor;
		CEdit ctrlTabExample;
		
		typedef boost::unordered_map<wstring, string> ColorThemeMap;
		typedef pair<wstring, string> ThemePair;
		CComboBox ctrlTheme;
		ColorThemeMap m_ThemeList;
		void GetThemeList();
};

#endif // _PROP_PAGE_TEXT_STYLES_H_
