#include "stdafx.h"

#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION

#include "Resource.h"
#include "CDMDebugFrame.h"
#include "../client/File.h"
#include "../client/CFlylinkDBManager.h"

#define MAX_TEXT_LEN 131072

LRESULT CDMDebugFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlCMDPad.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_READONLY, WS_EX_CLIENTEDGE);
	ctrlCMDPad.LimitText(0);
	ctrlCMDPad.SetFont(Fonts::g_font);
	
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	statusContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	ctrlClear.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON, 0, IDC_CLEAR);
	ctrlClear.SetWindowText(CTSTRING(CDM_CLEAR));
	ctrlClear.SetFont(Fonts::g_systemFont);
	clearContainer.SubclassWindow(ctrlClear.m_hWnd);
	
	CFlyRegistryMap l_store_values;
	CFlylinkDBManager::getInstance()->load_registry(l_store_values, e_CMDDebugFilterState);
	m_showHubCommands = l_store_values["m_showHubCommands"];
	
	ctrlHubCommands.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(CDM_HUB_COMMANDS), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_STATICEDGE);
	ctrlHubCommands.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlHubCommands.SetFont(Fonts::g_systemFont);
	ctrlHubCommands.SetCheck(m_showHubCommands ? BST_CHECKED : BST_UNCHECKED);
	HubCommandContainer.SubclassWindow(ctrlHubCommands.m_hWnd);
	
	m_showCommands = l_store_values["m_showCommands"];
	ctrlCommands.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(CDM_CLIENT_COMMANDS), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_STATICEDGE);
	ctrlCommands.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlCommands.SetFont(Fonts::g_systemFont);
	ctrlCommands.SetCheck(m_showCommands ? BST_CHECKED : BST_UNCHECKED);
	commandContainer.SubclassWindow(ctrlCommands.m_hWnd);
	
	m_showDetection = l_store_values["m_showDetection"];
	ctrlDetection.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(CDM_DETECTION), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_STATICEDGE);
	ctrlDetection.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlDetection.SetFont(Fonts::g_systemFont);
	ctrlDetection.SetCheck(m_showDetection ? BST_CHECKED : BST_UNCHECKED);
	detectionContainer.SubclassWindow(ctrlDetection.m_hWnd);
	
	m_bFilterIp = l_store_values["m_bFilterIp"];
	ctrlFilterIp.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(CDM_FILTER), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_STATICEDGE);
	ctrlFilterIp.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlFilterIp.SetFont(Fonts::g_systemFont);
	ctrlFilterIp.SetCheck(m_bFilterIp ? BST_CHECKED : BST_UNCHECKED);
	cFilterContainer.SubclassWindow(ctrlFilterIp.m_hWnd);
	// add ES_AUTOHSCROLL - fix http://code.google.com/p/flylinkdc/issues/detail?id=1249
	ctrlIPFilter.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ES_NOHIDESEL | ES_AUTOHSCROLL, WS_EX_STATICEDGE, IDC_DEBUG_IP_FILTER_TEXT);
	ctrlIPFilter.SetLimitText(22); // для IP+Port
	ctrlIPFilter.SetFont(Fonts::g_font);
	eFilterContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	m_ctrlIncludeFilter.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ES_NOHIDESEL | ES_AUTOHSCROLL, WS_EX_STATICEDGE, IDC_DEBUG_INCLUDE_FILTER_TEXT);
	m_ctrlIncludeFilter.SetLimitText(100);
	m_ctrlIncludeFilter.SetFont(Fonts::g_font);
	m_eIncludeFilterContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	m_ctrlExcludeFilter.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ES_NOHIDESEL | ES_AUTOHSCROLL, WS_EX_STATICEDGE, IDC_DEBUG_EXCLUDE_FILTER_TEXT);
	m_ctrlExcludeFilter.SetLimitText(100);
	m_ctrlExcludeFilter.SetFont(Fonts::g_font);
	m_eExcludeFilterContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	m_hWndClient    = ctrlCMDPad;
	m_hMenu         = WinUtil::g_mainMenu;
	
	start(64);
	DebugManager::newInstance(); // [+] IRainman opt.
	DebugManager::getInstance()->addListener(this);
	
	ctrlIPFilter.SetWindowText(l_store_values["m_sFilterIp"].toT().c_str());
	m_ctrlIncludeFilter.SetWindowText(l_store_values["m_sFilterInclude"].toT().c_str());
	m_ctrlExcludeFilter.SetWindowText(l_store_values["m_sFilterExclude"].toT().c_str());
	
	bHandled = FALSE;
	DebugManager::g_isCMDDebug = true;
	return 1;
}

