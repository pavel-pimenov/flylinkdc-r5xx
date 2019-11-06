#include "stdafx.h"
#include "Resource.h"
#include "RangesPage.h"

#include "../client/SettingsManager.h"
#include "../client/IpGuard.h"
#include "../client/PGLoader.h"
#include "../client/File.h"

#define BUFLEN 256

PropPage::TextItem RangesPage::texts[] =
{
	{ IDC_DEFAULT_POLICY_STR, ResourceManager::IPGUARD_DEFAULT_POLICY },
	{ IDC_DEFAULT_POLICY_EXCEPT_STR, ResourceManager::EXCEPT_SPECIFIED },
	{ IDC_ENABLE_IPGUARD, ResourceManager::IPGUARD_ENABLE },
	{ IDC_ENABLE_P2P_GUARD, ResourceManager::P2P_GUARD_ENABLE },
	{ IDC_ENABLE_P2P_GUARD_STR, ResourceManager::P2P_GUARD_ENABLE_STR },
	{ IDC_INTRO_IPGUARD, ResourceManager::IPGUARD_INTRO },
	{ IDC_FLYLINK_TRUST_IP_BOX, ResourceManager::SETTINGS_IPBLOCK },
	{ IDC_FLYLINK_TRUST_IP_URL_STR, ResourceManager::SETTINGS_IPBLOCK_DOWNLOAD_URL_STR },
	{ IDC_FLYLINK_MANUAL_P2P_GUARD_IP_LIST_REMOVE_BUTTON, ResourceManager::REMOVE2 },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item RangesPage::items[] =
{
	{ IDC_ENABLE_IPTRUST, SettingsManager::ENABLE_IPTRUST, PropPage::T_BOOL },
	{ IDC_ENABLE_IPGUARD, SettingsManager::ENABLE_IPGUARD, PropPage::T_BOOL },
	{ IDC_ENABLE_P2P_GUARD, SettingsManager::ENABLE_P2P_GUARD, PropPage::T_BOOL },
	{ IDC_FLYLINK_TRUST_IP_URL, SettingsManager::URL_IPTRUST, PropPage::T_STR }, //[+]PPA
	{ 0, 0, PropPage::T_END }
};

LRESULT RangesPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items);
	m_isEnabledIPGuard = BOOLSETTING(ENABLE_IPGUARD);
	
	ctrlPolicy.Attach(GetDlgItem(IDC_DEFAULT_POLICY));
	ctrlPolicy.AddString(CTSTRING(DENY_ALL));
	ctrlPolicy.AddString(CTSTRING(ALLOW_ALL));
	ctrlPolicy.SetCurSel(BOOLSETTING(IP_GUARD_IS_DENY_ALL) ? 0 : 1);
	
	// Do specialized reading here
	try
	{
		m_IPGuardPATH = IpGuard::getIPGuardFileName();
		m_IPGuard = File(m_IPGuardPATH, File::READ, File::OPEN).read();
		SetDlgItemText(IDC_FLYLINK_GUARD_IP, Text::toT(m_IPGuard).c_str());
		// SetDlgItemText(IDC_FLYLINK_PATH, Text::toT(m_IPGrantPATH).c_str());
	}
	catch (const FileException&)
	{
		SetDlgItemText(IDC_FLYLINK_GUARD_IP, _T(""));
		// SetDlgItemText(IDC_FLYLINK_PATH, CTSTRING(ERR_IPFILTER));
	}
	fixControls();
	try
	{
		m_IPFilterPATH = PGLoader::getConfigFileName();
		m_IPFilter = File(m_IPFilterPATH, File::READ, File::OPEN).read();
		SetDlgItemText(IDC_FLYLINK_TRUST_IP, Text::toT(m_IPFilter).c_str());
		SetDlgItemText(IDC_FLYLINK_PATH, Text::toT(m_IPFilterPATH).c_str());
		m_list_box.Attach(GetDlgItem(IDC_FLYLINK_MANUAL_P2P_GUARD_IP_LIST_BOX));
		loadManualP2PGuard();
	}
	
	catch (const FileException&)
	{
		SetDlgItemText(IDC_FLYLINK_PATH, CTSTRING(ERR_IPFILTER));
	}
	
	// Do specialized reading here
	return TRUE;
}

