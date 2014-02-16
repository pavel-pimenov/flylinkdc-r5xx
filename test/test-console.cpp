
#include "stdafx.h"
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <atlfile.h>
#include <time.h>
#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/asio/ip/address_v4.hpp>

//#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/thread.hpp>
#include <limits>
#include "../client/CFlyProfiler.h"
#include "../client/thread.h"
#include "cperformance.h"
#include "FastAlloc.h"
#include "cycle.h"

FastCriticalSection FastAllocBase::cs;

typedef boost::recursive_mutex	BoostCriticalSection;
typedef boost::detail::spinlock	BoostFastCriticalSection;
typedef boost::lock_guard<boost::recursive_mutex> BoostLock;
typedef boost::lock_guard<boost::detail::spinlock> BoostFastLock;

template<typename T>
void timemap(const int n=10000000, const int r=7) {
	std::map<std::string,std::vector<double> > timings;
	std::cout << std::fixed << std::setprecision(2);

	for (int j=0 ; j<r ; j++) {
		ticks start;
		T hsh;
		int kk = 0;

		start = getticks();
		// Insert every 2nd number in the sequence
		for (int i=0 ; i<n ; i+=2) {
			hsh[i] = i*2;
		}
		timings["1: Insert"].push_back(elapsed(getticks(), start));

		start = getticks();
		// Lookup every number in the sequence
		for (int i=0 ; i<n ; ++i) {
			if(hsh.find(i) != hsh.end())
              ++kk;
		}
		if(kk)
		{
		 timings["2: Lookup"].push_back(elapsed(getticks(), start));
		}

		start = getticks();
		// Iterate over the entries
		for (typename T::iterator it=hsh.begin() ; it != hsh.end() ; ++it) {
			int x = it->second;
			++x;
		}
		timings["3: Iterate"].push_back(elapsed(getticks(), start));

		start = getticks();
		// Erase the entries
		for (int i=0 ; i<n ; i+=2) {
			hsh.erase(i);
		}
		timings["4: Erase"].push_back(elapsed(getticks(), start));
	}

	for (std::map<std::string,std::vector<double> >::iterator it=timings.begin() ; it!=timings.end() ; ++it) {
		double sum = 0.0;
		std::cout << it->first << " ( ";
		for (int i=1 ; i<r ; i++) {
			sum += it->second.at(i);
			std::cout << it->second.at(i) << " ";
		}
		std::cout << ") " << sum/double(r-1) << std::endl;
	}
}


static std::wstring translateError(int aError)
{
	LPTSTR lpMsgBuf = 0;
	DWORD chars = FormatMessage(
	                  FORMAT_MESSAGE_ALLOCATE_BUFFER |
	                  FORMAT_MESSAGE_FROM_SYSTEM |
	                  FORMAT_MESSAGE_IGNORE_INSERTS,
	                  NULL,
	                  aError,
	                  MAKELANGID(LANG_NEUTRAL, SUBLANG_ENGLISH_US), // US
	                  (LPTSTR) &lpMsgBuf,
	                  0,
	                  NULL
	              );
	std::wstring tmp = lpMsgBuf;
	LocalFree(lpMsgBuf);
	return tmp;
}

#define MEMBER_LIST() \
    double m_double;\
    int    m_int;\
	std::string m_str1;\
	std::string m_str2;\
	char        m_char_array[100];\
	std::vector<int> m_vector

struct MyClassFast : FastAlloc<MyClassFast>
{
	MEMBER_LIST();
};
struct MyClass
{
	MEMBER_LIST();
};

// [+] IRainman fix
#define TEST_CODE(code, num)\
	std::cout << "Test " #code " (" #num ") started" << std::endl;\
	for(volatile unsigned int i = 0 /*запрещаем оптимизацию счЄтчика цикла дл€ гарантии одинакового поведени€ начинки цикла*/; i < g_cnt; ++i)\
	{\
		PROFILE_SCOPED();\
		START_PERFORMANCE_CHECK(num);\
		{\
			code;\
		}\
		FINISH_PERFORMANCE_CHECK(num);\
	}\
	std::cout << "Test " #code "(" #num ") done" << std::endl
// [~] IRainman fix

