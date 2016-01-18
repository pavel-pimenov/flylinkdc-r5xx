#include "stdafx.h"

#ifdef IRAINMAN_INCLUDE_SMILE
#include "Resource.h"
#include "EmoticonsDlg.h"
#include "WinUtil.h"
#include "MainFrm.h"

#ifndef AGEMOTIONSETUP_H__
#include "AGEmotionSetup.h"
#endif

//[?] #pragma comment(lib, "GdiOle.lib")

#define EMOTICONS_ICONMARGIN 8

WNDPROC EmoticonsDlg::g_MFCWndProc = nullptr;
EmoticonsDlg* EmoticonsDlg::g_pDialog = nullptr;

LRESULT EmoticonsDlg::onEmoticonClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	WinUtil::GetWindowText(result, hWndCtl);
	// pro ucely testovani emoticon packu...
	if (WinUtil::isShift() && WinUtil::isCtrl())
	{
		const CAGEmotion::Array& Emoticons = CAGEmotionSetup::g_pEmotionsSetup->getEmoticonsArray();
		result.clear();
		string lastEmotionPath, lastAnimEmotionPath;
		int l_count_emotion = 0;
		bool bUseAnimation = BOOLSETTING(SMILE_SELECT_WND_ANIM_SMILES);
		
		for (auto pEmotion = Emoticons.cbegin();
		        pEmotion != Emoticons.cend() && l_count_emotion < CAGEmotionSetup::g_pEmotionsSetup->m_CountSelEmotions;
		        ++pEmotion, ++l_count_emotion)
		{
			if (bUseAnimation)
			{
				if (lastEmotionPath != (*pEmotion)->getEmotionBmpPath() || lastAnimEmotionPath != (*pEmotion)->getEmotionGifPath())
					result += (*pEmotion)->getEmotionText() + _T(' ');
					
				lastEmotionPath = (*pEmotion)->getEmotionBmpPath();
				lastAnimEmotionPath = (*pEmotion)->getEmotionGifPath();
			}
			else
			{
				if (lastEmotionPath != (*pEmotion)->getEmotionBmpPath())
					result += (*pEmotion)->getEmotionText() + _T(' ');
				lastEmotionPath = (*pEmotion)->getEmotionBmpPath();
			}
		}
	}
	PostMessage(WM_CLOSE);
	return 0;
}

void EmoticonsDlg::cleanHandleList()
{
	for (auto i = m_HandleList.cbegin(); i != m_HandleList.cend(); ++i)
		DeleteObject(*i);
	m_HandleList.clear();
}

