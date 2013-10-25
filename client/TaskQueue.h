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

#include "Thread.h"

class Task
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		virtual ~Task() { }
};

class StringTask : public Task
#ifdef _DEBUG
	, virtual NonDerivable<StringTask> // [+] IRainman fix.
#endif
{
	public:
		StringTask(const string& str) : m_str(str) { }
		GETC(string, m_str, Str);
};

class TaskQueue
#ifdef _DEBUG
	: boost::noncopyable, virtual NonDerivable<TaskQueue> // [+] IRainman fix.
#endif
{
	public:
		typedef pair<uint8_t, Task*> Pair;
		typedef vector<Pair> List;
		
		TaskQueue()
		{
		}
		
		virtual ~TaskQueue()
		{
			deleteTasks(tasks);// [!] IRainman fix.
		}
		
		bool empty() const
		{
			return tasks.empty();
		}
#if 0
		size_t size()
		{
			FastLock l(cs);
			return tasks.size();
		}
		string get_debug_info()
		{
			FastLock l(cs);
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
			FastLock l(cs);
			tasks.push_back(make_pair(type, data)); // [2] https://www.box.net/shared/6hnn9eeg42q1qammnlub
		}
		void get(List& list)
		{
			// [!] IRainman fix: FlylinkDC != StrongDC: please be more attentive to the code during the merge.
			FastLock l(cs);
			swap(tasks, list);
		}
		void clear()
		{
			// [!] IRainman fix: FlylinkDC != StrongDC: please be more attentive to the code during the merge.
			List tmp;
			{
				FastLock l(cs);
				swap(tasks, tmp);
			}
			deleteTasks(tmp);
			// [~] IRainman fix
		}
	private:
		void deleteTasks(List& list)// [+] IRainman fix.
		{
			for (auto i = list.cbegin(); i != list.cend(); ++i)
			{
				delete i->second;
			}
		}
		FastCriticalSection cs;
		List tasks;
};

#endif
