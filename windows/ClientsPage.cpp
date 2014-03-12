
#include "stdafx.h"

#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER

#include "ClientsPage.h"

#include "../client/SettingsManager.h"
#include "Resource.h"
#include "DetectionEntryDlg.h"

PropPage::TextItem ClientsPage::texts[] =
{
	{ IDC_MOVE_CLIENT_UP, ResourceManager::MOVE_UP },
	{ IDC_MOVE_CLIENT_DOWN, ResourceManager::MOVE_DOWN },
	{ IDC_ADD_CLIENT, ResourceManager::ADD },
	{ IDC_CHANGE_CLIENT, ResourceManager::SETTINGS_CHANGE },
	{ IDC_REMOVE_CLIENT, ResourceManager::REMOVE },
	{ IDC_RELOAD_CLIENTS, ResourceManager::RELOAD },
	{ IDC_UPDATE, ResourceManager::UPDATE_CHECK },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item ClientsPage::items[] =
{
	{ IDC_UPDATE_URL, SettingsManager::PROFILES_URL, PropPage::T_STR }, // TODO - похерить
	{ 0, 0, PropPage::T_END }
};

LRESULT ClientsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	
	CRect rc;
	
	ctrlProfiles.Attach(GetDlgItem(IDC_CLIENT_ITEMS));
	ctrlProfiles.GetClientRect(rc);
	
	ctrlProfiles.InsertColumn(0, CTSTRING(SETTINGS_NAME),   LVCFMT_LEFT, rc.Width() / 3, 0);
	ctrlProfiles.InsertColumn(1, CTSTRING(COMMENT),         LVCFMT_LEFT, rc.Width() / 2 - 18, 1);
	ctrlProfiles.InsertColumn(2, _T("Raw"),                 LVCFMT_LEFT, rc.Width() / 6, 0);
	
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlProfiles);
	SET_LIST_COLOR_IN_SETTING(ctrlProfiles);
	
	// Do specialized reading here
	const DetectionManager::DetectionItems& lst = DetectionManager::getInstance()->getProfiles();
	auto cnt = ctrlProfiles.GetItemCount();
	for (auto i = lst.cbegin(); i != lst.cend(); ++i)
	{
		const DetectionEntry& de = *i;
		addEntry(de, cnt++);
	}
	
	SetDlgItemText(IDC_PROFILE_COUNT, (TSTRING(PROFILE_COUNT) + _T(": ") + Util::toStringW(ctrlProfiles.GetItemCount())).c_str());
	SetDlgItemText(IDC_PROFILE_VERSION, (TSTRING(PROFILE_VERSION) + _T(": ") + Text::toT(DetectionManager::getInstance()->getProfileVersion())).c_str());
	SetDlgItemText(IDC_PROFILE_MESSAGE, (TSTRING(PROFILE_MESSAGE) + _T(": ") + Text::toT(DetectionManager::getInstance()->getProfileMessage())).c_str());
	return TRUE;
}

LRESULT ClientsPage::onAddClient(WORD , WORD , HWND , BOOL&)
{
	DetectionEntry de;
	DetectionEntryDlg dlg(de);
	
	if (dlg.DoModal() == IDOK)
	{
		addEntry(de, ctrlProfiles.GetItemCount());
		SetDlgItemText(IDC_PROFILE_COUNT, (TSTRING(PROFILE_COUNT) + _T(": ") + Util::toStringW(ctrlProfiles.GetItemCount())).c_str());
	}
	
	return 0;
}

LRESULT ClientsPage::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_MOVE_CLIENT_UP), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_MOVE_CLIENT_DOWN), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_CHANGE_CLIENT), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_REMOVE_CLIENT), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

