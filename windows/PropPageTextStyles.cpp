
#include "stdafx.h"

#include "Resource.h"
#include "../client/SimpleXML.h"

#include "WinUtil.h"
#include "OperaColorsPage.h"
#include "PropertiesDlg.h"

PropPage::TextItem PropPageTextStyles::texts[] =
{
	{ IDC_AVAILABLE_STYLES, ResourceManager::SETCZDC_STYLES },
	{ IDC_BACK_COLOR, ResourceManager::SETCZDC_BACK_COLOR },
	{ IDC_TEXT_COLOR, ResourceManager::SETCZDC_TEXT_COLOR },
	{ IDC_TEXT_STYLE, ResourceManager::SETCZDC_TEXT_STYLE },
	{ IDC_DEFAULT_STYLES, ResourceManager::SETCZDC_DEFAULT_STYLE },
	{ IDC_BLACK_AND_WHITE, ResourceManager::SETCZDC_BLACK_WHITE },
	{ IDC_BOLD_AUTHOR_MESS, ResourceManager::SETCZDC_BOLD },
	{ IDC_WINDOWS_STYLE_URL, ResourceManager::WINDOWS_STYLE_URL },
	{ IDC_CZDC_PREVIEW, ResourceManager::PREVIEW_MENU },
	{ IDC_SELTEXT, ResourceManager::SETTINGS_SELECT_TEXT_FACE },
	{ IDC_RESET_TAB_COLOR, ResourceManager::SETTINGS_RESET },
	{ IDC_SELECT_TAB_COLOR, ResourceManager::SETTINGS_SELECT_COLOR },
	{ IDC_STYLES, ResourceManager::SETTINGS_TEXT_STYLES },
	{ IDC_IMPORT, ResourceManager::SETTINGS_THEME_IMPORT },
	{ IDC_EXPORT, ResourceManager::SETTINGS_THEME_EXPORT },
	{ IDC_CHATCOLORS, ResourceManager::SETTINGS_COLORS },
	{ IDC_BAN_COLOR, ResourceManager::BAN_COLOR_DLG }, // !necros!
	{ IDC_DUPE_COLOR, ResourceManager::DUPE_COLOR_DLG }, // !necros!
	{ IDC_DUPE_EX1, ResourceManager::DUPE_EX1 }, // [+]NSL
	{ IDC_DUPE_EX2, ResourceManager::DUPE_EX2 }, // [+]NSL
	
	{ IDC_MODCOLORS, ResourceManager::MOD_COLOR_DLG }, // !SMT!-UI
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item PropPageTextStyles::items[] =
{
	{ IDC_BOLD_AUTHOR_MESS, SettingsManager::BOLD_AUTHOR_MESS, PropPage::T_BOOL },
	{ IDC_WINDOWS_STYLE_URL, SettingsManager::WINDOWS_STYLE_URL, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

PropPageTextStyles::clrs PropPageTextStyles::colours[] =
{
	{ResourceManager::SETTINGS_SELECT_WINDOW_COLOR, SettingsManager::BACKGROUND_COLOR, 0},
	//[-] PPA   {ResourceManager::SETTINGS_COLOR_ALTERNATE, SettingsManager::SEARCH_ALTERNATE_COLOUR, 0},
	{ResourceManager::SETCZDC_ERROR_COLOR,  SettingsManager::ERROR_COLOR, 0},
	{ResourceManager::TAB_SELECTED_COLOR,   SettingsManager::TAB_SELECTED_COLOR, 0},
	{ResourceManager::TAB_OFFLINE_COLOR,   SettingsManager::TAB_OFFLINE_COLOR, 0},
	{ResourceManager::TAB_ACTIVITY_COLOR,   SettingsManager::TAB_ACTIVITY_COLOR, 0},
	{ResourceManager::TAB_SELECTED_TEXT_COLOR,   SettingsManager::TAB_SELECTED_TEXT_COLOR, 0},
	{ResourceManager::TAB_OFFLINE_TEXT_COLOR,   SettingsManager::TAB_OFFLINE_TEXT_COLOR, 0},
	{ResourceManager::TAB_ACTIVITY_TEXT_COLOR,   SettingsManager::TAB_ACTIVITY_TEXT_COLOR, 0},
	{ResourceManager::PROGRESS_BACK,    SettingsManager::PROGRESS_BACK_COLOR, 0},
	{ResourceManager::PROGRESS_COMPRESS,    SettingsManager::PROGRESS_COMPRESS_COLOR, 0},
	{ResourceManager::PROGRESS_SEGMENT, SettingsManager::PROGRESS_SEGMENT_COLOR, 0},
	{ResourceManager::PROGRESS_DOWNLOADED,  SettingsManager::COLOR_DOWNLOADED, 0},
	{ResourceManager::PROGRESS_RUNNING, SettingsManager::COLOR_RUNNING, 0},
	{ResourceManager::PROGRESS_VERIFIED,    SettingsManager::COLOR_VERIFIED, 0},
	{ResourceManager::PROGRESS_AVOIDING,    SettingsManager::COLOR_AVOIDING, 0},
	
	{ResourceManager::SETTINGS_DUPE_COLOR,    SettingsManager::DUPE_COLOR, 0},  //[+] SCALOlaz
	{ResourceManager::DUPE_EX1,    SettingsManager::DUPE_EX1_COLOR, 0},         //[+]
	{ResourceManager::DUPE_EX2,    SettingsManager::DUPE_EX2_COLOR, 0},         //[+]
	{ResourceManager::BAN_COLOR_DLG,    SettingsManager::BAN_COLOR, 0},         //[+]
	
	{ResourceManager::HUB_IN_FAV_BK_COLOR,   SettingsManager::HUB_IN_FAV_BK_COLOR, 0},
	{ResourceManager::HUB_IN_FAV_CONNECT_BK_COLOR,   SettingsManager::HUB_IN_FAV_CONNECT_BK_COLOR, 0},
};
// [++]NSL
LRESULT PropPageTextStyles::onSelectColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int color;
	SettingsManager::IntSetting key;
	switch (wID)
	{
		case IDC_DUPE_COLOR:
			color = SETTING(DUPE_COLOR);
			key = SettingsManager::DUPE_COLOR;
			break;
		case IDC_DUPE_EX1:
			color = SETTING(DUPE_EX1_COLOR);
			key = SettingsManager::DUPE_EX1_COLOR;
			break;
		case IDC_DUPE_EX2:
			color = SETTING(DUPE_EX2_COLOR);
			key = SettingsManager::DUPE_EX2_COLOR;
			break;
		case IDC_BAN_COLOR:
			color = SETTING(BAN_COLOR);
			key = SettingsManager::BAN_COLOR;
			break;
		default:
			color = SETTING(ERROR_COLOR);
			key = SettingsManager::ERROR_COLOR;
	}
	CColorDialog dlg(color, CC_FULLOPEN);
	if (dlg.DoModal() == IDOK)
		SettingsManager::set(key, (int)dlg.GetColor());
	return 0;
}

LRESULT PropPageTextStyles::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	
	m_maincolor_changed = false;
	m_tempfile = Text::toT(Util::getThemesPath() + "temp.dctmp");
	
	m_lsbList.Attach(GetDlgItem(IDC_TEXT_STYLES));
	m_lsbList.ResetContent();
	m_Preview.Attach(GetDlgItem(IDC_PREVIEW));
	
	Fonts::decodeFont(Text::toT(SETTING(TEXT_FONT)), m_Font);
	m_BackColor = SETTING(BACKGROUND_COLOR);
	m_ForeColor = SETTING(TEXT_COLOR);
	
	fg = SETTING(TEXT_COLOR);
	bg = SETTING(BACKGROUND_COLOR);
	
	TextStyles[ TS_GENERAL ].Init(
	    this, settings, (char*)CSTRING(GENERAL_TEXT), (char*)CSTRING(GENERAL_TEXT),
	    SettingsManager::TEXT_GENERAL_BACK_COLOR, SettingsManager::TEXT_GENERAL_FORE_COLOR,
	    SettingsManager::TEXT_GENERAL_BOLD, SettingsManager::TEXT_GENERAL_ITALIC);
	    
	TextStyles[ TS_MYNICK ].Init(
	    this, settings, (char*)CSTRING(MY_NICK), (char*)CSTRING(MY_NICK),
	    SettingsManager::TEXT_MYNICK_BACK_COLOR, SettingsManager::TEXT_MYNICK_FORE_COLOR,
	    SettingsManager::TEXT_MYNICK_BOLD, SettingsManager::TEXT_MYNICK_ITALIC);
	    
	TextStyles[ TS_MYMSG ].Init(
	    this, settings, (char*)CSTRING(MY_MESSAGE), (char*)CSTRING(MY_MESSAGE),
	    SettingsManager::TEXT_MYOWN_BACK_COLOR, SettingsManager::TEXT_MYOWN_FORE_COLOR,
	    SettingsManager::TEXT_MYOWN_BOLD, SettingsManager::TEXT_MYOWN_ITALIC);
	    
	TextStyles[ TS_PRIVATE ].Init(
	    this, settings, (char*)CSTRING(PRIVATE_MESSAGE), (char*)CSTRING(PRIVATE_MESSAGE),
	    SettingsManager::TEXT_PRIVATE_BACK_COLOR, SettingsManager::TEXT_PRIVATE_FORE_COLOR,
	    SettingsManager::TEXT_PRIVATE_BOLD, SettingsManager::TEXT_PRIVATE_ITALIC);
	    
	TextStyles[ TS_SYSTEM ].Init(
	    this, settings, (char*)CSTRING(SYSTEM_MESSAGE), (char*)CSTRING(SYSTEM_MESSAGE),
	    SettingsManager::TEXT_SYSTEM_BACK_COLOR, SettingsManager::TEXT_SYSTEM_FORE_COLOR,
	    SettingsManager::TEXT_SYSTEM_BOLD, SettingsManager::TEXT_SYSTEM_ITALIC);
	    
	TextStyles[ TS_SERVER ].Init(
	    this, settings, (char*)CSTRING(SERVER_MESSAGE), (char*)CSTRING(SERVER_MESSAGE),
	    SettingsManager::TEXT_SERVER_BACK_COLOR, SettingsManager::TEXT_SERVER_FORE_COLOR,
	    SettingsManager::TEXT_SERVER_BOLD, SettingsManager::TEXT_SERVER_ITALIC);
	    
	TextStyles[ TS_TIMESTAMP ].Init(
	    this, settings, (char*)CSTRING(TIMESTAMP), (char*)CSTRING(TEXT_STYLE_TIME_SAMPLE),
	    SettingsManager::TEXT_TIMESTAMP_BACK_COLOR, SettingsManager::TEXT_TIMESTAMP_FORE_COLOR,
	    SettingsManager::TEXT_TIMESTAMP_BOLD, SettingsManager::TEXT_TIMESTAMP_ITALIC);
	    
	TextStyles[ TS_URL ].Init(
	    this, settings, (char*)CSTRING(TEXT_STYLE_URL), (char*)CSTRING(TEXT_STYLE_URL_SAMPLE),
	    SettingsManager::TEXT_URL_BACK_COLOR, SettingsManager::TEXT_URL_FORE_COLOR,
	    SettingsManager::TEXT_URL_BOLD, SettingsManager::TEXT_URL_ITALIC);
	    
	TextStyles[ TS_FAVORITE ].Init(
	    this, settings, (char*)CSTRING(FAV_USER), (char*)CSTRING(FAV_USER),
	    SettingsManager::TEXT_FAV_BACK_COLOR, SettingsManager::TEXT_FAV_FORE_COLOR,
	    SettingsManager::TEXT_FAV_BOLD, SettingsManager::TEXT_FAV_ITALIC);
	    
	TextStyles[ TS_OP ].Init(
	    this, settings, (char*)CSTRING(OPERATOR), (char*)CSTRING(OPERATOR),
	    SettingsManager::TEXT_OP_BACK_COLOR, SettingsManager::TEXT_OP_FORE_COLOR,
	    SettingsManager::TEXT_OP_BOLD, SettingsManager::TEXT_OP_ITALIC);
	    
	for (int i = 0; i < TS_LAST; i++)
	{
		TextStyles[ i ].LoadSettings();
		_tcscpy(TextStyles[i].szFaceName, m_Font.lfFaceName);
		TextStyles[ i ].bCharSet = m_Font.lfCharSet;
		TextStyles[ i ].yHeight = m_Font.lfHeight;
		m_lsbList.AddString(Text::toT(TextStyles[i].m_sText).c_str());
	}
	m_lsbList.SetCurSel(0);
	
	ctrlTabList.Attach(GetDlgItem(IDC_TABCOLOR_LIST));
	cmdResetTab.Attach(GetDlgItem(IDC_RESET_TAB_COLOR));
	cmdSetTabColor.Attach(GetDlgItem(IDC_SELECT_TAB_COLOR));
	ctrlTabExample.Attach(GetDlgItem(IDC_SAMPLE_TAB_COLOR));
	
	ctrlTabList.ResetContent();
	for (int i = 0; i < _countof(colours); i++)
	{
		ctrlTabList.AddString(Text::toT(ResourceManager::getString(colours[i].name)).c_str());
		onResetColor(i);
	}
	
	setForeColor(ctrlTabExample, GetSysColor(COLOR_3DFACE));
	
	ctrlTabList.SetCurSel(0);
	BOOL bTmp;
	onTabListChange(0, 0, 0, bTmp);
	
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO2));
	GetThemeList();
	ctrlTheme.Detach();
	
	RefreshPreview();
	return TRUE;
}