std::string toString(const std::string& p_sep, const std::vector<std::string>& p_lst, bool p_alloc)
{
	std::string ret;
	if(p_alloc)
	{
	 int l_size = 2;
	 for (auto j = p_lst.cbegin(); j != p_lst.cend(); ++j)
		l_size += p_sep.size() + j->size();
	 ret.reserve(l_size);
	}
	for (auto i = p_lst.cbegin(), iend = p_lst.cend(); i != iend; ++i)
	{
		ret += *i;
		if (i + 1 != iend)
			ret += p_sep;
	}
	return ret;
}

#ifdef _DEBUG
unsigned int g_cnt = 10000;
const unsigned int g_size = 1000;
const unsigned int g_ext_cnt = 1000;
#else
unsigned int g_cnt = std::numeric_limits<unsigned int>::max();
const unsigned int g_size = 1000 * 1000;
const unsigned int g_ext_cnt = 1000;
#endif

DECLARE_PERFORMANCE_FILE_STREAM(profiler-swap.log, g_flylinkdc_perf_swap);
DECLARE_PERFORMANCE_CHECKER(1, g_flylinkdc_perf_swap);
DECLARE_PERFORMANCE_CHECKER(2, g_flylinkdc_perf_swap);
DECLARE_PERFORMANCE_CHECKER(3, g_flylinkdc_perf_swap);
DECLARE_PERFORMANCE_CHECKER(4, g_flylinkdc_perf_swap);

DECLARE_PERFORMANCE_FILE_STREAM(profiler-map.log, g_flylinkdc_map_insert);
DECLARE_PERFORMANCE_CHECKER(5, g_flylinkdc_map_insert);
DECLARE_PERFORMANCE_CHECKER(6, g_flylinkdc_map_insert);
DECLARE_PERFORMANCE_CHECKER(7, g_flylinkdc_map_insert);

//DECLARE_PERFORMANCE_CHECKER(5, g_flylinkdc_perf_swap);


//DECLARE_PERFORMANCE_FILE_STREAM(profiler-dir-scan.log, g_dir_scan);