LRESULT CDMDebugFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	DebugManager::g_isCMDDebug = false;
	if (!m_closed)
	{
		m_closed = true;
		{
			CFlyRegistryMap l_values;
			if (!m_sFilterIp.empty())
				l_values["m_sFilterIp"] = m_sFilterIp;
			if (!m_sFilterInclude.empty())
				l_values["m_sFilterInclude"] = m_sFilterInclude;
			if (!m_sFilterExclude.empty())
				l_values["m_sFilterExclude"] = m_sFilterExclude;
			if (m_showCommands)
				l_values["m_showCommands"] = CFlyRegistryValue(m_showCommands);
			if (m_showHubCommands)
				l_values["m_showHubCommands"] = CFlyRegistryValue(m_showHubCommands);
			if (m_showDetection)
				l_values["m_showDetection"] = CFlyRegistryValue(m_showDetection);
			if (m_bFilterIp)
				l_values["m_bFilterIp"] = CFlyRegistryValue(m_bFilterIp);
			CFlylinkDBManager::getInstance()->save_registry(l_values, e_CMDDebugFilterState, true);
		}
		
		DebugManager::getInstance()->removeListener(this);
		DebugManager::deleteInstance(); // [+] IRainman opt.
		
		m_stop = true;
		m_sem.signal();
		
		PostMessage(WM_CLOSE);
	}
	else
	{
		bHandled = FALSE;
	}
	return 0;
}

void CDMDebugFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect = { 0 };
	GetClientRect(&rect);
	
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[9];
		ctrlStatus.GetClientRect(sr);
		w[0] = 64;
		w[1] = w[0] + 100;
		w[2] = w[1] + 100;
		w[3] = w[2] + 80;
		w[4] = w[3] + 150;
		w[5] = w[4] + 100;
		w[6] = w[5] + 100;
		w[7] = w[6] + 100;
		w[8] = sr.Width() - 4;
		ctrlStatus.SetParts(9, w);
		
		ctrlStatus.GetRect(0, sr);
		ctrlClear.MoveWindow(sr);
		ctrlStatus.GetRect(1, sr);
		ctrlCommands.MoveWindow(sr);
		ctrlStatus.GetRect(2, sr);
		ctrlHubCommands.MoveWindow(sr);
		ctrlStatus.GetRect(3, sr);
		ctrlDetection.MoveWindow(sr);
		ctrlStatus.GetRect(4, sr); //-V112
		ctrlFilterIp.MoveWindow(sr);
		ctrlStatus.GetRect(5, sr);
		ctrlIPFilter.MoveWindow(sr);
		ctrlStatus.GetRect(6, sr);
		m_ctrlIncludeFilter.MoveWindow(sr);
		ctrlStatus.GetRect(7, sr);
		m_ctrlExcludeFilter.MoveWindow(sr);
		
		tstring msg;
		if (m_bFilterIp)
		{
			if (!m_sFilterIp.empty())
				msg += _T(" IP:Port = ") + Text::toT(m_sFilterIp); // TODO - ругаться если Ip и порт не указан. не найдет иначе.
			if (!m_sFilterInclude.empty())
				msg += _T(" Include: ") + Text::toT(m_sFilterInclude);
			if (!m_sFilterExclude.empty())
				msg += _T(",Exclude: ") + Text::toT(m_sFilterExclude);
		}
		else
		{
			msg = TSTRING(CDM_WATCHING_ALL);
		}
		ctrlIPFilter.EnableWindow(m_bFilterIp);
		m_ctrlIncludeFilter.EnableWindow(m_bFilterIp);
		m_ctrlExcludeFilter.EnableWindow(m_bFilterIp);
		ctrlStatus.SetText(8, msg.c_str());
	}
	
	// resize client window
	if (m_hWndClient)
		::SetWindowPos(m_hWndClient, NULL, rect.left, rect.top,
		               rect.right - rect.left, rect.bottom - rect.top,
		               SWP_NOZORDER | SWP_NOACTIVATE);
}

