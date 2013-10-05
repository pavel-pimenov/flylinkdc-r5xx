#include "stdafx.h"
#include "InputBox.h"
#include "WinUtil.h"

HFONT CInputBox::m_hFont = NULL;
HWND  CInputBox::m_hWndInputBox = NULL;
HWND  CInputBox::m_hWndParent = NULL;
HWND  CInputBox::m_hWndEditMD5 = NULL;
HWND  CInputBox::m_hWndEditTTH = NULL;
HWND  CInputBox::m_hWndEditMagnet = NULL;
HWND  CInputBox::m_hWndOK = NULL;
HWND  CInputBox::m_hWndCopyMD5 = NULL;
HWND  CInputBox::m_hWndCopyTTH = NULL;
HWND  CInputBox::m_hWndCopyMagnet = NULL;
HWND  CInputBox::m_hWndPrompt = NULL;
HWND  CInputBox::m_hWndPrompt1 = NULL;
HWND  CInputBox::m_hWndPrompt2 = NULL;

HINSTANCE CInputBox::m_hInst = nullptr;

//////////////////////////////////////////////////////////////////////
// CInputBox Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

/*
Author      : mah
Date        : 13.06.2002
Description :
    Constructs window class InputBox
*/
CInputBox::CInputBox(HWND hWndParent) : m_Text(nullptr)
{
	HINSTANCE hInst = GetModuleHandle(NULL);
	
	WNDCLASSEX wcex = {0};
	
	if (!GetClassInfoEx(hInst, _T("InputBox"), &wcex))
	{
		wcex.cbSize = sizeof(WNDCLASSEX);
		
		wcex.style          = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc    = (WNDPROC)WndProc;
		wcex.cbClsExtra     = 0;
		wcex.cbWndExtra     = 0;
		wcex.hInstance      = hInst;
		wcex.hIcon          = NULL;//LoadIcon(hInst, (LPCTSTR)IDI_MYINPUTBOX);
		wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW);
		wcex.lpszMenuName   = NULL;
		wcex.lpszClassName  = _T("InputBox");
		wcex.hIconSm        = NULL;
		
		if (RegisterClassEx(&wcex) == 0)
			MessageBox(NULL, CTSTRING(CANT_CREATE), CTSTRING(ERROR_STRING), MB_OK);
	}
	
	m_hWndParent = hWndParent;
}

CInputBox::~CInputBox()
{
	delete[] m_Text;
}

