#pragma once
#ifdef RIP_USE_SKIN

#include "SkinableCmdBar.h"

class ITabCtrl
{
	public:
		virtual int getHeight() const = 0;
		virtual void SwitchTo(bool next = true) = 0;
		virtual void Invalidate(BOOL bErase = TRUE) = 0;
		virtual void MoveWindow(LPCRECT lpRect, BOOL bRepaint = TRUE) = 0;
		virtual BOOL IsWindow() const = 0;
		virtual HWND getNext() = 0;
		virtual HWND getPrev() = 0;
		virtual void setActive(HWND hWnd) = 0;
		virtual void addTab(HWND hWnd, COLORREF color = RGB(0, 0, 0), uint16_t icon = 0, uint16_t stateIcon = 0, bool p_mini = false) = 0;
		virtual void removeTab(HWND aWnd) = 0;
		virtual void updateText(HWND aWnd, LPCTSTR text) = 0;
		virtual void startSwitch() = 0;
		virtual void endSwitch() = 0;
		virtual void setDirty(HWND aWnd) = 0;
		virtual void setColor(HWND aWnd, COLORREF color) = 0;
		virtual void setIconState(HWND aWnd) = 0;
		virtual void unsetIconState(HWND aWnd) = 0;
		virtual void setCustomIcon(HWND aWnd, HICON custom) = 0;
};

class CSkinableTab: public ITabCtrl, public CSkinableCmdBar
{
	public:
		int getHeight() const;
		void SwitchTo(bool next = true);
		void Invalidate(BOOL bErase = TRUE);
		void MoveWindow(LPCRECT lpRect, BOOL bRepaint = TRUE);
		BOOL IsWindow() const;
		HWND getNext();
		HWND getPrev();
		void setActive(HWND hWnd);
		void addTab(HWND hWnd, COLORREF color = RGB(0, 0, 0), uint16_t icon = 0, uint16_t stateIcon = 0, bool p_mini = false);
		void removeTab(HWND aWnd);
		void updateText(HWND aWnd, LPCTSTR text);
		void startSwitch();
		void endSwitch();
		void setDirty(HWND aWnd);
		void setColor(HWND aWnd, COLORREF color);
		void setIconState(HWND aWnd);
		void unsetIconState(HWND aWnd);
		void setCustomIcon(HWND aWnd, HICON custom);
};
#endif