void PropPageTextStyles::write()
{
	PropPage::write((HWND)*this, items);
	
	tstring f = WinUtil::encodeFont(m_Font);
	settings->set(SettingsManager::TEXT_FONT, Text::fromT(f));
	
	m_BackColor = TextStyles[ TS_GENERAL ].crBackColor;
	m_ForeColor = TextStyles[ TS_GENERAL ].crTextColor;
	
	settings->set(SettingsManager::TEXT_COLOR, (int)fg);
	settings->set(SettingsManager::BACKGROUND_COLOR, (int)bg);
	
	for (int i = 1; i < _countof(colours); i++)
	{
		settings->set((SettingsManager::IntSetting)colours[i].setting, (int)colours[i].value);
	}
	
	for (int i = 0; i < TS_LAST; i++)
	{
		TextStyles[ i ].SaveSettings();
	}
	if (m_maincolor_changed)
		SettingsManager::getInstance()->save();
	File::deleteFile(m_tempfile);
	
	Colors::init();
}

void PropPageTextStyles::cancel()
{
	if (m_maincolor_changed)
	{
		SettingsManager::importDctheme(m_tempfile);
		SendMessage(WM_DESTROY, 0, 0);
		PropertiesDlg::needUpdate = true;
		SendMessage(WM_INITDIALOG, 0, 0);
		m_maincolor_changed = false;
	}
	write();
}