/*
Author      : mah
Date        : 13.06.2002
Description : Window procedure
*/
LRESULT CALLBACK CInputBox::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LOGFONT lfont = {0};
	
	switch (message)
	{
		case WM_CREATE:
			// font
			lstrcpy(lfont.lfFaceName, _T("Arial"));
			lfont.lfHeight = 16;
			lfont.lfWeight = FW_NORMAL;//FW_BOLD;
			lfont.lfItalic = FALSE;
			lfont.lfCharSet = DEFAULT_CHARSET;
			lfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
			lfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
			lfont.lfQuality = DEFAULT_QUALITY;
			lfont.lfPitchAndFamily = DEFAULT_PITCH;
			m_hFont = CreateFontIndirect(&lfont);
			
			m_hInst = GetModuleHandle(NULL);
			
			// creating Edit
			m_hWndEditMD5 = CreateWindowEx(WS_EX_STATICEDGE,
			                               _T("edit"), _T(""),
			                               WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL | ES_READONLY,
			                               5, INPUTBOX_HEIGHT - 90, INPUTBOX_WIDTH - 145, 20,
			                               hWnd,
			                               NULL,
			                               m_hInst,
			                               NULL);
			                               
			// setting font
			SendMessage(m_hWndEditMD5, WM_SETFONT, (WPARAM)m_hFont, 0);
			
			// creating Edit
			m_hWndEditTTH = CreateWindowEx(WS_EX_STATICEDGE,
			                               _T("edit"), _T(""),
			                               WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_READONLY,
			                               5, INPUTBOX_HEIGHT - 50, INPUTBOX_WIDTH - 16, 20,
			                               hWnd,
			                               NULL,
			                               m_hInst,
			                               NULL);
			                               
			// setting font
			SendMessage(m_hWndEditTTH, WM_SETFONT, (WPARAM)m_hFont, 0);
			
			// creating Edit
			m_hWndEditMagnet = CreateWindowEx(WS_EX_STATICEDGE,
			                                  _T("edit"), _T(""),
			                                  WS_CHILD  | WS_TABSTOP | ES_READONLY,
			                                  5, INPUTBOX_HEIGHT - 50, INPUTBOX_WIDTH - 16, 20,
			                                  hWnd,
			                                  NULL,
			                                  m_hInst,
			                                  NULL);
			                                  
			// setting font
			SendMessage(m_hWndEditMagnet, WM_SETFONT, (WPARAM)m_hFont, 0);
			
			// button OK
			m_hWndOK = CreateWindowEx(NULL,
			                          _T("button"), CTSTRING(OK),
			                          WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			                          INPUTBOX_WIDTH - 135, 10, 125, 23,
			                          hWnd,
			                          NULL,
			                          m_hInst,
			                          NULL);
			                          
			// setting font
			SendMessage(m_hWndOK, WM_SETFONT, (WPARAM)m_hFont, 0);
//[+]IRainman
			// button Copy MD5
			m_hWndCopyMD5 = CreateWindowEx(NULL,
			                               _T("button"), CTSTRING(COPY_MD5),
			                               WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			                               INPUTBOX_WIDTH - 135, 40, 125, 23,
			                               hWnd,
			                               NULL,
			                               m_hInst,
			                               NULL);
			                               
			// setting font
			SendMessage(m_hWndCopyMD5, WM_SETFONT, (WPARAM)m_hFont, 0);
			
			// button Copy TTH
			m_hWndCopyTTH = CreateWindowEx(NULL,
			                               _T("button"), CTSTRING(COPY_TTH),
			                               WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			                               INPUTBOX_WIDTH - 135, 70, 125, 23,
			                               hWnd,
			                               NULL,
			                               m_hInst,
			                               NULL);
			                               
			// setting font
			SendMessage(m_hWndCopyTTH, WM_SETFONT, (WPARAM)m_hFont, 0);
			
			// button Copy magnet link
			m_hWndCopyMagnet = CreateWindowEx(NULL,
			                                  _T("button"), CTSTRING(COPY_MAGNET),
			                                  WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			                                  INPUTBOX_WIDTH - 135, 100, 125, 23,
			                                  hWnd,
			                                  NULL,
			                                  m_hInst,
			                                  NULL);
			                                  
			// setting font
			SendMessage(m_hWndCopyMagnet, WM_SETFONT, (WPARAM)m_hFont, 0);
//[~]IRainman
			// static Propmpt
			m_hWndPrompt = CreateWindowEx(WS_EX_STATICEDGE,
			                              _T("static"), _T(""),
			                              WS_VISIBLE | WS_CHILD,
			                              5, 10, INPUTBOX_WIDTH - 145, INPUTBOX_HEIGHT - 135,
			                              hWnd,
			                              NULL,
			                              m_hInst,
			                              NULL);
			                              
			// setting font
			SendMessage(m_hWndPrompt, WM_SETFONT, (WPARAM)m_hFont, 0);
			
			// static Propmpt
			m_hWndPrompt1 = CreateWindowEx(NULL,
			                               _T("static"), _T(""),
			                               WS_VISIBLE | WS_CHILD,
			                               5, 83, INPUTBOX_WIDTH - 145, 16,
			                               hWnd,
			                               NULL,
			                               m_hInst,
			                               NULL);
			                               
			//HDC dc = GetDC(m_hWndPrompt1);
			//SelectObject(dc, m_hWndPrompt1);
//          SetBkColor(dc, COLOR_WINDOW);

			// setting font
			SendMessage(m_hWndPrompt1, WM_SETFONT, (WPARAM)m_hFont, 0);
			SendMessage(m_hWndPrompt1, WM_CTLCOLORSTATIC, (WPARAM)COLOR_WINDOW, 0);
			// static Propmpt
			m_hWndPrompt2 = CreateWindowEx(NULL,
			                               _T("static"), _T(""),
			                               WS_VISIBLE | WS_CHILD,
			                               5, 123, INPUTBOX_WIDTH - 16, 16,
			                               hWnd,
			                               NULL,
			                               m_hInst,
			                               NULL);
			                               
			// setting font
			SendMessage(m_hWndPrompt2, WM_SETFONT, (WPARAM)m_hFont, 0);
			
			SetFocus(m_hWndEditTTH);
			break;
		case WM_DESTROY:
		
			DeleteObject(m_hFont);
			
			EnableWindow(m_hWndParent, TRUE);
			SetForegroundWindow(m_hWndParent);
			DestroyWindow(hWnd);
			PostQuitMessage(0);
			
			break;
			//case WM_CTLCOLORSTATIC:
			//  if (((HWND)lParam == m_hWndPrompt1) || ((HWND)lParam == m_hWndPrompt2))
			//  {
			//      SetBkColor((HDC) wParam, GetSysColor(COLOR_WINDOW));
			//      return((DWORD)  CreateSolidBrush(GetSysColor(COLOR_WINDOW)));
			//  }
			//  return DefWindowProc(hWnd, message, wParam, lParam);
			//  break;
		case WM_COMMAND:
			switch (HIWORD(wParam))
			{
				case BN_CLICKED:
					if ((HWND)lParam == m_hWndOK)
					{
						PostMessage(m_hWndInputBox, WM_KEYDOWN, VK_RETURN, 0);
						break;
					}
					else
					{
						// [+] IRainman
						tstring buf;
						
#define GET_WINDOW_TEXT(p) \
	if ((HWND)lParam == m_hWndCopy##p)\
	{\
		WinUtil::GetWindowText(buf, m_hWndEdit##p);\
	}
						GET_WINDOW_TEXT(MD5)
						else GET_WINDOW_TEXT(TTH)
							else GET_WINDOW_TEXT(Magnet)
								else
									break;
									
#undef GET_WINDOW_TEXT
									
						WinUtil::setClipboard(buf);
					}
					// [~] IRainman
					break;
			}
			break;
			
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

/*
Author      : mah
Date        : 13.06.2002
Description :
        Constructs InputBox window
*/
BOOL CInputBox::DoModal(LPCTSTR szCaption, LPCTSTR szPrompt, LPCTSTR szTextMD5, LPCTSTR szTextTTHCalc/*, LPCTSTR szTextTTHfromBase*/, LPCTSTR szTextMagnet)
{
	RECT r;
	GetWindowRect(GetDesktopWindow(), &r);
	
	m_hWndInputBox = CreateWindowEx(WS_EX_TOOLWINDOW,
	                                _T("InputBox"),
	                                szCaption,
	                                WS_POPUPWINDOW | WS_CAPTION | WS_TABSTOP,
	                                (r.right - INPUTBOX_WIDTH) / 2, (r.bottom - INPUTBOX_HEIGHT) / 2,
	                                INPUTBOX_WIDTH, INPUTBOX_HEIGHT,
	                                m_hWndParent,
	                                NULL,
	                                m_hInst,
	                                NULL);
	if (m_hWndInputBox == NULL)
		return FALSE;
		
	SetWindowText(m_hWndPrompt, szPrompt);
	SetWindowText(m_hWndEditMD5, szTextMD5);
	SetWindowText(m_hWndEditTTH, szTextTTHCalc);
	SetWindowText(m_hWndEditMagnet, szTextMagnet);
	
	SetWindowText(m_hWndPrompt1, _T("MD5:"));
	SetWindowText(m_hWndPrompt2, _T("TTH:"));
	
	SetForegroundWindow(m_hWndInputBox);
	
	EnableWindow(m_hWndParent, FALSE);
	
	ShowWindow(m_hWndInputBox, SW_SHOW);
	UpdateWindow(m_hWndInputBox);
	
	BOOL ret = 0;
	
	MSG msg = {0};
	
	HWND hWndFocused = nullptr;
	
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == WM_KEYDOWN)
		{
			if (msg.wParam == VK_ESCAPE)
			{
				SendMessage(m_hWndInputBox, WM_DESTROY, 0, 0);
				ret = 0;
			}
			if (msg.wParam == VK_RETURN)
			{
				const int nCount = GetWindowTextLength(m_hWndEditTTH) + 1;
				delete[] m_Text;
				m_Text = new TCHAR[static_cast<size_t>(nCount)];
				GetWindowText(m_hWndEditTTH, m_Text, nCount);
				SendMessage(m_hWndInputBox, WM_DESTROY, 0, 0);
				ret = 1;
			}
			if (msg.wParam == VK_TAB) // :)
			{
				hWndFocused = GetFocus();
				if (hWndFocused == m_hWndEditTTH) SetFocus(m_hWndOK);
				if (hWndFocused == m_hWndOK) SetFocus(m_hWndEditTTH);
			}
			
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	return ret;
}
