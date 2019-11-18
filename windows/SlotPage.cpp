
#include "stdafx.h"

#include "SlotPage.h"
#include "../client/IPGrant.h"
#include "LineDlg.h"

#include "../client/ShareManager.h"

PropPage::TextItem SlotPage::texts[] =
{
	{ IDC_SLOT_CONTROL_GROUP, ResourceManager::SLOT_CONTROL_GROUP },
	{ IDC_MINISLOT_CONTROL_GROUP, ResourceManager::MINISLOT_CONTROL_GROUP },
	{ IDC_SETTINGS_SHARE_SIZE, ResourceManager::SETTINGS_SHARE_SIZE },
	{ IDC_SETTINGS_UPLOADS_MIN_SPEED, ResourceManager::SETTINGS_UPLOADS_MIN_SPEED },
	{ IDC_SETTINGS_KBPS, ResourceManager::KBPS },
	{ IDC_SETTINGS_KB, ResourceManager::KB },
	{ IDC_SETTINGS_UPLOADS_SLOTS, ResourceManager::SETTINGS_UPLOADS_SLOTS },
	{ IDC_CZDC_SMALL_SLOTS, ResourceManager::SETCZDC_SMALL_UP_SLOTS },
	{ IDC_CZDC_SMALL_SIZE, ResourceManager::SETCZDC_SMALL_FILES },
	{ IDC_CZDC_NOTE_SMALL, ResourceManager::SETCZDC_NOTE_SMALL_UP },
	{ IDC_STATICb, ResourceManager::EXTRA_HUB_SLOTS },
	{ IDC_SLOT_DL, ResourceManager::EXTRASLOT_TO_DL },
	{ IDC_SETTINGS_AUTO_SLOTS, ResourceManager::SETTINGS_AUTO_SLOTS },
	{ IDC_SETTINGS_PARTIAL_SLOTS, ResourceManager::SETCZDC_PARTIAL_SLOTS },
#ifdef SSA_IPGRANT_FEATURE
	{ IDC_GRANT_IP_GROUP, ResourceManager::GRANT_IP_GROUP },
	{ IDC_EXTRA_SLOT_BY_IP, ResourceManager::EXTRA_SLOT_BY_IP},
	{ IDC_GRANTIP_INI_STAIC, ResourceManager::GRANTIP_INI_STAIC},
#endif
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item SlotPage::items[] =
{
	{ IDC_SLOTS, SettingsManager::SLOTS, PropPage::T_INT },
	{ IDC_MIN_UPLOAD_SPEED, SettingsManager::MIN_UPLOAD_SPEED, PropPage::T_INT },
	{ IDC_EXTRA_SLOTS, SettingsManager::EXTRA_SLOTS, PropPage::T_INT },
	{ IDC_SMALL_FILE_SIZE, SettingsManager::SET_MINISLOT_SIZE, PropPage::T_INT },
	{ IDC_EXTRA_SLOTS2, SettingsManager::HUB_SLOTS, PropPage::T_INT },
	{ IDC_SLOT_DL, SettingsManager::EXTRASLOT_TO_DL, PropPage::T_BOOL },
	{ IDC_AUTO_SLOTS, SettingsManager::AUTO_SLOTS, PropPage::T_INT  },
	{ IDC_PARTIAL_SLOTS, SettingsManager::EXTRA_PARTIAL_SLOTS, PropPage::T_INT  },
#ifdef SSA_IPGRANT_FEATURE
	{ IDC_EXTRA_SLOT_BY_IP, SettingsManager::EXTRA_SLOT_BY_IP, PropPage::T_BOOL },
#endif
	{ 0, 0, PropPage::T_END }
};

LRESULT SlotPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	
	PropPage::read(*this, items);
	
	CUpDownCtrl updown;
	updown.Attach(GetDlgItem(IDC_SLOTSPIN));
	updown.SetRange(1, 500);
	updown.Detach();
	updown.Attach(GetDlgItem(IDC_MIN_UPLOAD_SPIN));
	updown.SetRange32(0, UD_MAXVAL);
	updown.Detach();
	updown.Attach(GetDlgItem(IDC_EXTRA_SLOTS_SPIN));
	updown.SetRange(3, 100);
	updown.Detach();
	updown.Attach(GetDlgItem(IDC_SMALL_FILE_SIZE_SPIN));
	updown.SetRange32(16, 32768);
	updown.Detach();
	updown.Attach(GetDlgItem(IDC_EXTRASPIN));
	updown.SetRange(0, 10);
	updown.Detach();
	updown.Attach(GetDlgItem(IDC_AUTO_SLOTS_SPIN));
	updown.SetRange(0, 100);
	updown.Detach();
	updown.Attach(GetDlgItem(IDC_PARTIAL_SLOTS_SPIN));
	updown.SetRange(0, 10);
	updown.Detach();
	
#ifdef SSA_IPGRANT_FEATURE
	try
	{
		m_isEnabledIPGrant = BOOLSETTING(EXTRA_SLOT_BY_IP);
		m_IPGrantPATH = IpGrant::getConfigFileName();
		m_IPGrant = File(m_IPGrantPATH, File::READ, File::OPEN).read();
		SetDlgItemText(IDC_GRANTIP_INI, Text::toT(m_IPGrant).c_str());
		// SetDlgItemText(IDC_FLYLINK_PATH, Text::toT(m_IPGrantPATH).c_str());
	}
	catch (const FileException&)
	{
		SetDlgItemText(IDC_GRANTIP_INI, _T(""));
		// SetDlgItemText(IDC_FLYLINK_PATH, CTSTRING(ERR_IPFILTER));
	}
	
	fixControls();
#else
	::EnableWindow(GetDlgItem(IDC_EXTRA_SLOT_BY_IP), FALSE);
	GetDlgItem(IDC_EXTRA_SLOT_BY_IP).ShowWindow(FALSE);
	
	::EnableWindow(GetDlgItem(IDC_GRANT_IP_GROUP), FALSE);
	GetDlgItem(IDC_GRANT_IP_GROUP).ShowWindow(FALSE);
	
	::EnableWindow(GetDlgItem(IDC_GRANTIP_INI_STAIC), FALSE);
	GetDlgItem(IDC_GRANTIP_INI_STAIC).ShowWindow(FALSE);
	
	::EnableWindow(GetDlgItem(IDC_GRANTIP_INI), FALSE);
	GetDlgItem(IDC_GRANTIP_INI).ShowWindow(FALSE);
#endif // SSA_IPGRANT_FEATURE
	
	return TRUE;
}

void SlotPage::write()
{
	PropPage::write(*this, items);
#ifdef SSA_IPGRANT_FEATURE
	tstring l_buf;
	GET_TEXT(IDC_GRANTIP_INI, l_buf);
	const string l_new = Text::fromT(l_buf);
	if (BOOLSETTING(EXTRA_SLOT_BY_IP))
	{
		if (l_new != m_IPGrant || m_isEnabledIPGrant == false) // Изменился текст или включили галку - прогрузимся?
		{
			try
			{
				{
					File fout(m_IPGrantPATH, File::WRITE, File::CREATE | File::TRUNCATE);
					fout.write(l_new);
				}
				IpGrant::load();
			}
			catch (const FileException&)
			{
				return;
			}
		}
	}
	else
	{
		IpGrant::clear();
	}
#endif // SSA_IPGRANT_FEATURE
}
#ifdef SSA_IPGRANT_FEATURE
void SlotPage::fixControls()
{
	bool state = (IsDlgButtonChecked(IDC_EXTRA_SLOT_BY_IP) != 0);
	::EnableWindow(GetDlgItem(IDC_GRANTIP_INI_STAIC), state);
	::EnableWindow(GetDlgItem(IDC_GRANTIP_INI), state);
}
#endif