LRESULT PropPageTextStyles::onEditBackColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int iNdx = m_lsbList.GetCurSel();
	TextStyles[ iNdx ].EditBackColor();
	RefreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onEditForeColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int iNdx = m_lsbList.GetCurSel();
	TextStyles[ iNdx ].EditForeColor();
	RefreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onEditTextStyle(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int iNdx = m_lsbList.GetCurSel();
	TextStyles[ iNdx ].EditTextStyle();
	
	_tcscpy(m_Font.lfFaceName, TextStyles[ iNdx ].szFaceName);
	m_Font.lfCharSet = TextStyles[ iNdx ].bCharSet;
	m_Font.lfHeight = TextStyles[ iNdx ].yHeight;
	
	if (iNdx == TS_GENERAL)
	{
		if ((TextStyles[ iNdx ].dwEffects & CFE_ITALIC) == CFE_ITALIC)
			m_Font.lfItalic = TRUE;
		if ((TextStyles[ iNdx ].dwEffects & CFE_BOLD) == CFE_BOLD)
			m_Font.lfWeight = FW_BOLD;
	}
	
	for (int i = 0; i < TS_LAST; i++)
	{
		_tcscpy(TextStyles[ iNdx ].szFaceName, m_Font.lfFaceName);
		TextStyles[ i ].bCharSet = m_Font.lfCharSet;
		TextStyles[ i ].yHeight = m_Font.lfHeight;
		m_Preview.AppendText(ClientManager::getFlylinkDCIdentity(), false, true, _T("12:34 "), Text::toT(TextStyles[i].m_sPreviewText).c_str(), TextStyles[i]);
	}
	
	RefreshPreview();
	return TRUE;
}