void CDMDebugFrame::addLine(const DebugTask& task)
{
	dcassert(!isClosedOrShutdown());
	if (!isClosedOrShutdown() && !m_stop)
	{
		if (ctrlCMDPad.GetWindowTextLength() > MAX_TEXT_LEN)
		{
			CLockRedraw<> l_lock_draw(ctrlCMDPad);
			ctrlCMDPad.SetSel(0, ctrlCMDPad.LineIndex(ctrlCMDPad.LineFromChar(2000)));
			ctrlCMDPad.ReplaceSel(_T(""));
		}
		BOOL noscroll = TRUE;
		POINT p = ctrlCMDPad.PosFromChar(ctrlCMDPad.GetWindowTextLength() - 1);
		CRect r;
		if (m_stop)
			return; // Костыль-1
		ctrlCMDPad.GetClientRect(r);
		
		if (r.PtInRect(p) || MDIGetActive() != m_hWnd)
		{
			noscroll = FALSE;
		}
		else
		{
			if (m_stop)
				return; // Костыль-1
			ctrlCMDPad.SetRedraw(FALSE); // Strange!! This disables the scrolling...????
		}
		
		if (m_stop)
			return; // Костыль-1
		auto l_message = DebugTask::format(task);
		LogManager::cmd_debug_message(l_message);
		l_message += "\r\n";
		if (m_stop)
			return; // Костыль-1
		ctrlCMDPad.AppendText(Text::toT(l_message).c_str()); // [!] IRainman fix.
		if (noscroll && !m_stop)
		{
			ctrlCMDPad.SetRedraw(TRUE);
		}
	}
}

LRESULT CDMDebugFrame::onClear(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlCMDPad.SetWindowText(_T(""));
	ctrlCMDPad.SetFocus();
	return 0;
}
LRESULT CDMDebugFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND hWnd = (HWND)lParam;
	const HDC hDC = (HDC)wParam;
	if (hWnd == ctrlCMDPad.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	bHandled = FALSE;
	return FALSE;
};
LRESULT CDMDebugFrame::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring tmp;
	switch (wID)
	{
		case IDC_DEBUG_IP_FILTER_TEXT:
			WinUtil::GetWindowText(tmp, ctrlIPFilter);
			m_sFilterIp = Text::fromT(tmp);
			m_IPTokens = StringTokenizer<string>(m_sFilterIp, ',');
			break;
		case IDC_DEBUG_INCLUDE_FILTER_TEXT:
			WinUtil::GetWindowText(tmp, m_ctrlIncludeFilter);
			m_sFilterInclude = Text::fromT(tmp);
			m_IncludeTokens = StringTokenizer<string>(m_sFilterInclude, ',');
			break;
		case IDC_DEBUG_EXCLUDE_FILTER_TEXT:
			WinUtil::GetWindowText(tmp, m_ctrlExcludeFilter);
			m_sFilterExclude = Text::fromT(tmp);
			m_ExcludeTokens = StringTokenizer<string>(m_sFilterExclude, ',');
			break;
		default:
			dcassert(0);
	}
	UpdateLayout();
	return 0;
}
int CDMDebugFrame::run()
{
	setThreadPriority(Thread::LOW);
	// [!] IRainman.
	DebugTask task;
	
	while (true)
	{
		m_sem.wait();
		if (m_stop || ClientManager::isShutdown())
		{
			break;
		}
		{
			FastLock l(m_cs);
			dcassert(!m_cmdList.empty()); // [~]
			
			std::swap(task, m_cmdList.front()); // [!] opt: use swap.
			m_cmdList.pop_front();
		}
		
		addLine(task);
	}
	// [~] IRainman.
	return 0;
}
void CDMDebugFrame::on(DebugManagerListener::DebugEvent, const DebugTask& task) noexcept
{
	switch (task.m_type)
	{
		case DebugTask::HUB_IN:
		case DebugTask::HUB_OUT:
			if (!m_showHubCommands)
				return;
			if (m_bFilterIp && !m_sFilterIp.empty() && !m_IPTokens.is_contains(task.m_ip_and_port))
				return;
			break;
		case DebugTask::CLIENT_IN:
		case DebugTask::CLIENT_OUT:
			if (!m_showCommands)
				return;
			if (m_bFilterIp && !m_sFilterIp.empty() && !m_IPTokens.is_contains(task.m_ip_and_port))
				return;
			break;
		case DebugTask::DETECTION:
			if (!m_showDetection)
				return;
			break;
#ifdef _DEBUG
		default:
			dcassert(0);
			return;
#endif
	}
	// http://code.google.com/p/flylinkdc/issues/detail?id=419
	if (m_bFilterIp && !m_sFilterInclude.empty())
	{
		if (m_IncludeTokens.is_find2(task.m_ip_and_port, task.m_message) == false)
			return;
	}
	if (m_bFilterIp && !m_sFilterExclude.empty())
	{
		if (m_ExcludeTokens.is_find2(task.m_ip_and_port, task.m_message) == true)
			return;
	}
	addCmd(task);
}

#endif // IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION