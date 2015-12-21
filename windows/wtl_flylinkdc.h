/*
 * Copyright (C) 2012-2015 FlylinkDC++ Team http://flylinkdc.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DCPLUSPLUS_WTL_FLYLINKDC_H
#define DCPLUSPLUS_WTL_FLYLINKDC_H

#include <Shellapi.h>
#include <atlctrlx.h>
#include "../client/w.h"
#include "../client/SettingsManager.h"
#include "../client/ResourceManager.h"
#include "../client/TaskQueue.h"

class CFlyToolBarCtrl : public CToolBarCtrl
{
	private:
		int AddStrings(LPCTSTR lpszStrings);
	public:
		// https ://msdn.microsoft.com/en-us/library/windows/desktop/bb760476%28v=vs.85%29.aspx
		// Zero - based index of the button string, or a pointer to a string buffer that contains text for the button.
		INT_PTR AddStringsSafe(const TCHAR* p_str)
		{
			return (INT_PTR)p_str;
		}
};
template <class T> bool safe_post_message(HWND p_wnd, int p_x, T* p_ptr)
{
	if (::PostMessage(p_wnd, WM_SPEAKER, WPARAM(p_x), LPARAM(p_ptr)) == FALSE)
	{
		delete p_ptr;
// TODO - LOG dcassert(0);
		dcdebug("safe_post_message error %d\n", GetLastError());
		return false;
	}
	return true;
}


#if 0
template <class T> class CFlyFrameInstallerFlag
{
		static uint32_t g_count_instance;
	public:
		CFlyFrameInstallerFlag()
		{
			++g_count_instance;
		}
		~CFlyFrameInstallerFlag()
		{
			--g_count_instance;
		}
};
template <class T> uint32_t CFlyFrameInstallerFlag<T>::g_count_instance;
#endif
class CFlyHandlerAdapter
{
	protected:
		const HWND& m_win_handler;
	public:
		explicit CFlyHandlerAdapter(const HWND& p_hWnd) : m_win_handler(p_hWnd)
		{
		}
};
class CFlySpeakerAdapter : public CFlyHandlerAdapter
{
	public:
		bool m_spoken;
		explicit CFlySpeakerAdapter(const HWND& p_hWnd) : CFlyHandlerAdapter(p_hWnd), m_spoken(false)
		{
		}
		BOOL async_speak()
		{
			extern bool g_isShutdown;
			dcassert(!g_isShutdown);
			BOOL l_res = false;
			ATLASSERT(::IsWindow(m_win_handler));
			if (!m_spoken)
			{
				m_spoken = true;
				l_res = PostMessage(m_win_handler, WM_SPEAKER, 0, 0);
// TODO - LOG               dcassert(l_res);
				if (l_res == 0)
				{
					dcdebug("[async_speak] PostMessage error %d\n", GetLastError());
				}
			}
			return l_res;
		}
		BOOL force_speak() const
		{
			extern bool g_isShutdown;
			dcassert(!g_isShutdown);
			ATLASSERT(::IsWindow(m_win_handler));
			const auto l_res = PostMessage(m_win_handler, WM_SPEAKER, 0, 0);
// TODO - LOG               dcassert(l_res);
			if (l_res == 0)
			{
				dcdebug("[force_speak] PostMessage error %d\n", GetLastError());
			}
			return l_res;
		}
};
class CFlyTaskAdapter : public CFlySpeakerAdapter
{
	protected:
		TaskQueue m_tasks;
	public:
		explicit CFlyTaskAdapter(const HWND& p_hWnd) : CFlySpeakerAdapter(p_hWnd)
		{
		}
		~CFlyTaskAdapter()
		{
			dcassert(m_tasks.empty());
		}
		
	protected:
		virtual void doTimerTask()
		{
			if (!m_tasks.empty())
			{
				async_speak();
			}
		}
		LRESULT onTimerTask(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
		{
			doTimerTask();
			return 0;
		}
		void clear_and_destroy_task()
		{
			m_tasks.destroy_task();
			m_tasks.clear_task();
		}
};

class CFlyTimerAdapter : public CFlyHandlerAdapter
{
		UINT_PTR m_timer_id;
		UINT_PTR m_timer_id_event;
	public:
		explicit CFlyTimerAdapter(const HWND& p_hWnd) : CFlyHandlerAdapter(p_hWnd), m_timer_id(0), m_timer_id_event(NULL)
		{
		}
		virtual ~CFlyTimerAdapter()
		{
			dcassert(!m_timer_id);
		}
	protected:
		UINT_PTR create_timer(UINT p_Elapse, UINT_PTR p_IDEvent = 1)
		{
			m_timer_id_event = p_IDEvent;
			ATLASSERT(::IsWindow(m_win_handler));
			ATLASSERT(m_timer_id == NULL);
			if (m_timer_id == NULL) // ¬ стеке странный рекурсивный вызов SetTimer http://code.google.com/p/flylinkdc/issues/detail?id=1328
			{
				m_timer_id = SetTimer(m_win_handler, p_IDEvent, p_Elapse, NULL);
				ATLASSERT(m_timer_id != NULL);
			}
			return m_timer_id;
		}
		void safe_destroy_timer()
		{
			dcassert(m_timer_id);
			if (m_timer_id)
			{
				ATLASSERT(::IsWindow(m_win_handler));
				BOOL l_res;
				if (m_timer_id_event)
					l_res = KillTimer(m_win_handler, m_timer_id_event);
				else
					l_res = KillTimer(m_win_handler, m_timer_id);
				if (l_res == NULL)
				{
					const auto l_error_code = GetLastError();
					if (l_error_code)
					{
						ATLASSERT(l_res != NULL);
						dcdebug("KillTimer = error_code = %d", l_error_code);
					}
				}
				m_timer_id = 0;
			}
		}
#if 0 // TODO: needs review, see details here https://code.google.com/p/flylinkdc/source/detail?r=15539
		void safe_destroy_timer_if_exists()
		{
			if (m_timer_id)
				safe_destroy_timer();
		}
#endif
};

class CFlyHyperLink : public CHyperLink
#ifdef _DEBUG
	, private boost::noncopyable
#endif
{
#ifdef _DEBUG
		void Attach(_In_opt_ HWND hWndNew) noexcept // запретим дл€ класса звать Attach - ломаетс€.
		{
		}
#endif
	public:
		void init(HWND p_dlg_window, const tstring& p_tool)
		{
			SubclassWindow(p_dlg_window);
			SetHyperLinkExtendedStyle(HLINK_COMMANDBUTTON | HLINK_UNDERLINEHOVER);
			if (BOOLSETTING(WINDOWS_STYLE_URL))
				m_clrLink = GetSysColor(COLOR_HOTLIGHT); // [~] JhaoDa
			ATLVERIFY(m_tip.AddTool(m_hWnd, p_tool.c_str(), &m_rcLink, 1));
		}
};

class CFlyToolTipCtrl : public CToolTipCtrl
#ifdef _DEBUG
	, private boost::noncopyable
#endif
{
	public:
		CFlyToolTipCtrl()
		{
		}
		void AddTool(HWND hWnd, const wchar_t* p_Text = LPSTR_TEXTCALLBACK)
		{
			CToolInfo l_ti(TTF_SUBCLASS, hWnd, 0, nullptr, (LPWSTR) p_Text);
			ATLVERIFY(CToolTipCtrl::AddTool(&l_ti));
		}
		void AddTool(HWND p_hWnd, ResourceManager::Strings p_ID)
		{
			AddTool(p_hWnd, ResourceManager::getStringW(p_ID).c_str());
		}
};

template<bool needsInvalidate = false>
class CLockRedraw
#ifdef _DEBUG
	: private boost::noncopyable
#endif
{
	public:
	explicit CLockRedraw(const HWND p_hWnd) noexcept :
		m_hWnd(p_hWnd)
		{
			ATLASSERT(::IsWindow(m_hWnd));
			::SendMessage(m_hWnd, WM_SETREDRAW, (WPARAM)FALSE, 0);
		}
		~CLockRedraw() noexcept
		{
			ATLASSERT(::IsWindow(m_hWnd));
			::SendMessage(m_hWnd, WM_SETREDRAW, (WPARAM)TRUE, 0);
			if (needsInvalidate)
			{
				::InvalidateRect(m_hWnd, nullptr, TRUE);
			}
		}
	private:
		const HWND m_hWnd;
};
#if 0
class CFlyTimer // TODO пока не используетс€
#ifdef _DEBUG
	: private boost::noncopyable
#endif
{
		TIMERPROC m_f;
		UINT_PTR m_Timer;
		int m_interval;
	public:
		CFlyTimer(TIMERPROC p_f, int p_interval) : m_f(p_f), m_interval(p_interval), m_Timer(NULL)
		{
		}
		void start()
		{
			dcassert(m_Timer == NULL);
			m_Timer = SetTimer(nullptr, 0, m_interval, m_f);
		}
		void stop()
		{
			if (m_Timer)
			{
				KillTimer(nullptr, m_Timer);
				m_Timer = NULL;
			}
		}
		virtual ~CFlyTimer()
		{
			stop();
		}
};
#endif

// copy-paste from wtl\atlwinmisc.h
// (»наче много предупреждений валитс€ warning C4245: 'argument' : conversion from 'int' to 'UINT_PTR', signed/unsigned mismatch )
class CFlyLockWindowUpdate
#ifdef _DEBUG
	: private boost::noncopyable
#endif
{
	public:
		explicit CFlyLockWindowUpdate(HWND hWnd)
		{
			// NOTE: A locked window cannot be moved.
			//       See also Q270624 for problems with layered windows.
			ATLASSERT(::IsWindow(hWnd));
			::LockWindowUpdate(hWnd);
		}
		
		~CFlyLockWindowUpdate()
		{
			::LockWindowUpdate(NULL);
		}
};

#define ATTACH(p_id, p_var) \
	p_var.Attach(GetDlgItem(p_id));

#define ATTACH_AND_SET_TEXT(p_id, p_var, p_txt) \
	{ \
		p_var.Attach(GetDlgItem(p_id)); \
		p_var.SetWindowText(p_txt.c_str()); \
	}

#define GET_TEXT(id, var) \
	{ \
		dcassert(var.c_str()); \
		var.resize(::GetWindowTextLength(GetDlgItem(id))); \
		if (var.size()) GetDlgItemText(id, &var[0], var.size() + 1); \
	}


#endif // DCPLUSPLUS_WTL_FLYLINKDC_H