void  RangesPage::loadManualP2PGuard()
{
	m_ManualP2PGuard = CFlylinkDBManager::getInstance()->load_manual_p2p_guard();
	StringTokenizer<string> l_lines(m_ManualP2PGuard, "\r\n");
	for (auto i = 0; i < l_lines.getTokens().size(); ++i)
	{
		m_list_box.AddString(Text::toT(l_lines.getTokens()[i]).c_str());
	}
	
}

LRESULT RangesPage::onRemoveP2PManual(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int n = m_list_box.GetSelCount();
	if (n)
	{
		std::vector<int> aiSel;
		aiSel.resize(n);
		m_list_box.GetSelItems(n, aiSel.data());
		for (int i = 0; i < n; ++i)
		{
			int nLen = m_list_box.GetTextLen(aiSel[i]);
			if (nLen > 0)
			{
				tstring l_str;
				l_str.resize(nLen + 2);
				m_list_box.GetText(aiSel[i], &l_str[0]);
				CFlylinkDBManager::getInstance()->remove_manual_p2p_guard(Text::fromT(l_str));
			}
		}
	}
	m_list_box.ResetContent();
	loadManualP2PGuard();
	return 0;
}

LRESULT RangesPage::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_CHANGE), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_REMOVE), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

LRESULT RangesPage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

void RangesPage::write()
{
	PropPage::write(*this, items);
	g_settings->set(SettingsManager::IP_GUARD_IS_DENY_ALL, !ctrlPolicy.GetCurSel());
	
	if (BOOLSETTING(ENABLE_IPGUARD))
	{
		tstring l_Guardbuf;
		GET_TEXT(IDC_FLYLINK_GUARD_IP, l_Guardbuf);
		const string l_newG = Text::fromT(l_Guardbuf);
		if (l_newG != m_IPGuard  || m_isEnabledIPGuard == false) // Изменился текст или включили галку - прогрузимся?
		{
			try
			{
				{
					File fout(m_IPGuardPATH, File::WRITE, File::CREATE | File::TRUNCATE);
					fout.write(l_newG);
				}
				IpGuard::load();
			}
			catch (const FileException&)
			{
				return;
			}
		}
	}
	else
	{
		IpGuard::clear();
	}
	
	tstring l_Trustbuf;
	GET_TEXT(IDC_FLYLINK_TRUST_IP, l_Trustbuf);
	const string l_newT = Text::fromT(l_Trustbuf);
	if (l_newT != m_IPFilter)
	{
		try
		{
			File fout(m_IPFilterPATH, File::WRITE, File::CREATE | File::TRUNCATE);
			fout.write(l_newT);
			fout.close();
#ifdef FLYLINKDC_USE_IPFILTER
			PGLoader::load(l_newT);
#endif
		}
		catch (const FileException&)
		{
			return;
		}
	}
}

void RangesPage::fixControls()
{
	const bool state = (IsDlgButtonChecked(IDC_ENABLE_IPGUARD) != 0);
	::EnableWindow(GetDlgItem(IDC_INTRO_IPGUARD), state);
	::EnableWindow(GetDlgItem(IDC_DEFAULT_POLICY_STR), state);
	::EnableWindow(GetDlgItem(IDC_DEFAULT_POLICY), state);
	::EnableWindow(GetDlgItem(IDC_DEFAULT_POLICY_EXCEPT_STR), state);
	::EnableWindow(GetDlgItem(IDC_FLYLINK_GUARD_IP), state);
	const bool state_ip_trust = (IsDlgButtonChecked(IDC_ENABLE_IPTRUST) != 0);
	::EnableWindow(GetDlgItem(IDC_FLYLINK_TRUST_IP), state_ip_trust);
	::EnableWindow(GetDlgItem(IDC_FLYLINK_TRUST_IP_URL), state_ip_trust);
}

