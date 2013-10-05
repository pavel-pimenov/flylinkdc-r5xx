#ifndef CLIENTSPAGE_H
#define CLIENTSPAGE_H

#pragma once

#include "../client/DCPlusPlus.h"

#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER

#include "PropPage.h"
#include "ExListViewCtrl.h"

#include "../client/HttpConnection.h"
#include "../client/File.h"
#include "../client/DetectionManager.h"

class ClientsPage : public CPropertyPage<IDD_CLIENTS_PAGE>, public PropPage, private HttpConnectionListener
{
	public:
		enum { WM_PROFILE = WM_APP + 53 };
		
		ClientsPage(SettingsManager *s) : PropPage(s)
		{
			title = _tcsdup((TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_FAKEDETECT) + _T('\\') + TSTRING(SETTINGS_CLIENTS)).c_str());
			SetTitle(title);
			m_psp.dwFlags |= PSP_RTLREADING;
			c.addListener(this);
		};
		
		~ClientsPage()
		{
			c.removeListener(this);
			ctrlProfiles.Detach();
			free(title);
		};
		
		BEGIN_MSG_MAP(ClientsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ADD_CLIENT, onAddClient)
		COMMAND_ID_HANDLER(IDC_REMOVE_CLIENT, onRemoveClient)
		COMMAND_ID_HANDLER(IDC_CHANGE_CLIENT, onChangeClient)
		COMMAND_ID_HANDLER(IDC_MOVE_CLIENT_UP, onMoveClientUp)
		COMMAND_ID_HANDLER(IDC_MOVE_CLIENT_DOWN, onMoveClientDown)
		COMMAND_ID_HANDLER(IDC_RELOAD_CLIENTS, onReload)
		COMMAND_ID_HANDLER(IDC_UPDATE, onUpdate)
		NOTIFY_HANDLER(IDC_CLIENT_ITEMS, NM_CUSTOMDRAW, onCustomDraw)
		NOTIFY_HANDLER(IDC_CLIENT_ITEMS, NM_DBLCLK, onDblClick)
		NOTIFY_HANDLER(IDC_CLIENT_ITEMS, LVN_GETINFOTIP, onInfoTip)
		NOTIFY_HANDLER(IDC_CLIENT_ITEMS, LVN_ITEMCHANGED, onItemchangedDirectories)
		NOTIFY_HANDLER(IDC_CLIENT_ITEMS, LVN_KEYDOWN, onKeyDown)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		
		LRESULT onAddClient(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChangeClient(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemoveClient(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMoveClientUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMoveClientDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onReload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onUpdate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onInfoTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onItemchangedDirectories(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		
		LRESULT onDblClick(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& bHandled)
		{
			return onChangeClient(0, 0, 0, bHandled);
		}
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel() {}
	private:
		ExListViewCtrl ctrlProfiles;
		
		static Item items[];
		static TextItem texts[];
		TCHAR* title;
		
		void addEntry(const DetectionEntry& de, int pos);
		void reload();
		void reloadFromHttp();
		
		void on(HttpConnectionListener::Data, HttpConnection* /*conn*/, const uint8_t* buf, size_t len) noexcept
		{
			downBuf.append((char*)buf, len);
		}
		void on(HttpConnectionListener::Complete, HttpConnection* conn, const string& /*aLine*/) noexcept
		{
			conn->removeListener(this);
			if (!downBuf.empty())
			{
				string fname = Util::getConfigPath() + "Profiles.xml";
				File f(fname + ".tmp", File::WRITE, File::CREATE | File::TRUNCATE);
				f.write(downBuf);
				f.close();
				File::deleteFile(fname);
				File::renameFile(fname + ".tmp", fname);
				reloadFromHttp();
				MessageBox(CTSTRING(CLIENT_PROFILE_NOW_UPDATED), CTSTRING(UPDATE), MB_OK);
			}
			::EnableWindow(GetDlgItem(IDC_UPDATE), true);
		}
		void on(HttpConnectionListener::Failed, HttpConnection* conn, const string& aLine) noexcept
		{
			conn->removeListener(this);
			{
				const tstring msg = Text::toT(STRING(CLIENT_PROFILE_DOWNLOAD_FAILED) + aLine);
				MessageBox(msg.c_str(), CTSTRING(DOWNLOAD_FAILED), MB_OK);
			}
			::EnableWindow(GetDlgItem(IDC_UPDATE), true);
		}
		HttpConnection c;
		string downBuf;
};
#else
class ClientsPage : public EmptyPage
{
	public:
		ClientsPage(SettingsManager *s) : EmptyPage(s, TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_FAKEDETECT) + _T('\\') + TSTRING(SETTINGS_CLIENTS))
		{
		}
};
#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER

#endif //CLIENTSPAGE_H

/**
 * @file
 * $Id: ClientsPage.h 453 2009-08-04 15:46:31Z BigMuscle $
 */
