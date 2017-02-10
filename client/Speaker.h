/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 * Copyright (C) 2011-2013 Alexey Solomin, a.rainman on gmail pt com
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

#pragma once


#ifndef DCPLUSPLUS_DCPP_SPEAKER_H
#define DCPLUSPLUS_DCPP_SPEAKER_H

#include <boost/range/algorithm/find.hpp>
#include <utility>
#include <vector>
#include "CFlyThread.h"
#include "noexcept.h"
#include "webrtc/system_wrappers/include/rw_lock_wrapper.h"

template<typename Listener>
class Speaker
{
		typedef std::vector<Listener*> ListenerList;
#ifdef _DEBUG
# define  _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1 // Only critical event debug.
# ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
//#  define  _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2 // Show all events in debug window.
# endif
#endif

		void log_listener_list(const ListenerList& p_list, const char* p_log_message)
		{
			dcdebug("[log_listener_list][%s][tid = %u] [this=%p] count = %d ", p_log_message, ::GetCurrentThreadId(), this, p_list.size());
			for (size_t i = 0; i != p_list.size(); ++i)
			{
				dcdebug("[%u] = %p, ", i, p_list[i]);
			}
			dcdebug("\r\n");
		}
		
	public:
		explicit Speaker() noexcept
		{
		}
		virtual ~Speaker()
		{
			dcassert(m_listeners.empty());
		}
		
#ifdef FLYLINKDC_USE_PROFILER_CS
		template<typename... ArgT>
		void fire_log(const char* p_function, int p_line, ArgT && ... args)
		{
			const string l_f = p_function + string(" Line: ") + Util::toString(p_line);
			CFlyLockLine(m_listenerCS, l_f.c_str());
			ListenerList tmp = m_listeners;
#ifdef _DEBUG
			extern volatile bool g_isShutdown;
			if (g_isShutdown && !tmp.empty())
			{
				log_listener_list(tmp, "fire-destroy!");
			}
#endif
			for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
			{
				(*i)->on(std::forward<ArgT>(args)...);
			}
		}
#define fly_fire(p_type) fire_log(__FUNCTION__,__LINE__,p_type);
#define fly_fire1(p_type,p_arg_1) fire_log(__FUNCTION__,__LINE__,p_type,p_arg_1);
#define fly_fire2(p_type,p_arg_1,p_arg_2) fire_log(__FUNCTION__,__LINE__,p_type,p_arg_1,p_arg_2);
#define fly_fire3(p_type,p_arg_1,p_arg_2,p_arg_3) fire_log(__FUNCTION__,__LINE__,p_type,p_arg_1,p_arg_2,p_arg_3);
#define fly_fire4(p_type,p_arg_1,p_arg_2,p_arg_3,p_arg_4) fire_log(__FUNCTION__,__LINE__,p_type,p_arg_1,p_arg_2,p_arg_3,p_arg_4);
#define fly_fire5(p_type,p_arg_1,p_arg_2,p_arg_3,p_arg_4,p_arg_5) fire_log(__FUNCTION__,__LINE__,p_type,p_arg_1,p_arg_2,p_arg_3,p_arg_4,p_arg_5);
#else
#define fly_fire fire
#define fly_fire1 fire
#define fly_fire2 fire
#define fly_fire3 fire
#define fly_fire4 fire
#define fly_fire5 fire
#endif
#if _MSC_VER > 1600 // > VC++2010
		template<typename... ArgT>
		void fire(ArgT && ... args)
		{
			CFlyLock(m_listenerCS);
			ListenerList tmp = m_listeners;
#ifdef _DEBUG
			extern volatile bool g_isBeforeShutdown;
			if (g_isBeforeShutdown && !tmp.empty())
			{
				log_listener_list(tmp, "fire-before-destroy!");
			}
			
			extern volatile bool g_isShutdown;
			if (g_isShutdown && !tmp.empty())
			{
				log_listener_list(tmp, "fire-destroy!");
			}
#endif
			for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
			{
				(*i)->on(std::forward<ArgT>(args)...);
			}
		}
		
#else // VC++2010
		
#define IRAINMAN_USE_SIMPLE_SPEAKER
		