LRESULT ClientsPage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD_CLIENT, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE_CLIENT, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT ClientsPage::onChangeClient(WORD , WORD , HWND , BOOL&)
{
	if (ctrlProfiles.GetSelectedCount() == 1)
	{
		int sel = ctrlProfiles.GetSelectedIndex();
		DetectionEntry de;
		
		if (DetectionManager::getInstance()->getDetectionItem(ctrlProfiles.GetItemData(sel), de))
		{
			DetectionEntryDlg dlg(de);
			dlg.DoModal();
			
			CLockRedraw<> l_lock_draw(ctrlProfiles);
			ctrlProfiles.DeleteAllItems();
			const DetectionManager::DetectionItems& lst = DetectionManager::getInstance()->getProfiles();
			for (auto i = lst.cbegin(); i != lst.cend(); ++i)
			{
				addEntry(*i, ctrlProfiles.GetItemCount());
			}
			ctrlProfiles.SelectItem(sel);
		}
	}
	
	return 0;
}

LRESULT ClientsPage::onRemoveClient(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlProfiles.GetSelectedCount() == 1)
	{
		int i = ctrlProfiles.GetNextItem(-1, LVNI_SELECTED);
		DetectionManager::getInstance()->removeDetectionItem(ctrlProfiles.GetItemData(i));
		ctrlProfiles.DeleteItem(i);
	}
	return 0;
}

LRESULT ClientsPage::onMoveClientUp(WORD , WORD , HWND , BOOL&)
{
	int i = ctrlProfiles.GetSelectedIndex();
	if (i != -1 && i != 0)
	{
		int n = ctrlProfiles.GetItemData(i);
		DetectionManager::getInstance()->moveDetectionItem(n, -1);
		CLockRedraw<> l_lock_draw(ctrlProfiles);
		ctrlProfiles.DeleteItem(i);
		DetectionEntry de;
		DetectionManager::getInstance()->getDetectionItem(n, de);
		addEntry(de, i - 1);
		ctrlProfiles.SelectItem(i - 1);
		ctrlProfiles.EnsureVisible(i - 1, FALSE);
	}
	return 0;
}

LRESULT ClientsPage::onMoveClientDown(WORD , WORD , HWND , BOOL&)
{
	int i = ctrlProfiles.GetSelectedIndex();
	if (i != -1 && i != (ctrlProfiles.GetItemCount() - 1))
	{
		int n = ctrlProfiles.GetItemData(i);
		DetectionManager::getInstance()->moveDetectionItem(n, 1);
		CLockRedraw<> l_lock_draw(ctrlProfiles);
		ctrlProfiles.DeleteItem(i);
		DetectionEntry de;
		DetectionManager::getInstance()->getDetectionItem(n, de);
		addEntry(de, i + 1);
		ctrlProfiles.SelectItem(i + 1);
		ctrlProfiles.EnsureVisible(i + 1, FALSE);
	}
	return 0;
}

LRESULT ClientsPage::onReload(WORD , WORD , HWND , BOOL&)
{
	reload();
	return 0;
}

LRESULT ClientsPage::onUpdate(WORD , WORD , HWND , BOOL&)
{
	char buf[MAX_PATH];
	GetWindowTextA(GetDlgItem(IDC_UPDATE_URL), buf, MAX_PATH);
	::EnableWindow(GetDlgItem(IDC_UPDATE), false);
	c.downloadFile(string(buf) + "Profiles.xml");
	return 0;
}

LRESULT ClientsPage::onInfoTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	int item = ctrlProfiles.GetHotItem();
	if (item != -1)
	{
		NMLVGETINFOTIP* lpnmtdi = (NMLVGETINFOTIP*) pnmh;
		
		DetectionEntry de;
		DetectionManager::getInstance()->getDetectionItem(ctrlProfiles.GetItemData(item), de);
		const tstring infoTip = TSTRING(NAME) + _T(": ") + Text::toT(de.name) +
		                        _T("\r\n") + TSTRING(COMMENT) + _T(": ") + Text::toT(de.comment) +
		                        _T("\r\n") + TSTRING(CHEATING_DESCRIPTION) + _T(": ") + Text::toT(de.cheat);
		                        
		_tcscpy(lpnmtdi->pszText, infoTip.c_str());
	}
	return 0;
}