LRESULT EmoticonsDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ShowWindow(SW_HIDE);
	WNDPROC temp = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(EmoticonsDlg::m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(NewWndProc)));
	if (!g_MFCWndProc)
		g_MFCWndProc = temp;
	g_pDialog = this;
	::EnableWindow(WinUtil::g_mainWnd, true);
	
	bool bUseAnimation = BOOLSETTING(SMILE_SELECT_WND_ANIM_SMILES);
	
	if (CAGEmotionSetup::g_pEmotionsSetup)
	{
		const CAGEmotion::Array& Emoticons = CAGEmotionSetup::g_pEmotionsSetup->getEmoticonsArray();
		unsigned int pocet = 0;
		int l_count_emotion = 0;
		string lastEmotionPath, lastAnimEmotionPath;
		for (auto pEmotion = Emoticons.cbegin();
		        pEmotion != Emoticons.cend() && l_count_emotion < CAGEmotionSetup::g_pEmotionsSetup->m_CountSelEmotions;
		        ++pEmotion, ++l_count_emotion)
		{
			if (bUseAnimation)
			{
				if ((*pEmotion)->getEmotionBmpPath() != lastEmotionPath || (*pEmotion)->getEmotionGifPath() != lastAnimEmotionPath)
					pocet++;
					
				lastEmotionPath = (*pEmotion)->getEmotionBmpPath();
				lastAnimEmotionPath = (*pEmotion)->getEmotionGifPath();
			}
			else
			{
				if ((*pEmotion)->getEmotionBmpPath() != lastEmotionPath)
					pocet++;
				lastEmotionPath = (*pEmotion)->getEmotionBmpPath();
			}
		}
		
		// x, y jen pro for cyklus
		const unsigned int l_Emoticons_size = CAGEmotionSetup::g_pEmotionsSetup->m_CountSelEmotions;
		unsigned int i = (unsigned int)sqrt(double(l_Emoticons_size));
		unsigned int nXfor = i;
		unsigned int nYfor = i;
		
		if ((i * i) != l_Emoticons_size) //[+]PPA
		{
			nXfor = i + 1;
			if ((i * nXfor) < l_Emoticons_size) nYfor = i + 1;
			else nYfor = i;
		}
		// x, y pro korektni vkladani ikonek za sebou
		i = (unsigned int)sqrt((double)pocet);
		unsigned int nX = i;
		unsigned int nY = i;
		if ((i * i) != pocet) //[+]PPA
		{
			nX = i + 1;
			if ((i * nX) < pocet) nY = i + 1;
			else nY = i;
		}
		if (Emoticons.empty() || !*Emoticons.begin()) //[+]PPA
			return 0;
			
		// [~] brain-ripper
		// If first icon failed to load, h_bm will be zero, and all icons will be drawn extremely small.
		// So cycle through Emoticons and find first loaded icon.
		//HBITMAP h_bm = (*Emoticons.begin())->getEmotionBmp(GetSysColor(COLOR_BTNFACE));
		
		DWORD iSW = 0, iSH = 0, dwCount = 0;
		l_count_emotion = 0;
		for (auto i = Emoticons.cbegin(); i != Emoticons.cend() && l_count_emotion < CAGEmotionSetup::g_pEmotionsSetup->m_CountSelEmotions; ++i, ++l_count_emotion)
		{
			int w = 0, h = 0;
			CGDIImage *pImage = nullptr;
			
			if (bUseAnimation)
				pImage = (*i)->getAnimatedImage(MainFrame::getMainFrame()->m_hWnd, WM_ANIM_CHANGE_FRAME);
				
			if (pImage)
			{
				w = pImage->GetWidth();
				h = pImage->GetHeight();
				dwCount++;
			}
			else
			{
				if ((*i)->getDimensions(&w, &h))
				{
					if (bUseAnimation)
						dwCount++;
					else
					{
						iSW = w;
						iSH = h;
						break;
					}
				}
			}
			
			iSW += w * w;
			iSH += h * h;
		}
		
		if (bUseAnimation && dwCount)
		{
			// Get mean square of all icon dimensions
			iSW = DWORD(sqrt(float(iSW / dwCount)));
			iSH = DWORD(sqrt(float(iSH / dwCount)));
		}
		
		pos.bottom = pos.top - 3;
		pos.left = pos.right - nX * (iSW + EMOTICONS_ICONMARGIN) - 2;
		pos.top = pos.bottom - nY * (iSH + EMOTICONS_ICONMARGIN) - 2;
		
		// [+] brain-ripper
		// Fit window in screen's work area
		RECT rcWorkArea;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
		if (pos.right > rcWorkArea.right)
		{
			pos.left -= pos.right - rcWorkArea.right;
			pos.right = rcWorkArea.right;
		}
		if (pos.bottom > rcWorkArea.bottom)
		{
			pos.top -= pos.bottom - rcWorkArea.bottom;
			pos.bottom = rcWorkArea.bottom;
		}
		if (pos.left < rcWorkArea.left)
		{
			pos.right += rcWorkArea.left - pos.left;
			pos.left = rcWorkArea.left;
		}
		if (pos.top < rcWorkArea.top)
		{
			pos.bottom += rcWorkArea.top - pos.top;
			pos.top = rcWorkArea.top;
		}
		
		MoveWindow(pos);
		
		lastEmotionPath.clear();
		lastAnimEmotionPath.clear();
		
		m_ctrlToolTip.Create(EmoticonsDlg::m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
		m_ctrlToolTip.SetDelayTime(TTDT_AUTOMATIC, 1000);
		
		pos.left = 0;
		pos.right = iSW + EMOTICONS_ICONMARGIN;
		pos.top = 0;
		pos.bottom = iSH + EMOTICONS_ICONMARGIN;
		
		cleanHandleList();
		auto l_Emotion = Emoticons.begin();
		for (unsigned int iY = 0; iY < nYfor; iY++)
			for (unsigned int iX = 0; iX < nXfor; iX++)
			{
				if (l_Emotion != Emoticons.end()) // TODO - merge
				{
				
					const auto i = *l_Emotion;
					if ((iY * nXfor) + iX + 1 > l_Emoticons_size) break;
					
					bool bNotDuplicated = (bUseAnimation ?
					                       (i->getEmotionBmpPath() != lastEmotionPath ||
					                        i->getEmotionGifPath() != lastAnimEmotionPath) :
					                       i->getEmotionBmpPath() != lastEmotionPath);
					                       
					                       
					// dve stejne emotikony za sebou nechceme
					if (bNotDuplicated)
					{
						bool bCreated = false;
						CGDIImage *pImage = nullptr;
						
						if (bUseAnimation)
							pImage = i->getAnimatedImage(MainFrame::getMainFrame()->m_hWnd, WM_ANIM_CHANGE_FRAME);
							
						if (pImage)
						{
							const tstring smajl = i->getEmotionText();
							CAnimatedButton *pemoButton = new CAnimatedButton(pImage);
							m_BtnList.push_back(pemoButton);
							pemoButton->Create(EmoticonsDlg::m_hWnd, pos, smajl.c_str(), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_FLAT | BS_BITMAP, WS_EX_TRANSPARENT);
							m_ctrlToolTip.AddTool(*pemoButton, smajl.c_str());
							bCreated = true;
						}
						else
						{
							if (const HBITMAP l_h_bm = i->getEmotionBmp(GetSysColor(COLOR_BTNFACE)))
							{
								CButton emoButton;
								const tstring smajl = i->getEmotionText();
								emoButton.Create(EmoticonsDlg::m_hWnd, pos, smajl.c_str(), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_FLAT | BS_BITMAP | BS_CENTER);
								m_HandleList.push_back(l_h_bm);
								emoButton.SetBitmap(l_h_bm);
								m_ctrlToolTip.AddTool(emoButton, smajl.c_str());
								DeleteObject((HGDIOBJ)emoButton);
								bCreated = true;
							}
						}
						
						if (bCreated)
						{
							// Calculate position of next button
							pos.left = pos.left + iSW + EMOTICONS_ICONMARGIN;
							pos.right = pos.left + iSW + EMOTICONS_ICONMARGIN;
							if (pos.left >= (LONG)(nX * (iSW + EMOTICONS_ICONMARGIN)))
							{
								pos.left = 0;
								pos.right = iSW + EMOTICONS_ICONMARGIN;
								pos.top = pos.top + iSH + EMOTICONS_ICONMARGIN;
								pos.bottom = pos.top + iSH + EMOTICONS_ICONMARGIN;
							}
						}
					}
					
					lastEmotionPath = i->getEmotionBmpPath();
					
					if (bUseAnimation)
						lastAnimEmotionPath = i->getEmotionGifPath();
					++l_Emotion;
				}
			}
			
		pos.left = -128;
		pos.right = pos.left;
		pos.top = -128;
		pos.bottom = pos.top;
		CButton emoButton;
		emoButton.Create(EmoticonsDlg::m_hWnd, pos, _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_FLAT);
		emoButton.SetFocus();
		DeleteObject((HGDIOBJ)emoButton);
		ShowWindow(SW_SHOW);
		
		for (auto i = m_BtnList.cbegin(); i != m_BtnList.cend(); ++i)
		{
			(*i)->Update();
		}
	}
	else PostMessage(WM_CLOSE);
	
	return 0;
}