//DECLARE_PERFORMANCE_CHECKER(101, g_dir_scan);
//DECLARE_PERFORMANCE_CHECKER(102, g_dir_scan);
//DECLARE_PERFORMANCE_CHECKER(103, g_dir_scan);
///////////////////////////////////////////
/*
static int g_hash_byte = 0;
static long long g_sum_byte = 0;
static void Process_Files( const boost::filesystem::path &Path, bool recurse )
{
	std::cout << "Folder: " << Path << " [scan=" << g_sum_byte/1024/1024 << " Mb]\n";

	boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end

	for(
	    boost::filesystem::directory_iterator itr( Path );
	    itr != end_itr;
	    ++itr
	)
	{
		if( recurse && boost::filesystem::is_directory( *itr ) )
		{
			// ѕогружаемс€ на 1 уровень вниз по дереву каталогов
			boost::filesystem::path Deeper( *itr );

			Process_Files( Deeper,recurse );
			continue;
		}
		for (int i=1; i<4; ++i)
		{
			switch(i)
			{
			case 1: // atl-map
			{
				START_PERFORMANCE_CHECK(101);
				CAtlFile l_file;
				const std::wstring filename = itr->path().generic_wstring();
				const SIZE_T filesize = boost::filesystem::file_size(filename);
				HRESULT hr = l_file.Create(filename.c_str(), GENERIC_READ,FILE_SHARE_READ, OPEN_EXISTING);
				if( SUCCEEDED(hr) )
				{
					CAtlFileMapping<char> l_filemap;
					hr = l_filemap.MapFile(l_file, 0, 0, PAGE_READONLY, FILE_MAP_READ);
					if( SUCCEEDED(hr) )
					{
						const auto l_size = filesize; //l_filemap.GetMappingSize();
						g_sum_byte += l_size;
						char* l_Mem = l_filemap;
						for(size_t i = 0; i<l_size; ++i)
						{
							g_hash_byte += l_Mem[i];
						}
					}
					else
					{
						std::cout << "Error l_filemap.MapFile: "<< *itr << " GetLastError() = " << ::GetLastError() <<std::endl;
						std::wcout << translateError(::GetLastError());
					}
				}
				else
				{
					std::cout << "Error l_file.Create[MAP]: "<< *itr << " GetLastError() = " << ::GetLastError() << std::endl;
					std::wcout << translateError(::GetLastError());
				}
			FINISH_PERFORMANCE_CHECK(101);
			}
		    break;
			case 2: // boost - map
			{
				START_PERFORMANCE_CHECK(102);
				boost::iostreams::mapped_file_source l_map_file;
				l_map_file.open(itr->path()); // filename.c_str() // , filesize
				if(l_map_file.is_open())
				{
					const auto l_size = l_map_file.size();
					g_sum_byte += l_size;
					const char* l_Mem = l_map_file.data();
					for(size_t i = 0; i<l_size; ++i)
					{
						g_hash_byte += l_Mem[i];
					}
				}
				else
				{
					std::cout << "Error boost::iostreams::mapped_file_source l_map_file: "<< *itr << " GetLastError() = " << ::GetLastError() << std::endl;
				}
			FINISH_PERFORMANCE_CHECK(102);
			}
		    break;
			case 3: // native-read
			{
				START_PERFORMANCE_CHECK(103);
				CAtlFile l_file;
				const std::wstring filename = itr->path().generic_wstring();
				const SIZE_T filesize = boost::filesystem::file_size(filename);
				HRESULT hr = l_file.Create(filename.c_str(), GENERIC_READ,FILE_SHARE_READ, OPEN_EXISTING);
				if( SUCCEEDED(hr) )
				{
					if(filesize)
					{
						std::vector<char> l_buf(filesize);
						hr = l_file.Read(l_buf.data(),filesize);
						if( SUCCEEDED(hr) )
						{
							g_sum_byte += filesize;
							for(size_t i = 0; i < filesize; ++i)
							{
								g_hash_byte += l_buf[i];
							}
						}
						else
						{
							std::cout << "Error l_file.Read: "<< *itr << " GetLastError() = " << ::GetLastError() << std::endl;
							std::wcout << translateError(::GetLastError());
						}
					}
				}
				else
				{
					std::cout << "Error l_file.Create: "<< *itr << " GetLastError() = " << ::GetLastError() << std::endl;
					std::wcout << translateError(::GetLastError());
				}
			FINISH_PERFORMANCE_CHECK(103);
			}
		    break;
			}
		}
//рому содержитс€ в filename, можно обрабатывать.
		//const std::string filename = *itr->;
		// std::cout << *itr << "\n";
	}

	return;
}
*/
///////////////////////////////////////////
std::vector<std::string> g_target_vector;
std::vector<std::string> getSourceVector()
{
	std::vector<std::string> l_vec;
	for(int j=0;j<10000;++j)
	{
		l_vec.push_back("test");
	}
	return l_vec;
}
void test_swap_member_vector()
{
	auto l_vec = getSourceVector();
	TEST_CODE(g_target_vector.swap(l_vec), 1);
}
void test_swap_vector()
{
	auto l_vec = getSourceVector();
	TEST_CODE(swap(g_target_vector,l_vec), 2);
}
void test_replace_vector()
{
	auto l_vec = getSourceVector();
	TEST_CODE(g_target_vector = l_vec, 3);
}

void test_critical_section()
{
	CriticalSection l_cs;
	FastCriticalSection l_fcs;
	BoostCriticalSection l_boost_cs;
	//BoostFastCriticalSection l_boost_fcs;
	int l_count = 0;

	TEST_CODE(l_count++, 1);

	TEST_CODE(Lock l(l_cs); l_count++, 2);

	TEST_CODE(FastLock l(l_fcs); l_count++, 3);

	TEST_CODE(BoostLock l(l_boost_cs); l_count++, 4);

	//TEST_CODE(BoostFastLock l(l_boost_fcs); l_count++, 5); // Ёто пока вешаетс€ - не пон€тно почему

	std::cout << l_count <<std::endl;
}

void testFastAlloc()
{
	DECLARE_PERFORMANCE_FILE_STREAM(profiler-fastalloc.log, g_flylinkdc_perf_fast_alloc);

	DECLARE_PERFORMANCE_CHECKER(1, g_flylinkdc_perf_fast_alloc);
	DECLARE_PERFORMANCE_CHECKER(11, g_flylinkdc_perf_fast_alloc);

	DECLARE_PERFORMANCE_CHECKER(2, g_flylinkdc_perf_fast_alloc);
	DECLARE_PERFORMANCE_CHECKER(21, g_flylinkdc_perf_fast_alloc);
#ifndef _DEBUG
	const auto cnt = g_cnt;
	g_cnt = 5 * 1000 * 1000;
#endif
	{
		std::vector<MyClassFast*> l_array_fast;
		TEST_CODE(l_array_fast.push_back(new MyClassFast()), 1);
		TEST_CODE(delete l_array_fast[i], 11);
	}

	{
		std::vector<MyClass*> l_array;
		TEST_CODE(l_array.push_back(new MyClass()), 2);
		TEST_CODE(delete l_array[i], 21);
	}
#ifndef _DEBUG
	g_cnt = cnt;
#endif
}