void PropPageTextStyles::RefreshPreview()
{
	m_Preview.SetBackgroundColor(bg);
	
	CHARFORMAT2 old = Colors::g_TextStyleMyNick;
	CHARFORMAT2 old2 = Colors::g_TextStyleTimestamp;
	m_Preview.SetTextStyleMyNick(TextStyles[ TS_MYNICK ]);
	Colors::g_TextStyleTimestamp = TextStyles[ TS_TIMESTAMP ];
	m_Preview.SetWindowText(_T(""));
	
	//[-] PVS-Studio V808 string sText;
	for (int i = 0; i < TS_LAST; i++)
	{
		m_Preview.AppendText(ClientManager::getFlylinkDCIdentity(), false, true, _T("12:34 "), Text::toT(TextStyles[i].m_sPreviewText).c_str(), TextStyles[i]
#ifdef IRAINMAN_INCLUDE_SMILE
		                     , false
#endif
		                    );
	}
	m_Preview.InvalidateRect(NULL);
	m_Preview.SetTextStyleMyNick(old);
	Colors::g_TextStyleTimestamp = old2;
}

LRESULT PropPageTextStyles::onDefaultStyles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bg = RGB(255, 255, 255);
	fg = RGB(67, 98, 154);
	::GetObject((HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(m_Font), &m_Font);
	TextStyles[ TS_GENERAL ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_GENERAL ].crTextColor = RGB(67, 98, 154);
	TextStyles[ TS_GENERAL ].dwEffects = 0;
	
	TextStyles[ TS_MYNICK ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_MYNICK ].crTextColor = RGB(67, 98, 154);
	TextStyles[ TS_MYNICK ].dwEffects = CFE_BOLD;
	
	TextStyles[ TS_MYMSG ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_MYMSG ].crTextColor = RGB(67, 98, 154);
	TextStyles[ TS_MYMSG ].dwEffects = 0;
	
	TextStyles[ TS_PRIVATE ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_PRIVATE ].crTextColor = RGB(67, 98, 154);
	TextStyles[ TS_PRIVATE ].dwEffects = CFE_ITALIC;
	
	TextStyles[ TS_SYSTEM ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_SYSTEM ].crTextColor = RGB(0, 128, 64);
	TextStyles[ TS_SYSTEM ].dwEffects = CFE_BOLD;
	
	TextStyles[ TS_SERVER ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_SERVER ].crTextColor = RGB(0, 128, 64);
	TextStyles[ TS_SERVER ].dwEffects = CFE_BOLD;
	
	TextStyles[ TS_TIMESTAMP ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_TIMESTAMP ].crTextColor = RGB(67, 98, 154);
	TextStyles[ TS_TIMESTAMP ].dwEffects = 0;
	
	TextStyles[ TS_URL ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_URL ].crTextColor = RGB(0, 0, 255);
	TextStyles[ TS_URL ].dwEffects = 0;
	
	TextStyles[ TS_FAVORITE ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_FAVORITE ].crTextColor = RGB(67, 98, 154);
	TextStyles[ TS_FAVORITE ].dwEffects = CFE_BOLD;
	
	TextStyles[ TS_OP ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_OP ].crTextColor = RGB(0, 128, 64);
	TextStyles[ TS_OP ].dwEffects = CFE_BOLD;
	
	RefreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onBlackAndWhite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bg = RGB(255, 255, 255);
	fg = RGB(0, 0, 0);
	TextStyles[ TS_GENERAL ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_GENERAL ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_GENERAL ].dwEffects = 0;
	
	TextStyles[ TS_MYNICK ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_MYNICK ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_MYNICK ].dwEffects = 0;
	
	TextStyles[ TS_MYMSG ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_MYMSG ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_MYMSG ].dwEffects = 0;
	
	TextStyles[ TS_PRIVATE ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_PRIVATE ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_PRIVATE ].dwEffects = 0;
	
	TextStyles[ TS_SYSTEM ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_SYSTEM ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_SYSTEM ].dwEffects = 0;
	
	TextStyles[ TS_SERVER ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_SERVER ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_SERVER ].dwEffects = 0;
	
	TextStyles[ TS_TIMESTAMP ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_TIMESTAMP ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_TIMESTAMP ].dwEffects = 0;
	
	TextStyles[ TS_URL ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_URL ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_URL ].dwEffects = 0;
	
	TextStyles[ TS_FAVORITE ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_FAVORITE ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_FAVORITE ].dwEffects = 0;
	
	TextStyles[ TS_OP ].crBackColor = RGB(255, 255, 255);
	TextStyles[ TS_OP ].crTextColor = RGB(37, 60, 121);
	TextStyles[ TS_OP ].dwEffects = 0;
	
	RefreshPreview();
	return TRUE;
}

