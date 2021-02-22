#ifndef DCPLUSPLUS_DCPP_CFLYLOCKPROFILER_H
#define DCPLUSPLUS_DCPP_CFLYLOCKPROFILER_H

#pragma once

#ifdef _DEBUG
//#define FLYLINKDC_USE_PROFILER_CS
#endif

#ifdef FLYLINKDC_USE_PROFILER_CS
#include <ctime>
#include <string>
#include <map>
#include <boost/utility.hpp>
#include <boost/thread.hpp>

typedef boost::detail::spinlock BoostFastCriticalSection;
typedef boost::lock_guard<boost::detail::spinlock> BoostFastLock;

class CFlyLockProfiler
{
	public:
		explicit CFlyLockProfiler(const char* p_function, int p_line) : m_function(p_function), m_line(p_line)
		{
			//QueryPerformanceCounter(&m_start);
			m_start_tick = GetTickCount();
		}
		~CFlyLockProfiler();
		static void print_stat();
		void log(const char* p_path, int p_recursion_count, bool p_is_unlock = false);
	private:
		const char* m_function;
		int m_line;
		//LARGE_INTEGER m_start;
		DWORD m_start_tick;
		static BoostFastCriticalSection g_cs_stat_map;
		static std::map<std::string, std::pair<DWORD, DWORD> > g_lock_map;
	public:
		std::string m_add_log_info;
};
#endif // FLYLINKDC_USE_PROFILER_CS


#endif // DCPLUSPLUS_DCPP_CFLYLOCKPROFILER_H                                 
