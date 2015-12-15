#ifndef DCPLUSPLUS_DCPP_CFLYLOCKPROFILER_H
#define DCPLUSPLUS_DCPP_CFLYLOCKPROFILER_H

#pragma once

#ifdef _DEBUG
 #if _MSC_VER > 1600 // VC++ 2010
   #define FLYLINKDC_USE_PROFILER_CS
 #endif
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
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		explicit CFlyLockProfiler(const char* p_function = nullptr) : m_function(p_function)
		{
			//QueryPerformanceCounter(&m_start);
			m_start_tick = GetTickCount();
		}
		static void print_stat();
	protected:
		void log(const char* p_path, int p_recursion_count, bool p_is_unlock = false);
	private:
		const char* m_function;
		//LARGE_INTEGER m_start;
		DWORD m_start_tick;
		static BoostFastCriticalSection g_cs_stat_map;
		static std::map<std::string, std::pair<DWORD, DWORD> > g_lock_map;
	public:
		std::string m_add_log_info;
};
#endif // FLYLINKDC_USE_PROFILER_CS


#endif // DCPLUSPLUS_DCPP_CFLYLOCKPROFILER_H                                 