void PropPageTextStyles::TextStyleSettings::Init(
    PropPageTextStyles *pParent, SettingsManager *pSM,
    LPCSTR sText, LPCSTR sPreviewText,
    SettingsManager::IntSetting iBack, SettingsManager::IntSetting iFore,
    SettingsManager::IntSetting iBold, SettingsManager::IntSetting iItalic)
{

	cbSize = sizeof(CHARFORMAT2);
	dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_BACKCOLOR;
	dwReserved = 0;
	
	m_pParent = pParent;
	settings = pSM;
	m_sText = sText;
	m_sPreviewText = sPreviewText;
	m_iBackColor = iBack;
	m_iForeColor = iFore;
	m_iBold = iBold;
	m_iItalic = iItalic;
}

void PropPageTextStyles::TextStyleSettings::LoadSettings()
{
	dwEffects = 0;
	crBackColor = settings->get(m_iBackColor);
	crTextColor = settings->get(m_iForeColor);
	if (settings->get(m_iBold)) dwEffects |= CFE_BOLD;
	if (settings->get(m_iItalic)) dwEffects |= CFE_ITALIC;
}

void PropPageTextStyles::TextStyleSettings::SaveSettings()
{
	settings->set(m_iBackColor, (int) crBackColor);
	settings->set(m_iForeColor, (int) crTextColor);
	BOOL boBold = ((dwEffects & CFE_BOLD) == CFE_BOLD);
	settings->set(m_iBold, (int) boBold);
	BOOL boItalic = ((dwEffects & CFE_ITALIC) == CFE_ITALIC);
	settings->set(m_iItalic, (int) boItalic);
}