// [+] IRainman fix.
void processPreparing(DWORD_PTR affinityMask)
{
	// [!] при запуске многопоточных тестов данную функцию необходимо звать в каждом тестовом потоке!
	std::cout << "Preliminary \"warming\" of the processor the current thread..";
	//::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL); // [?] ставим повышенный приоритет процессу дл€ исключени€ мнимых задержек в системной очереди.

	::SetThreadAffinityMask(::GetCurrentThread(), affinityMask); // устанавливаем используемый процессор (€дро).

	// "√реем" железо дл€ исключени€ возможности работы процессора на пониженных частотах во врем€ теста.
	volatile int heaterCount = std::numeric_limits<int>::min();
	volatile int heater = std::numeric_limits<int>::min();

	for (volatile int i = 0; i != 10; ++i)
	{
		std::cout << ".";
		for (; heater != std::numeric_limits<int>::max(); ++heater)
		{
			++heaterCount;
		}
		std::cout << ".";
		for (; heater != std::numeric_limits<int>::min(); --heater)
		{
			--heaterCount;
		}
	}
	heater = heaterCount;
	std::cout << " Done!" << std::endl;
}
// [+] IRainman fix.

namespace std
{
template<> struct hash<boost::asio::ip::address_v4>
{
	size_t operator()(const boost::asio::ip::address_v4& p_ip) const
	{
		return p_ip.to_ulong();
	}
};
}

static string toString(int val)
		{
			char buf[16];
			snprintf(buf, _countof(buf), "%d", val);
			return buf;
		}

class TestTimer
{
public:
    TestTimer(const std::string & name) : name(name),
        start(boost::date_time::microsec_clock<boost::posix_time::ptime>::local_time())
    {
    }
    ~TestTimer()
    {
        using namespace std;
        using namespace boost;
        posix_time::ptime now(date_time::microsec_clock<posix_time::ptime>::local_time());
        posix_time::time_duration d = now - start;
        cout << name << " completed in " << d.total_milliseconds() / 1000.0 <<
            " seconds   || ";
    }
private:
    std::string name;
    boost::posix_time::ptime start;
};
void UseVector(size_t dimension)
{
    TestTimer t("UseVector");
    std::vector<char> pixels;
    for(size_t i = 0; i < dimension; ++i)
			pixels.push_back(1);
}
void UseVectorResize(size_t dimension)
{
    TestTimer t("UseVectorResize");
    std::vector<char> pixels;
	pixels.resize(dimension);
    for(size_t i = 0; i < dimension; ++i)
			pixels[i] = 1;
}

