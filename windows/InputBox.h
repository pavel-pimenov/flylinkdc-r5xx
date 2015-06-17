// InputBox.h: interface for the CInputBox class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(DCPLUSPLUS_INPUTBOX_H)
#define DCPLUSPLUS_INPUTBOX_H

#pragma once

#define INPUTBOX_WIDTH 500
#define INPUTBOX_HEIGHT 190

/*
Author      : mah
Date        : 13.06.2002
Description :
    similar to Visual Basic InputBox
*/
class CInputBox
{
		static HFONT m_hFont;
		static HWND  m_hWndInputBox;
		static HWND  m_hWndParent;
		static HWND  m_hWndEditMD5;
		static HWND  m_hWndEditTTH;
		static HWND  m_hWndEditMagnet;
		static HWND  m_hWndOK;
		static HWND  m_hWndCopyMD5;
		static HWND  m_hWndCopyTTH;
		static HWND  m_hWndCopyMagnet;
		static HWND  m_hWndPrompt;
		static HWND  m_hWndPrompt1;
		static HWND  m_hWndPrompt2;
		
		static HINSTANCE m_hInst;
		
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	public:
		// text from InputBox
		LPTSTR m_Text;
		BOOL DoModal(LPCTSTR szCaption, LPCTSTR szPrompt, LPCTSTR szTextMD5, LPCTSTR szTextTTHCalc/*, LPCTSTR szTextTTHfromBase*/, LPCTSTR szTextMagnet);
		
		explicit CInputBox(HWND hWndParent);
		~CInputBox();
};

#endif // !defined(DCPLUSPLUS_INPUTBOX_H)