		template<typename T0>
		void fire(T0 && type) noexcept
		{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
			CFlyLock(m_listenerCS);
			ListenerList tmp = m_listeners;
			for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
#else
			{
				FastSharedLock l(m_listenerCS);
				for (auto i = m_listeners.cbegin(); i != m_listeners.cend(); ++i)
				{
					if (!is_listener_zombie(*i))
					{
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
				(*i)->on(std::forward<T0>(type));
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
		}
}
}
after_fire_process();
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
}
template<typename T0, typename T1>
void fire(T0 && type, T1 && p1) noexcept
{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
	CFlyLock(m_listenerCS);
	ListenerList tmp = m_listeners;
	for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
#else
	{
		FastSharedLock l(m_listenerCS);
		for (auto i = m_listeners.cbegin(); i != m_listeners.cend(); ++i)
		{
			if (!is_listener_zombie(*i))
			{
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
		(*i)->on(std::forward<T0>(type), std::forward<T1>(p1)); // [2] https://www.box.net/shared/da9ee6ddd7ec801b1a86
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
}
}
}
after_fire_process();
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
}
template<typename T0, typename T1, typename T2>
void fire(T0 && type, T1 && p1, T2 && p2) noexcept
{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
	CFlyLock(m_listenerCS);
	ListenerList tmp = m_listeners;
	for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
#else
	{
		FastSharedLock l(m_listenerCS);
		for (auto i = m_listeners.cbegin(); i != m_listeners.cend(); ++i)
		{
			if (!is_listener_zombie(*i))
			{
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
		(*i)->on(std::forward<T0>(type), std::forward<T1>(p1), std::forward<T2>(p2));
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
}
}
}
after_fire_process();
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
}
template<typename T0, typename T1, typename T2, typename T3>
void fire(T0 && type, T1 && p1, T2 && p2, T3 && p3) noexcept
{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
	CFlyLock(m_listenerCS);
	ListenerList tmp = m_listeners;
	for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
#else
	{
		FastSharedLock l(m_listenerCS);
		for (auto i = m_listeners.cbegin(); i != m_listeners.cend(); ++i)
		{
			if (!is_listener_zombie(*i))
			{
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
		(*i)->on(std::forward<T0>(type), std::forward<T1>(p1), std::forward<T2>(p2), std::forward<T3>(p3));
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
}
}
}
after_fire_process();
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
}
template<typename T0, typename T1, typename T2, typename T3, typename T4>
void fire(T0 && type, T1 && p1, T2 && p2, T3 && p3, T4 && p4) noexcept
{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
	CFlyLock(m_listenerCS);
	ListenerList tmp = m_listeners;
	for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
#else
	{
		FastSharedLock l(m_listenerCS);
		for (auto i = m_listeners.cbegin(); i != m_listeners.cend(); ++i)
		{
			if (!is_listener_zombie(*i))
			{
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
		(*i)->on(std::forward<T0>(type), std::forward<T1>(p1), std::forward<T2>(p2), std::forward<T3>(p3), std::forward<T4>(p4));
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
}
}
}
after_fire_process();
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
}
template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
void fire(T0 && type, T1 && p1, T2 && p2, T3 && p3, T4 && p4, T5 && p5) noexcept
{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
	CFlyLock(m_listenerCS);
	ListenerList tmp = m_listeners;
	for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
#else
	{
		FastSharedLock l(m_listenerCS);
		for (auto i = m_listeners.cbegin(); i != m_listeners.cend(); ++i)
		{
			if (!is_listener_zombie(*i))
			{
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
		(*i)->on(std::forward<T0>(type), std::forward<T1>(p1), std::forward<T2>(p2), std::forward<T3>(p3), std::forward<T4>(p4), std::forward<T5>(p5));
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
}
}
}
after_fire_process();
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
}
template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
void fire(T0 && type, T1 && p1, T2 && p2, T3 && p3, T4 && p4, T5 && p5, T6 && p6) noexcept
{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
	CFlyLock(m_listenerCS);
	ListenerList tmp = m_listeners;
	for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
#else
	{
		FastSharedLock l(m_listenerCS);
		for (auto i = m_listeners.cbegin(); i != m_listeners.cend(); ++i)
		{
			if (!is_listener_zombie(*i))
			{
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
		(*i)->on(std::forward<T0>(type), std::forward<T1>(p1), std::forward<T2>(p2), std::forward<T3>(p3), std::forward<T4>(p4), std::forward<T5>(p5), std::forward<T6>(p6));
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
}
}
}
after_fire_process();
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
}
template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
void fire(T0 && type, T1 && p1, T2 && p2, T3 && p3, T4 && p4, T5 && p5, T6 && p6, T7 && p7) noexcept
{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
	CFlyLock(m_listenerCS);
	ListenerList tmp = m_listeners;
	for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
#else
	{
		FastSharedLock l(m_listenerCS);
		for (auto i = m_listeners.cbegin(); i != m_listeners.cend(); ++i)
		{
			if (!is_listener_zombie(*i))
			{
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
		(*i)->on(std::forward<T0>(type), std::forward<T1>(p1), std::forward<T2>(p2), std::forward<T3>(p3), std::forward<T4>(p4), std::forward<T5>(p5), std::forward<T6>(p6), std::forward<T7>(p7));
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
}
}
}
after_fire_process();
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
}
#endif // <= VC++2010
		
		void addListener(Listener* aListener)
		{
			extern volatile bool g_isBeforeShutdown;
			dcassert(!g_isBeforeShutdown);
			CFlyLock(m_listenerCS);
			if (boost::range::find(m_listeners, aListener) == m_listeners.end())
			{
				m_listeners.push_back(aListener);
				m_listeners.shrink_to_fit();
			}
#ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
			else
			{
				dcassert(0);
# ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2
				log_listener_list(m_listeners, "addListener-twice!!!");
# endif
			}
#endif // _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
		}
		
		void removeListener(Listener* aListener)
		{
			CFlyLock(m_listenerCS);
			if (!m_listeners.empty())
			{
				auto it = boost::range::find(m_listeners, aListener);
				if (it != m_listeners.end())
				{
					m_listeners.erase(it);
				}
#ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
				else
				{
					dcassert(0);
# ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2
					log_listener_list(m_listeners, "removeListener-zombie!!!");
# endif
				}
#endif // _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_1
			}
		}
		
		void removeListeners()
		{
			CFlyLock(m_listenerCS);
			m_listeners.clear();
		}
		
	private:
		ListenerList m_listeners;
		CriticalSection m_listenerCS;
};

#endif // !defined(SPEAKER_H)

/**
 * @file
 * $Id: Speaker.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
