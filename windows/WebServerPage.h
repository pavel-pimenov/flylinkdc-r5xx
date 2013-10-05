#ifndef WebServerPage_H
#define WebServerPage_H

#include "PropPage.h"

class WebServerPage : public CPropertyPage<IDD_WEBSERVER_PAGE>, public PropPage
#ifdef _DEBUG
	, virtual NonDerivable<WebServerPage> // [+] IRainman fix.
#endif
{
	public:
		WebServerPage(SettingsManager *s) : PropPage(s)
		{
			title = TSTRING(SETTINGS_RC) + _T('\\') + TSTRING(WEBSERVER);
			SetTitle(title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		};
		
		~WebServerPage()
		{
		};
		
		BEGIN_MSG_MAP(WebServerPage)
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
	private:
		CComboBox BindCombo;
		
	protected:
		static Item items[];
		static TextItem texts[];
		
		wstring title;
};

#endif //WebServerPage_H