void PropPageTextStyles::TextStyleSettings::EditBackColor()
{
	CColorDialog d(crBackColor, 0, *m_pParent);
	if (d.DoModal() == IDOK)
	{
		crBackColor = d.GetColor();
	}
}

void PropPageTextStyles::TextStyleSettings::EditForeColor()
{
	CColorDialog d(crTextColor, 0, *m_pParent);
	if (d.DoModal() == IDOK)
	{
		crTextColor = d.GetColor();
	}
}

void PropPageTextStyles::TextStyleSettings::EditTextStyle()
{
	LOGFONT font = {0};
	Fonts::decodeFont(Text::toT(SETTING(TEXT_FONT)), font);
	
	_tcscpy(font.lfFaceName, szFaceName);
	font.lfCharSet = bCharSet;
	font.lfHeight = yHeight;
	
	if (dwEffects & CFE_BOLD)
		font.lfWeight = FW_BOLD;
	else
		font.lfWeight = FW_REGULAR;
		
	if (dwEffects & CFE_ITALIC)
		font.lfItalic = TRUE;
	else
		font.lfItalic = FALSE;
		
	CFontDialog d(&font, CF_SCREENFONTS | CF_FORCEFONTEXIST, NULL, *m_pParent);   // !SMT!-F
	d.m_cf.rgbColors = crTextColor;
	if (d.DoModal() == IDOK)
	{
		_tcscpy(szFaceName, font.lfFaceName);
		bCharSet = font.lfCharSet;
		yHeight = font.lfHeight;
		
		crTextColor = d.m_cf.rgbColors;
		if (font.lfWeight == FW_BOLD)
			dwEffects |= CFE_BOLD;
		else
			dwEffects &= ~CFE_BOLD;
			
		if (font.lfItalic)
			dwEffects |= CFE_ITALIC;
		else
			dwEffects &= ~CFE_ITALIC;
	}
}

LRESULT PropPageTextStyles::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_lsbList.Detach();
	m_Preview.Detach();
	ctrlTabList.Detach();
	cmdResetTab.Detach();
	cmdSetTabColor.Detach();
	ctrlTabExample.Detach();
	
	return 1;
}