void UseVectorPushBack(size_t dimension)
{
    TestTimer t("UseVectorPushBack");
        std::vector<char> pixels;  
        pixels.reserve(dimension);
        for(size_t i = 0; i < dimension; ++i)
            pixels.push_back(1);
}
int _tmain(int argc, _TCHAR* argv[])
{
    for (size_t i=0; i < 1*1024*1024*1024; i += 100*1024*1024)
	{
     std::cout << "dimension = " << i/1024/1024 << " Mb: " << "\t";
     UseVector(i);
     UseVectorPushBack(i);
	 UseVectorResize(i);
	 std::cout << std::endl;
	}
    return 0;

		const int max_count = 10000000;
		{
			std::vector<std::string> elements;
			START_PERFORMANCE_CHECK(5);
			for(int i=0; i<max_count;++i)
			{
				elements.push_back(std::string());
			}
			FINISH_PERFORMANCE_CHECK(5);
		}

		{
			std::vector<std::string> elements;
			START_PERFORMANCE_CHECK(6);
			for(int i=0; i<max_count;++i)
			{
				elements.emplace_back(std::string());
			}
			FINISH_PERFORMANCE_CHECK(6);
		}
		return 0;
{
		boost::unordered_map<std::string,double> myrecipe;
		START_PERFORMANCE_CHECK(5);
		for(int i=0; i<max_count;++i)
		{
			myrecipe.insert (myrecipe.end(), std::make_pair<std::string,double>(toString(i),0)); // move insertion + hint
		}
		FINISH_PERFORMANCE_CHECK(5);
}

{
		boost::unordered_map<std::string,double> myrecipe;
	    START_PERFORMANCE_CHECK(6);
		for(int i=0; i<max_count;++i)
		{
			const auto s = toString(i);
			if(myrecipe.find(s) == myrecipe.end())
			{
			 myrecipe.insert (std::make_pair<std::string,double>(toString(i),0)); // move insertion
			}
		}
		FINISH_PERFORMANCE_CHECK(6);
}

{
		boost::unordered_map<std::string,double> myrecipe;
	    START_PERFORMANCE_CHECK(7);
		for(int i=0; i<max_count;++i)
		{
			std::pair<std::string,double> l_val(toString(i),0);
			myrecipe.insert (l_val); // copy insertion
		}
		FINISH_PERFORMANCE_CHECK(7);
}


  return 0;
  //boost::unordered_map<boost::asio::ip::address_v4, int> tmp;
  boost::asio::ip::address_v4 ip = boost::asio::ip::address_v4::from_string("010.010.010.010");
  //tmp[ip] = 1;

	std::cout << "Timing boost::unordered_map<int,int>" << std::endl;
	timemap<boost::unordered_map<int,int> >();
	std::cout << std::endl;

	std::cout << "Timing std::unordered_map<int,int>" << std::endl;
	timemap<std::unordered_map<int,int> >();
	std::cout << std::endl;

	std::cout << "Timing std::map<int,int>" << std::endl;
	timemap<std::map<int,int> >();
	std::cout << std::endl;

	return 0;
	// [+] IRainman fix.
	::SetProcessPriorityBoost(::GetCurrentProcess(), FALSE); // «апрещаем системе измен€ть приоритет процесса.

	processPreparing(1); // [!] ”становите разные €дра дл€ разных потоков при тестировании параллельного кода дл€ снижени€ искажений за счЄт выполнени€ тестов на одном €дре.

	std::cout << "Tests started" << std::endl;
	// [~] IRainman fix.
	
//	Process_Files("G:\\dc-test-issue-956", true);  // boost::filesystem::current_path()
//	std::cout << std::endl << "g_hash_byte = " << g_hash_byte <<   " g_sum_byte = " << g_sum_byte <<  std::endl;
//	return 0;

	test_critical_section();
	//testFastAlloc();
	//test_replace_vector();
	//test_swap_vector();
	//test_swap_member_vector();


#if 0
std::unordered_set<int> l_id;

	for (int i=0; i<g_cnt; ++i)
		  l_id.insert(i);

	DECLARE_PERFORMANCE_CHECKER(1, g_flylinkdc_perf_fast_alloc);
	DECLARE_PERFORMANCE_CHECKER(2, g_flylinkdc_perf_fast_alloc);
	DECLARE_PERFORMANCE_CHECKER(3, g_flylinkdc_perf_fast_alloc);
	
	unsigned int l_sum = 0;
	unsigned int l_sum2 = 0;
	unsigned int l_sum3 = 0;
	for (int i=0; i<g_ext_cnt; i++)
	{
		for (int i=0; i<g_cnt; i++)
		{
			START_PERFORMANCE_CHECK(1);
			if (l_id.count(i) == 1)
				l_sum += 1;
			FINISH_PERFORMANCE_CHECK(1);
		}

		for (int i=0; i<g_cnt; i++)
		{
			START_PERFORMANCE_CHECK(2);
			if (l_id.count(i) != 0)
				l_sum2 += 1;
			FINISH_PERFORMANCE_CHECK(2);
		}

		for (int i=0; i<g_cnt; i++)
		{
			START_PERFORMANCE_CHECK(3);
			if (l_id.find(i) != l_id.end())
				l_sum3 += 1;
			FINISH_PERFORMANCE_CHECK(3);
		}
	}
	std::cout << " sum1 = "<< l_sum << "  sum2 = "<< l_sum2 << "  sum3 = " << l_sum3 << std::endl;
#endif

/*
		time_t l_t  = time(NULL);
		const tm* l_loc = localtime(&l_t);
		const std::string l_msgAnsi = "[%Y-%m-%d %H:%M] %<-FTTBkhv-> Running Verlihub 1.0.0 build Fri Mar 30 2012 ][ Runtime: 5 дн. 15 час. ][ User count: 533";
		std::vector<char> l_buf(l_msgAnsi.size() + 256);
		l_buf[0] = 0;
		const int l_len = strftime(&l_buf[0], l_buf.size()-1, l_msgAnsi.c_str(), l_loc); 
		printf("l_len = %d. buf = %s",l_len, l_buf.data());

		return 0;
*/
/*
struct tm newtime;
	__time64_t long_time;
	// Get time as 64-bit integer.
	_time64 ( &long_time );
	// Convert to local time.
	errno_t err = _localtime64_s ( &newtime, &long_time );
	if ( err )
	{
		printf( "Invalid argument to _localtime64_s." );
		exit ( 1 ); 
	}

		std::string l_msgAnsi = "[%%Y-%%m-%%d %%H:%%M%] %<-FTTBkhv-> Running Verlihub 1.0.0 build Fri Mar 30 2012 ][ Runtime: 5 дн. 15 час. ][ User count: 533";
		size_t bufsize = l_msgAnsi.size() + 256;
		std::vector<char> buf(l_msgAnsi.size() + 256);
		buf[0] = 0;
		int l_len = strftime(&buf[0], bufsize, l_msgAnsi.c_str(), &newtime); 
      printf("l_len = %d. buf = %s",l_len,buf.data());
		return 0;
*/

#if 0
	//======================================================
	{
	std::vector<bool> l_bool_vector;
	l_bool_vector.resize(l_size);
	/*
	DECLARE_PERFORMANCE_CHECKER(1, g_flylinkdc_perf_fast_alloc);
	for (int i=0;i<l_cnt; i++)
	{
		START_PERFORMANCE_CHECK(1);
		l_bool_vector.clear();
		l_bool_vector.resize(l_size);
		FINISH_PERFORMANCE_CHECK(1);
	}
	DECLARE_PERFORMANCE_CHECKER(2, g_flylinkdc_perf_fast_alloc);
	//l_bool_vector.resize(l_size);
	for (int i=0;i<l_cnt; i++)
	{
		START_PERFORMANCE_CHECK(2);
		l_bool_vector.resize(l_size);
        std::fill(l_bool_vector.begin(), l_bool_vector.end(), false);
		FINISH_PERFORMANCE_CHECK(2);
	}
	DECLARE_PERFORMANCE_CHECKER(3, g_flylinkdc_perf_fast_alloc);
	for (int i=0;i<l_cnt; i++)
	{
		START_PERFORMANCE_CHECK(3);
		l_bool_vector.resize(l_size);
        std::fill_n(l_bool_vector.begin(), l_size, false);
		FINISH_PERFORMANCE_CHECK(3);
	}
	*/
	std::vector<std::string> l_test;
	l_test.push_back("UserIP2"); 
	l_test.push_back("NoGetINFO"); 
	l_test.push_back("NoGetINFO"); 
	l_test.push_back("NoGetINFO"); 
	l_test.push_back("NoHello"); 
	DECLARE_PERFORMANCE_CHECKER(4, g_flylinkdc_perf_fast_alloc);
	for (int i=0;i<l_cnt; i++)
	{
		START_PERFORMANCE_CHECK(4);
		const std::string l_res = toString(" ", l_test, true);
		FINISH_PERFORMANCE_CHECK(4);
	}
	DECLARE_PERFORMANCE_CHECKER(5, g_flylinkdc_perf_fast_alloc);
	for (int i=0;i<l_cnt; i++)
	{
		START_PERFORMANCE_CHECK(5);
		const std::string l_res = toString(" ", l_test, false);
		FINISH_PERFORMANCE_CHECK(5);
	}


/*
3-тий шутрее немного
Instruction number = 3
Hole time = 16.0565
Call count = 100000
Average time = 0.000160565
===============================================
Instruction number = 2
Hole time = 18.2646
Call count = 100000
Average time = 0.000182646
===============================================
Instruction number = 1
Hole time = 25.5018
Call count = 100000
Average time = 0.000255018
===============================================
*/
	}
#endif


#if defined(__PROFILER_ENABLED__)
	Profiler::dumphtml();
#endif

	std::cout << "Tests ends" << std::endl;
	return 0;
}


