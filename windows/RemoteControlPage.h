#ifndef RemoteControlPage_H
#define RemoteControlPage_H

#include "PropPage.h"

class RemoteControlPage : public CPropertyPage<IDD_REMOTE_CONTROL_PAGE>, public PropPage
#ifdef _DEBUG
	, virtual NonDerivable<RemoteControlPage> // [+] IRainman fix.
#endif
{
	public:
		RemoteControlPage(SettingsManager *s) : PropPage(s, TSTRING(SETTINGS_RC))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~RemoteControlPage()
		{
		}
		
		BEGIN_MSG_MAP(RemoteControlPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel() {}
	protected:
		static Item items[];
		static TextItem texts[];
		
		
};

#endif //RemoteControlPage_H