LRESULT CALLBACK EmoticonsDlg::NewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (g_pDialog && //[+]PPA
	        message == WM_ACTIVATE && wParam == 0)
	{
		g_pDialog->PostMessage(WM_CLOSE);
		return FALSE;
	}
	return ::CallWindowProc(g_MFCWndProc, hWnd, message, wParam, lParam);
}


EmoticonsDlg::~EmoticonsDlg()
{
	for (auto i = m_BtnList.cbegin(); i != m_BtnList.cend(); ++i)
	{
		delete *i;
	}
}

CAnimatedButton::CAnimatedButton(CGDIImage *pImage):
	m_pImage(pImage), m_bInited(false), m_hBackDC(nullptr) , m_hDC(nullptr)
{
	m_xBk = 0;
	m_yBk = 0;
	m_xSrc = 0;
	m_ySrc = 0;
	m_wSrc = 0;
	m_hSrc = 0;
	m_h = m_w = 0;

	if (m_pImage)
	{
		m_pImage->AddRef();
	}
}

CAnimatedButton::~CAnimatedButton()
{
	BOOL bNull;
	onClose(0, 0, 0, bNull);
}

bool CAnimatedButton::OnFrameChanged(CGDIImage *pImage, LPARAM lParam)
{
	CAnimatedButton *pBtn = (CAnimatedButton*)lParam;
	
	if (pBtn->m_hDC)
	{
		pImage->Draw(pBtn->m_hDC,
		             0,
		             0,
		             pBtn->m_w,
		             pBtn->m_h,
		             pBtn->m_xSrc,
		             pBtn->m_ySrc,
		             pBtn->m_hBackDC,
		             pBtn->m_xBk,
		             pBtn->m_yBk,
		             pBtn->m_wSrc,
		             pBtn->m_hSrc);
	}
	
	return true;
}

