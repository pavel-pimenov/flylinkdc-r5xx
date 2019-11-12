/*
 * FlylinkDC++ // Chat Settings Page
 */

#include "stdafx.h"
#include "Resource.h"
#include "MessagesChatPage.h"

PropPage::TextItem MessagesChatPage::g_texts_chat[] =
{
	{ IDC_PROTECT_PRIVATE, ResourceManager::SETTINGS_PROTECT_PRIVATE },
	{ IDC_SETTINGS_PASSWORD, ResourceManager::SETTINGS_PASSWORD },
	{ IDC_SETTINGS_PASSWORD_HINT, ResourceManager::SETTINGS_PASSWORD_HINT },
	{ IDC_SETTINGS_PASSWORD_OK_HINT, ResourceManager::SETTINGS_PASSWORD_OK_HINT },
	{ IDC_PROTECT_PRIVATE_RND, ResourceManager::SETTINGS_PROTECT_PRIVATE_RND },
	{ IDC_PROTECT_PRIVATE_SAY, ResourceManager::SETTINGS_PROTECT_PRIVATE_SAY },
	{ IDC_PM_PASSWORD_GENERATE, ResourceManager::WIZARD_NICK_RND },
	{ IDC_MISC_CHAT, ResourceManager::USER_CMD_CHAT },
	
	{ IDC_BUFFER_STR, ResourceManager::BUFFER_STR },
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item MessagesChatPage::g_items_chat[] =
{
	{ IDC_PROTECT_PRIVATE, SettingsManager::PROTECT_PRIVATE, PropPage::T_BOOL },
	{ IDC_PASSWORD, SettingsManager::PM_PASSWORD, PropPage::T_STR },
	{ IDC_PASSWORD_HINT, SettingsManager::PM_PASSWORD_HINT, PropPage::T_STR },
	{ IDC_PASSWORD_OK_HINT, SettingsManager::PM_PASSWORD_OK_HINT, PropPage::T_STR },
	{ IDC_PROTECT_PRIVATE_RND, SettingsManager::PROTECT_PRIVATE_RND, PropPage::T_BOOL },
	{ IDC_PROTECT_PRIVATE_SAY, SettingsManager::PROTECT_PRIVATE_SAY, PropPage::T_BOOL },
	{ IDC_BUFFERSIZE, SettingsManager::CHATBUFFERSIZE, PropPage::T_INT },
	
	{ 0, 0, PropPage::T_END }
};

MessagesChatPage::ListItem MessagesChatPage::g_listItems_chat[] =
{
#ifdef IRAINMAN_INCLUDE_SMILE
	{ SettingsManager::SHOW_EMOTIONS_BTN, ResourceManager::SHOW_EMOTIONS_BTN }, // [+] SSA
	{ SettingsManager::CHAT_ANIM_SMILES, ResourceManager::CHAT_ANIM_SMILES },
	{ SettingsManager::SMILE_SELECT_WND_ANIM_SMILES, ResourceManager::SMILE_SELECT_WND_ANIM_SMILES },
#endif
	{ SettingsManager::SHOW_SEND_MESSAGE_BUTTON, ResourceManager::SHOW_SEND_MESSAGE_BUTTON}, // [+] SSA
#ifdef IRAINMAN_USE_BB_CODES
	{ SettingsManager::FORMAT_BB_CODES, ResourceManager::FORMAT_BB_CODES },//[+]IRainman
	{ SettingsManager::FORMAT_BB_CODES_COLORS, ResourceManager::FORMAT_BB_CODES_COLORS },//[+]SSA
#endif
	{ SettingsManager::SHOW_BBCODE_PANEL, ResourceManager::SHOW_BBCODE_PANEL}, // [+] SSA
	{ SettingsManager::FORMAT_BOT_MESSAGE, ResourceManager::FORMAT_BOT_MESSAGE },// [+] IRainman
	{ SettingsManager::MULTILINE_CHAT_INPUT, ResourceManager::MULTILINE_CHAT_INPUT },
	{ SettingsManager::MULTILINE_CHAT_INPUT_BY_CTRL_ENTER, ResourceManager::MULTILINE_CHAT_INPUT_BY_CTRL_ENTER },//[+] SSA
	{ SettingsManager::SHOW_MULTI_CHAT_BTN, ResourceManager::SHOW_MULTI_CHAT_BTN }, // [+] IRainman
#ifdef SCALOLAZ_CHAT_REFFERING_TO_NICK
	{ SettingsManager::CHAT_REFFERING_TO_NICK, ResourceManager::CHAT_REFFERING_TO_NICK },   // [+] SCALOlaz
#endif
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT MessagesChatPage::onInitDialog_chat(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::read(*this, g_items_chat, g_listItems_chat, GetDlgItem(IDC_MESSAGES_CHAT_BOOLEANS));
	
	ctrlList_chat.Attach(GetDlgItem(IDC_MESSAGES_CHAT_BOOLEANS)); // [+] IRainman
	
	tooltip_messageschat.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	tooltip_messageschat.SetDelayTime(TTDT_AUTOPOP, 15000);
	ATLASSERT(tooltip_messageschat.IsWindow());
	ctrlSee.Attach(GetDlgItem(IDC_PROTECT_PRIVATE_SAY));
	tooltip_messageschat.AddTool(ctrlSee, ResourceManager::PROTECT_PRIVATE_SAY_TOOLTIP);
	ctrlProtect.Attach(GetDlgItem(IDC_PROTECT_PRIVATE));
	tooltip_messageschat.AddTool(ctrlProtect, ResourceManager::PROTECT_PRIVATE_TOOLTIP);
	ctrlRnd.Attach(GetDlgItem(IDC_PROTECT_PRIVATE_RND));
	tooltip_messageschat.AddTool(ctrlRnd, ResourceManager::PROTECT_PRIVATE_RND_TOOLTIP);
	tooltip_messageschat.SetMaxTipWidth(256);   //[+] SCALOlaz: activate tooltips
	if (!BOOLSETTING(POPUPS_DISABLED))
	{
		tooltip_messageschat.Activate(TRUE);
	}
	
	PropPage::translate((HWND)(*this), g_texts_chat);
	fixControls();
	// Do specialized reading here
	return TRUE;
}

void MessagesChatPage::write()
{
	PropPage::write(*this, g_items_chat, g_listItems_chat, GetDlgItem(IDC_MESSAGES_CHAT_BOOLEANS));
	// Do specialized writing here
}

LRESULT MessagesChatPage::onClickedUse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void MessagesChatPage::fixControls()
{
	BOOL use_resources = IsDlgButtonChecked(IDC_PROTECT_PRIVATE_RND) == BST_UNCHECKED;
	GetDlgItem(IDC_PASSWORD).EnableWindow(use_resources);
	GetDlgItem(IDC_PM_PASSWORD_GENERATE).EnableWindow(use_resources);
}

LRESULT MessagesChatPage::onRandomPassword(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SetDlgItemText(IDC_PASSWORD, Text::toT(Util::getRandomNick()).c_str());
	return 0;
}

LRESULT MessagesChatPage::onClickedHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	MessageBox(CTSTRING(PRIVATE_PASSWORD_HELP), CTSTRING(PRIVATE_PASSWORD_HELP_DESC), MB_OK | MB_ICONINFORMATION);
	return S_OK;
}