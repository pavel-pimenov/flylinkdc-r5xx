#if !defined(SLOT_PAGE_H)
#define SLOT_PAGE_H

#include <atlcrack.h>
#include "PropPage.h"
#include "WinUtil.h"
#include "../client/SettingsManager.h"

class SlotPage : public CPropertyPage<IDD_SLOT_PAGE>, public PropPage
{
	public:
		explicit SlotPage(SettingsManager *s) : PropPage(s, TSTRING(SLOTS))
#ifdef SSA_IPGRANT_FEATURE
			, m_isEnabledIPGrant(false)
#endif
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~SlotPage()
		{
		}
		
		BEGIN_MSG_MAP_EX(SlotPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
#ifdef SSA_IPGRANT_FEATURE
		COMMAND_ID_HANDLER(IDC_EXTRA_SLOT_BY_IP, onFixControls)
#endif
		CHAIN_MSG_MAP(PropPage)
		REFLECT_NOTIFICATIONS()
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#ifdef SSA_IPGRANT_FEATURE
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			fixControls();
			return 0;
		}
#endif
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel()
		{
			cancel_check();
		}
	protected:
#ifdef SSA_IPGRANT_FEATURE
		void fixControls();
#endif
		static Item items[];
		static TextItem texts[];
		
#ifdef SSA_IPGRANT_FEATURE
	private:
		string m_IPGrant;
		string m_IPGrantPATH;
		bool m_isEnabledIPGrant;
#endif
};

#endif // !defined(SLOT_PAGE_H)