LRESULT CAnimatedButton::onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ValidateRect(NULL);
	
	bHandled = false;
	
	return 1;
}

LRESULT CAnimatedButton::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (m_pImage)
	{
		RECT rect;
		GetClientRect(&rect);
		
		m_w = rect.right - rect.left;
		m_h = rect.bottom - rect.top;
		int img_w = m_pImage->GetWidth();
		int img_h = m_pImage->GetHeight();
		
		m_hDC = ::GetDC(m_hWnd);
		m_hBackDC = m_pImage->CreateBackDC(GetSysColor(COLOR_BTNFACE), m_w, m_h);
		
		int iOffsetX = (m_w - img_w) / 2;
		int iOffsetY = (m_h - img_h) / 2;
		
		m_xBk = iOffsetX >= EMOTICONS_ICONMARGIN / 2 ? iOffsetX : EMOTICONS_ICONMARGIN / 2;
		m_yBk = iOffsetY >= EMOTICONS_ICONMARGIN / 2 ? iOffsetY : EMOTICONS_ICONMARGIN / 2;
		
		m_xSrc = iOffsetX >= EMOTICONS_ICONMARGIN / 2 ? 0 : EMOTICONS_ICONMARGIN / 2 - iOffsetX;
		m_ySrc = iOffsetY >= EMOTICONS_ICONMARGIN / 2 ? 0 : EMOTICONS_ICONMARGIN / 2 - iOffsetY;
		
		m_wSrc = img_w - m_xSrc * 2;
		m_hSrc = img_h - m_ySrc * 2;
		
		/*
		HTHEME hTheme = OpenThemeData(m_hWnd, L"Button");
		if (hTheme)
		{
		    DrawThemeBackground(hTheme, m_hBackDC, BP_PUSHBUTTON, PBS_NORMAL, &rect, NULL);
		    CloseThemeData(hTheme);
		}
		else
		    DrawFrameControl(m_hBackDC, &rect, DFC_BUTTON, DFCS_FLAT | DFCS_BUTTONPUSH);
		*/
		
		DrawFrameControl(m_hBackDC, &rect, DFC_BUTTON, DFCS_FLAT | DFCS_BUTTONPUSH);
		
		//BitBlt(m_hBackDC, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, 0, 0, BLACKNESS);
		m_pImage->RegisterCallback(OnFrameChanged, (LPARAM)this);
		m_bInited = true;
	}
	
	return 0;
}

LRESULT CAnimatedButton::onErase(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ValidateRect(NULL);
	bHandled = true;
	return 0;
}

LRESULT CAnimatedButton::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
// TODO тут всегда m_hWnd = NULL    dcassert(m_hWnd);
	if (m_pImage)
	{
		if (m_bInited)
		{
			m_pImage->UnregisterCallback(OnFrameChanged, (LPARAM)this);
			m_pImage->DeleteBackDC(m_hBackDC);
		}
		safe_release(m_pImage);
	}
	
	if (m_hDC)
	{
		dcdrun(int l_res =)::ReleaseDC(m_hWnd, m_hDC);
		// TODO dcassert(l_res);
		m_hDC = nullptr;
	}
	
	return 0;
}

void CAnimatedButton::Update()
{
	if (m_hDC)
	{
		m_pImage->Draw(m_hDC,
		               0,
		               0,
		               m_w,
		               m_h,
		               m_xSrc,
		               m_ySrc,
		               m_hBackDC,
		               m_xBk,
		               m_yBk,
		               m_wSrc,
		               m_hSrc);
	}
}

#endif // IRAINMAN_INCLUDE_SMILE