void ClientsPage::reload()
{
	CLockRedraw<> l_lock_draw(ctrlProfiles);
	ctrlProfiles.DeleteAllItems();
	DetectionManager::getInstance()->reload();
	const DetectionManager::DetectionItems& lst = DetectionManager::getInstance()->getProfiles();
	auto cnt = ctrlProfiles.GetItemCount();
	for (auto i = lst.cbegin(); i != lst.cend(); ++i)
	{
		const DetectionEntry& de = *i;
		addEntry(de, cnt++);
	}
	SetDlgItemText(IDC_PROFILE_COUNT, (TSTRING(PROFILE_COUNT) + _T(": ") + Util::toStringW(ctrlProfiles.GetItemCount())).c_str());
	SetDlgItemText(IDC_PROFILE_VERSION, (TSTRING(PROFILE_VERSION) + _T(": ") + Text::toT(DetectionManager::getInstance()->getProfileVersion())).c_str());
	SetDlgItemText(IDC_PROFILE_MESSAGE, (TSTRING(PROFILE_MESSAGE) + _T(": ") + Text::toT(DetectionManager::getInstance()->getProfileMessage())).c_str());
}

void ClientsPage::reloadFromHttp()
{
	CLockRedraw<> l_lock_draw(ctrlProfiles);
	ctrlProfiles.DeleteAllItems();
	
	const DetectionManager::DetectionItems& lst = DetectionManager::getInstance()->reloadFromHttp();
	auto cnt = ctrlProfiles.GetItemCount();
	for (auto i = lst.cbegin(); i != lst.cend(); ++i)
	{
		const DetectionEntry& de = *i;
		addEntry(de, cnt++);
	}
	SetDlgItemText(IDC_PROFILE_COUNT, (TSTRING(PROFILE_COUNT) + _T(": ") + Util::toStringW(ctrlProfiles.GetItemCount())).c_str());
	SetDlgItemText(IDC_PROFILE_VERSION, (TSTRING(PROFILE_VERSION) + _T(": ") + Text::toT(DetectionManager::getInstance()->getProfileVersion())).c_str());
	SetDlgItemText(IDC_PROFILE_MESSAGE, (TSTRING(PROFILE_MESSAGE) + _T(": ") + Text::toT(DetectionManager::getInstance()->getProfileMessage())).c_str());
}

void ClientsPage::addEntry(const DetectionEntry& de, int pos)
{
	TStringList lst;
	
	lst.push_back(Text::toT(de.name));
	lst.push_back(Text::toT(de.comment));
	switch (de.rawToSend)
	{
		case 1:
			lst.push_back(_T("Raw 1"));
			break;
		case 2:
			lst.push_back(_T("Raw 2"));
			break;
		case 3:
			lst.push_back(_T("Raw 3"));
			break;
		case 4:
			lst.push_back(_T("Raw 4"));
			break;
		case 5:
			lst.push_back(_T("Raw 5"));
			break;
		default:
			lst.push_back(CTSTRING(NO_ACTION));
	}
	ctrlProfiles.insert(pos, lst, 0, (LPARAM)de.Id);
}

void ClientsPage::write()
{
	DetectionManager::getInstance()->save();
	PropPage::write((HWND)*this, items);
}
// iDC++
LRESULT ClientsPage::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			try
			{
				DetectionEntry de;
				DetectionManager::getInstance()->getDetectionItem(ctrlProfiles.GetItemData(cd->nmcd.dwItemSpec), de);
				if (de.rawToSend)
				{
					cd->clrText = SETTING(BAD_CLIENT_COLOUR);
				}
				else if (!de.cheat.empty())
				{
					cd->clrText = SETTING(BAD_FILELIST_COLOUR);
				}
				if (cd->nmcd.dwItemSpec % 2 == 0)
				{
					cd->clrTextBk = RGB(245, 245, 245);
				}
				return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
			}
			catch (const Exception&)
			{
			}
			catch (...)
			{
			}
			Colors::alternationBkColor(cd); // [+] IRainman
		}
		return CDRF_NOTIFYSUBITEMDRAW;
		
		default:
			return CDRF_DODEFAULT;
	}
}
// iDC++

#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER

/**
 * @file
 * $Id: ClientsPage.cpp 477 2010-01-29 08:59:43Z bigmuscle $
 */