LRESULT PropPageTextStyles::onImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO2)); // [~] SCALOlaz: https://code.google.com/p/flylinkdc/issues/detail?id=455
	const tstring file = Text::toT(WinUtil::getDataFromMap(ctrlTheme.GetCurSel(), m_ThemeList));
	ctrlTheme.Detach();
	if (!m_maincolor_changed)
	{
		SettingsManager::exportDctheme(m_tempfile);
	}
	SettingsManager::importDctheme(file);
	SendMessage(WM_DESTROY, 0, 0);
	//  SettingsManager::getInstance()->save();
	PropertiesDlg::needUpdate = true;
	SendMessage(WM_INITDIALOG, 0, 0);
	m_maincolor_changed = true;
	//  RefreshPreview();
	return 0;
}

static const TCHAR types[] = _T("DC++ Theme Files\0*.dctheme;\0DC++ Settings Files\0*.xml;\0All Files\0*.*\0\0");// TODO translate
static const TCHAR defExt[] = _T(".dctheme");

LRESULT PropPageTextStyles::onExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
//  if (WinUtil::browseFile(file, m_hWnd, true, x, CTSTRING(DC_THEMES_DIALOG_FILE_TYPES_STRING), defExt) == IDOK)
	if (WinUtil::browseFile(file, m_hWnd, true, Text::toT(Util::getThemesPath()), types, defExt) == IDOK)// [!]IRainman  убрать
	{
		SettingsManager::exportDctheme(file); // [!] IRainman fix.
	}
	return 0;
}

void PropPageTextStyles::onResetColor(int i)
{
	colours[i].value = SettingsManager::get((SettingsManager::IntSetting)colours[i].setting, true);
	setForeColor(ctrlTabExample, colours[i].value);
}

LRESULT PropPageTextStyles::onTabListChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	COLORREF color = colours[ctrlTabList.GetCurSel()].value;
	setForeColor(ctrlTabExample, color);
	RefreshPreview();
	return S_OK;
}

LRESULT PropPageTextStyles::onClickedResetTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	onResetColor(ctrlTabList.GetCurSel());
	return S_OK;
}

LRESULT PropPageTextStyles::onClientSelectTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CColorDialog d(colours[ctrlTabList.GetCurSel()].value, CC_FULLOPEN, *this); //[~] SCALOlaz 0 before CC_FULLOPEN
	if (d.DoModal() == IDOK)
	{
		colours[ctrlTabList.GetCurSel()].value = d.GetColor();
		switch (ctrlTabList.GetCurSel())
		{
			case 0:
				bg = d.GetColor();
				break;
		}
		setForeColor(ctrlTabExample, d.GetColor());
		RefreshPreview();
	}
	return S_OK;
}

LRESULT PropPageTextStyles::onClickedText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LOGFONT tmp = m_Font;
	CFontDialog d(&tmp, CF_EFFECTS | CF_SCREENFONTS | CF_FORCEFONTEXIST, NULL, *this); // !SMT!-F
	d.m_cf.rgbColors = fg;
	if (d.DoModal() == IDOK)
	{
		m_Font = tmp;
		fg = d.GetColor();
	}
	RefreshPreview();
	return TRUE;
}

LRESULT PropPageTextStyles::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	HWND hWnd = (HWND)lParam;
	if (hWnd == ctrlTabExample.m_hWnd)
	{
		::SetBkMode((HDC)wParam, TRANSPARENT);
		HANDLE h = GetProp(hWnd, _T("fillcolor"));
		if (h != NULL)
		{
			return (LRESULT)h;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void PropPageTextStyles::GetThemeList()
{
	if (m_ThemeList.empty())
	{
		const string l_ext = ".dctheme";
		const string fileFindPath = Util::getThemesPath() + "*" + l_ext;
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			string name = i->getFileName();
			if (name.empty() || i->isDirectory() || i->getSize() == 0)
			{
				continue;
			}
			const wstring wName = Text::toT(name.substr(0, name.length() - l_ext.length()));
			m_ThemeList.insert(ThemePair(wName, Util::getThemesPath() + name));
		}
		
		int l_i = 0;
		for (auto i = m_ThemeList.cbegin(); i != m_ThemeList.cend(); ++i)
		{
			ctrlTheme.InsertString(l_i++, i->first.c_str());
		}
		
		if (l_i == 0)
		{
			ctrlTheme.InsertString(0, CTSTRING(THEME_DEFAULT_NAME));    //Only Default Theme
			ctrlTheme.EnableWindow(FALSE);
		}
		ctrlTheme.SetCurSel(0);
	}
}