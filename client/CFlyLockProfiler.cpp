#include "stdinc.h"
#include "CFlyLockProfiler.h"
#ifdef FLYLINKDC_USE_PROFILER_CS

#include "ClientManager.h"

BoostFastCriticalSection CFlyLockProfiler::g_cs_stat_map;
std::map<string, std::pair<DWORD, DWORD> > CFlyLockProfiler::g_lock_map;
void CFlyLockProfiler::log(const char* p_path, int p_recursion_count, bool p_is_unlock /*= false*/)
{
	//LARGE_INTEGER l_finish;
	//QueryPerformanceCounter(&l_finish);
	//const double l_delta = l_finish.QuadPart - m_start.QuadPart;
	const DWORD l_tick_delta = GetTickCount() - m_start_tick;
	if (p_is_unlock && m_function)
	{
		BoostFastLock l(g_cs_stat_map);
		auto& l_item = g_lock_map[m_function + string(" Line:") + Util::toString(m_line)];
		l_item.first++;
		if (l_tick_delta > l_item.second)
			l_item.second = l_tick_delta;
	}
	if (
#define FLYLINKDC_USE_PROFILE_SELECTED_CLASS
#ifdef FLYLINKDC_USE_PROFILE_SELECTED_CLASS
	    (l_tick_delta >= 50 &&
	     (m_function &&
	      (
	          // true
	          strstr(m_function, "Queue") != 0
	          //|| strstr(m_function, "CFlyServerConfig") != 0
	          
	      )
	     )
	     || p_recursion_count > 1
	    )
	    &&
#endif
	    (m_function &&
	     (
	         strstr(m_function, "DebugManager::") == 0 &&
	         strstr(m_function, "ShareManager::") == 0 &&
	         strstr(m_function, "::addListener") == 0 &&
	         strstr(m_function, "::removeListener") == 0 &&
	         strstr(m_function, "TaskQueue::add") == 0 &&
	         strstr(m_function, "Identity::") == 0 &&
	         strstr(m_function, "LogManager::") == 0
	         
	     )
	    )
	)
	{
		string l_path = p_path;
		if (ClientManager::isShutdown())
		{
			l_path += ".shutdown.txt";
		}
		else if (ClientManager::isStartup())
		{
			l_path += ".startup.txt";
		}
		if (!m_add_log_info.empty())
		{
			l_path += ".extinfo.txt";
		}
		FILE *f = fopen(l_path.c_str(), "ab+");
		if (f)
		{
			char timeFormat[64];
			timeFormat[0] = 0;
			time_t now;
			time(&now);
			tm *now_tm = localtime(&now);
			strftime(timeFormat, _countof(timeFormat), "%d.%m.%Y %H:%M:%S", now_tm);
			fprintf(f, "[%6d][%s] %s [%d] tick_delta = %d RecursionCount = %d ext_info = %s\r\n",
			        ::GetCurrentThreadId(),
			        timeFormat,
			        m_function ? m_function : "",
			        m_line,
			        //l_delta,
			        l_tick_delta,
			        p_recursion_count,
			        m_add_log_info.c_str()
			       );
			if (m_function == nullptr)
			{
				now_tm = nullptr;
			}
			fclose(f);
		}
	}
}
CFlyLockProfiler::~CFlyLockProfiler()
{
}
void CFlyLockProfiler::print_stat()
{
	BoostFastLock l(g_cs_stat_map);
	static std::multimap<DWORD, std::string> l_order_count;
	static std::multimap<DWORD, std::string> l_order_delta;
	FILE *f = fopen("D:\\AllStatCriticalSection.txt", "ab+");
	if (f)
	{
		fprintf(f, "==========================================\r\n\r\n");
		for (auto i = g_lock_map.cbegin(); i != g_lock_map.cend(); ++i)
		{
			fprintf(f, "%s count = %u max tick =%u\r\n", i->first.c_str(), i->second.first, i->second.second);
			l_order_count.insert(std::make_pair(i->second.first, i->first));
			l_order_delta.insert(std::make_pair(i->second.second, i->first));
		}
		fprintf(f, "==========================================\r\n\r\n");
		fprintf(f, "=============     order count   ==========\r\n\r\n");
		fprintf(f, "==========================================\r\n\r\n");
		for (auto j = l_order_count.cbegin(); j != l_order_count.cend(); ++j)
		{
			fprintf(f, "%d name =%s\r\n", j->first, j->second.c_str());
		}
		fprintf(f, "==========================================\r\n\r\n");
		fprintf(f, "=============     order tick   ==========\r\n\r\n");
		fprintf(f, "==========================================\r\n\r\n");
		for (auto j = l_order_delta.cbegin(); j != l_order_delta.cend(); ++j)
		{
			fprintf(f, "%d name =%s\r\n", j->first, j->second.c_str());
		}
		fclose(f);
	}
}
#endif // FLYLINKDC_USE_PROFILER_CS
