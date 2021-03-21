/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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


#ifndef DCPLUSPLUS_DCPP_TASK_H
#define DCPLUSPLUS_DCPP_TASK_H

#include "CFlyThread.h"

class Task
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
class StringArrayTask : public Task
{
	public:
		explicit StringArrayTask(const StringList& p_str) : m_str_array(p_str) { }
		StringList m_str_array;
};

class TaskQueue
{
	public:
		typedef std::vector<std::pair<uint8_t, Task*> > List;
		
		TaskQueue() : m_destroy_guard(0), m_lock_count(0)
		{
		}
		
		virtual ~TaskQueue()
		{
			deleteTasks(m_tasks);
		}
		bool is_destroy_task()
		{
			CFlyFastLock(m_csTaskQueue);
			return m_destroy_guard > 0;
		}
		bool is_lock_task()
		{
			CFlyFastLock(m_csTaskQueue);
			return m_lock_count > 0;
		}
		bool empty()
		{
			// CFlyFastLock(m_csTaskQueue);
			// Убрал лок - т.к. даже если пропустим задачу след раз поймаем
			// с ней слишком много блокировок бегает в клиенте
			return m_tasks.empty();
		}
#if 0
		size_t size()
		{
			CFlyFastLock(m_csTaskQueue);
			return tasks.size();
		}
		string get_debug_info()
		{
			CFlyFastLock(m_csTaskQueue);
			string l_tmp;
			for (auto i = tasks.cbegin(); i != tasks.cend(); ++i)
			{
				l_tmp += "," + Util::toString(i->first);
			}
			return l_tmp;
		}
#endif
		bool add_safe(uint8_t type, Task* data)
		{
			if (data)
			{
				return add(type, data);
			}
			return false;
		}
		bool add(uint8_t type, Task* data)
		{
			CFlyFastLock(m_csTaskQueue);
			dcassert(m_destroy_guard == 0);
			dcassert(m_lock_count == 0)
			if (m_destroy_guard == 0 && m_lock_count == 0)
			{
				m_tasks.push_back(std::make_pair(type, data)); 
				return true;
			}
			else
			{
				delete data;
				return false;
			}
		}
		void get(List& p_list)
		{
			CFlyFastLock(m_csTaskQueue);
			swap(m_tasks, p_list);
		}
		void clear()
		{
			List l_tmp;
			get(l_tmp);
			deleteTasks(l_tmp);
		}
		void lock_task()
		{
			CFlyFastLock(m_csTaskQueue);
			m_lock_count++;
		}
		void unlock_task()
		{
			CFlyFastLock(m_csTaskQueue);
			m_lock_count--;
		}
		void destroy_task()
		{
			CFlyFastLock(m_csTaskQueue);
			dcassert(m_destroy_guard == 0);
			++m_destroy_guard;
		}
		void clear_task()
		{
			clear();
		}
	private:
		void deleteTasks(const List& p_list)
		{
			for (auto i = p_list.cbegin(); i != p_list.cend(); ++i)
			{
				delete i->second;
			}
		}
		FastCriticalSection m_csTaskQueue;
		List m_tasks;
		unsigned m_destroy_guard;
		unsigned m_lock_count;
};

#endif
