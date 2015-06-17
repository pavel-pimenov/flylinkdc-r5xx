/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_DCPP_TASK_H
#define DCPLUSPLUS_DCPP_TASK_H

#include "CFlyThread.h"

class Task
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		virtual ~Task() { }
};

class StringTask : public Task
{
	public:
		explicit StringTask(const string& p_str) : m_str(p_str) { }
		string m_str;
};

class TaskQueue
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		typedef std::vector<std::pair<uint8_t, Task*> > List;
		
		TaskQueue()
#ifdef _DEBUG
			: m_destroy_guard(0)
#endif
		{
		}
		
		virtual ~TaskQueue()
		{
			deleteTasks(m_tasks);// [!] IRainman fix.
		}
		
		bool empty()
		{
			FastLock l(m_csTaskQueue);
			return m_tasks.empty();
		}
#if 0
		size_t size()
		{
			FastLock l(m_csTaskQueue);
			return tasks.size();
		}
		string get_debug_info()
		{
			FastLock l(m_csTaskQueue);
			string l_tmp;
			for (auto i = tasks.cbegin(); i != tasks.cend(); ++i)
			{
				l_tmp += "," + Util::toString(i->first);
			}
			return l_tmp;
		}
#endif
		void add(uint8_t type, Task* data)
		{
			FastLock l(m_csTaskQueue);
			dcassert(m_destroy_guard == 0);
			m_tasks.push_back(std::make_pair(type, data)); // [2] https://www.box.net/shared/6hnn9eeg42q1qammnlub
		}
		void get(List& p_list)
		{
			// [!] IRainman fix: FlylinkDC != StrongDC: please be more attentive to the code during the merge.
			FastLock l(m_csTaskQueue);
			swap(m_tasks, p_list);
		}
		void clear()
		{
			// [!] IRainman fix: FlylinkDC != StrongDC: please be more attentive to the code during the merge.
			List l_tmp;
			get(l_tmp);
			deleteTasks(l_tmp);
			// [~] IRainman fix
		}
		void clear_task()
		{
			dcassert(m_destroy_guard++ == 0);
			clear();
		}
	private:
		void deleteTasks(const List& p_list)// [+] IRainman fix.
		{
			for (auto i = p_list.cbegin(); i != p_list.cend(); ++i)
			{
				delete i->second;
			}
		}
		FastCriticalSection m_csTaskQueue;
		List m_tasks;
#ifdef _DEBUG
		int m_destroy_guard;
#endif
};

#endif
