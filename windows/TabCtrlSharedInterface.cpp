#include "stdafx.h"
#ifdef RIP_USE_SKIN
#include "TabCtrlSharedInterface.h"

#pragma warning(push)
#pragma warning(disable:4100)
int CSkinableTab::getHeight() const
{
	return GetHeight();
}

void CSkinableTab::SwitchTo(bool next)
{}

void CSkinableTab::Invalidate(BOOL bErase)
{
	Update();
}

void CSkinableTab::MoveWindow(LPCRECT lpRect, BOOL bRepaint)
{
}

BOOL CSkinableTab::IsWindow() const
{
	return (HWND)this != NULL;
}

HWND CSkinableTab::getNext()
{
	return NULL;
}

HWND CSkinableTab::getPrev()
{
	return NULL;
}

void CSkinableTab::setActive(HWND hWnd)
{
	DWORD dwItem = GetItemByCmd((LPARAM)hWnd);
	
	if (dwItem != 0xffffffff)
		SetItemState(dwItem, BST_CHECKED, true);
}

void CSkinableTab::addTab(HWND hWnd, COLORREF color, uint16_t icon, uint16_t stateIcon, bool mini)
{
	HICON hIcon = (HICON)LoadImage((HINSTANCE)::GetWindowLongPtr(hWnd, GWLP_HINSTANCE), MAKEINTRESOURCE(icon), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE);
	AddItem(hIcon, (LPARAM)hWnd, BS_AUTORADIOBUTTON, BST_CHECKED);
	DestroyIcon(hIcon);
}

void CSkinableTab::removeTab(HWND aWnd)
{
	DWORD dwItem = GetItemByCmd((LPARAM)aWnd);
	
	if (dwItem != 0xffffffff)
		RemoveItem(dwItem);
}

void CSkinableTab::updateText(HWND aWnd, LPCTSTR text)
{
}

void CSkinableTab::startSwitch()
{
}

void CSkinableTab::endSwitch()
{
}

void CSkinableTab::setDirty(HWND aWnd)
{
}

void CSkinableTab::setColor(HWND aWnd, COLORREF color)
{
}

void CSkinableTab::setIconState(HWND aWnd)
{
}

void CSkinableTab::unsetIconState(HWND aWnd)
{
}

void CSkinableTab::setCustomIcon(HWND aWnd, HICON custom)
{
}
#pragma warning(pop)
#endif
