/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_DCPP_SPEAKER_H
#define DCPLUSPLUS_DCPP_SPEAKER_H

#include <boost/range/algorithm/find.hpp>
#include <utility>
#include <vector>
#include "Thread.h"
#include "noexcept.h"
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

#ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2
		void log_listener_list(const ListenerList& p_list, const char* p_log_message)
		{
			dcdebug("[log_listener_list][%s][tid = %u] [this=%p] count = %d ", p_log_message, GetSelfThreadID(), this, p_list.size());
			for (size_t i = 0; i != p_list.size(); ++i)
			{
				dcdebug("[%u] = %p, ", i, p_list[i]);
			}
			dcdebug("\r\n");
		}
#endif // _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2
	public:
		explicit Speaker() noexcept
		{
		}
		virtual ~Speaker()
		{
			dcassert(m_listeners.empty());
#ifndef IRAINMAN_USE_SIMPLE_SPEAKER
			dcassert(listeners_to_delayed_remove.empty());
			dcassert(listeners_to_delayed_add.empty());
#endif // IRAINMAN_USE_SIMPLE_SPEAKER
		}
		
		/// @todo simplify when we have variadic templates
		
		template<typename T0>
		void fire(T0 && type) noexcept
		{
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
			Lock l(m_listenerCS);
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
	Lock l(m_listenerCS);
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
	Lock l(m_listenerCS);
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
		(*i)->on(std::forward<T0>(type), std::forward<T1>(p1), std::forward<T2>(p2)); // Venturi Firewall 2012-04-23_22-28-18_A6JRQEPFW5263A7S7ZOBOAJGFCMET3YJCUYOVCQ_0E0D7D71_crash-stack-r501-build-9812.dmp.bz2
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
	Lock l(m_listenerCS);
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
	Lock l(m_listenerCS);
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
	Lock l(m_listenerCS);
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
	Lock l(m_listenerCS);
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
	Lock l(m_listenerCS);
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
#ifdef IRAINMAN_USE_SIMPLE_SPEAKER
void addListener(Listener* aListener)
{
	Lock l(m_listenerCS);
	if (boost::range::find(m_listeners, aListener) == m_listeners.end())
	{
		m_listeners.push_back(aListener);
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
	Lock l(m_listenerCS);
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
	Lock l(m_listenerCS);
	m_listeners.clear();
}

private:
ListenerList m_listeners;
CriticalSection m_listenerCS;

#else // IRAINMAN_USE_SIMPLE_SPEAKER

void addListener(Listener* p_Listener)
{
	{
		TryFastUniqueLock l(m_listenerCS);
		if (l.locked())
		{
			internal_addL(p_Listener);
			return;
		}
	}
	FastLock l(delayed_eventCS);
	listeners_to_delayed_add.push_back(p_Listener);
}

void removeListener(Listener* p_Listener)
{
	{
		TryFastUniqueLock l(m_listenerCS);
		if (l.locked())
		{
			internal_removeL(p_Listener);
			return;
		}
	}
	FastLock l(delayed_eventCS);
	listeners_to_delayed_remove.push_back(p_Listener);
}

void removeListeners()
{
	{
		TryFastUniqueLock l(m_listenerCS);
		if (l.locked())
		{
#ifdef _DEBUG_SPEAKER_LISTENER_LIST_LEVEL_2
			log_listener_list(m_listeners, "removeListenerAll");
#endif
			m_listeners.clear();

			FastLock l(delayed_eventCS);

			dcassert(listeners_to_delayed_add.empty());
			listeners_to_delayed_add.clear();

			dcassert(listeners_to_delayed_remove.empty());
			listeners_to_delayed_remove.clear();

			return;
		}

	}
	FastLock l(delayed_eventCS);
	listeners_to_delayed_remove.insert(listeners_to_delayed_remove.end(), m_listeners.begin(), m_listeners.end());
}

private:
void after_fire_process()
{
	FastLock l(delayed_eventCS);
	if (!listeners_to_delayed_add.empty())
	{
		{
			FastUniqueLock l(m_listenerCS);
			for (auto i = listeners_to_delayed_add.cbegin(); i != listeners_to_delayed_add.cend(); ++i)
				internal_addL(*i);
		}
		listeners_to_delayed_add.clear();
	}
	if (!listeners_to_delayed_remove.empty())
	{
		{
			FastUniqueLock l(m_listenerCS);
			for (auto i = listeners_to_delayed_remove.cbegin(); i != listeners_to_delayed_remove.cend(); ++i)
				internal_removeL(*i);
		}
		listeners_to_delayed_remove.clear();
	}
}

void internal_addL(Listener* listener_to_add)
{
	if (boost::range::find(m_listeners, listener_to_add) == m_listeners.cend())
	{
		m_listeners.push_back(listener_to_add);
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

bool is_listener_zombie(Listener* p_listener)
{
	FastLock l(delayed_eventCS);
	const auto it = boost::range::find(listeners_to_delayed_remove, p_listener);
	if (it != listeners_to_delayed_remove.cend())
	{
		return true; //
	}
	else
	{
		return false;
	}
}

void internal_removeL(Listener* listener_to_remove)
{
	const auto it = boost::range::find(m_listeners, listener_to_remove);
	if (it != m_listeners.cend())
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

ListenerList m_listeners;
FastSharedCriticalSection m_listenerCS;

ListenerList listeners_to_delayed_remove;
ListenerList listeners_to_delayed_add;
FastCriticalSection delayed_eventCS;

#endif // IRAINMAN_USE_SIMPLE_SPEAKER

};

#endif // !defined(SPEAKER_H)

/**
 * @file
 * $Id: Speaker.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
