#ifndef __EMOTICONS_DLG
#define __EMOTICONS_DLG

#ifdef IRAINMAN_INCLUDE_SMILE

#include "../GdiOle/GDIImage.h"
#include "wtl_flylinkdc.h"

class CAnimatedButton: public CWindowImpl<CAnimatedButton, CButton>
{
		CGDIImage *m_pImage;
		HDC m_hDC;
		bool m_bInited;
		int m_xBk;
		int m_yBk;
		int m_xSrc;
		int m_ySrc;
		int m_wSrc;
		int m_hSrc;
		int m_h, m_w;
		
		HDC m_hBackDC;
		
		static bool __cdecl OnFrameChanged(CGDIImage *pImage, LPARAM lParam);
		
	public:
		BEGIN_MSG_MAP(CAnimatedButton)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_ERASEBKGND, onErase)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		END_MSG_MAP()
		
		CAnimatedButton(CGDIImage *pImage);
		~CAnimatedButton();
		
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onErase(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		void Update();
};

class EmoticonsDlg : public CDialogImpl<EmoticonsDlg>
{
	public:
		enum { IDD = IDD_EMOTICONS_DLG };
		
		
		BEGIN_MSG_MAP(EmoticonsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_RBUTTONUP, onClose)
		MESSAGE_HANDLER(WM_LBUTTONUP, onClose)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		COMMAND_CODE_HANDLER(BN_CLICKED, onEmoticonClick)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onEmoticonClick(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			// !BUGMASTER!
			cleanHandleList();
			EndDialog(0);
			return 0;
		}
		
		~EmoticonsDlg();
		
		tstring result;
		CRect pos;
		
	private:
		CFlyToolTipCtrl m_ctrlToolTip;
		
		static WNDPROC g_MFCWndProc;
		static EmoticonsDlg* g_pDialog;
		static LRESULT CALLBACK NewWndProc(HWND, UINT, WPARAM, LPARAM);
		
		void cleanHandleList();
		// !BUGMASTER!
		typedef std::vector<HBITMAP> HList;
		typedef std::vector<CAnimatedButton*> tBTN_LIST;
		HList m_HandleList;
		tBTN_LIST m_BtnList;
};

#endif // IRAINMAN_INCLUDE_SMILE

#endif // __EMOTICONS_DLG