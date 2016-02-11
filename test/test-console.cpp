
#include "stdafx.h"
#include "stdinc.h"
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <atlfile.h>
#include <unordered_map>
#include <time.h>

#include <boost/algorithm/string.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/asio/ip/address_v4.hpp>

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <limits>
#include "../client/CFlyProfiler.h"
#include "../client/CFlyThread.h"
#include "cperformance.h"
#include "FastAlloc.h"
#include "cycle.h"

#include<winsock2.h>
#include<Iphlpapi.h>
#include<stdio.h>

#include "zmq.h"

#pragma comment(lib,"Iphlpapi.lib")

int getmac();
void get_adapters();

bool g_UseCSRecursionLog = false;
FastCriticalSection FastAllocBase::cs;

using std::string;

typedef boost::recursive_mutex  BoostCriticalSection;
typedef boost::detail::spinlock BoostFastCriticalSection;
typedef boost::lock_guard<boost::recursive_mutex> BoostLock;
typedef boost::lock_guard<boost::detail::spinlock> BoostFastLock;

template<typename T>
void timemap(const int n = 10000000, const int r = 7)
{
	std::map<std::string, std::vector<double> > timings;
	std::cout << std::fixed << std::setprecision(2);
	
	for (int j = 0 ; j < r ; j++)
	{
		ticks start;
		T hsh;
		int kk = 0;
		
		start = getticks();
		// Insert every 2nd number in the sequence
		for (int i = 0 ; i < n ; i += 2)
		{
			hsh[i] = i * 2;
		}
		timings["1: Insert"].push_back(elapsed(getticks(), start));
		
		start = getticks();
		// Lookup every number in the sequence
		for (int i = 0 ; i < n ; ++i)
		{
			if (hsh.find(i) != hsh.end())
				++kk;
		}
		if (kk)
		{
			timings["2: Lookup"].push_back(elapsed(getticks(), start));
		}
		
		start = getticks();
		// Iterate over the entries
		for (typename T::iterator it = hsh.begin() ; it != hsh.end() ; ++it)
		{
			int x = it->second;
			++x;
		}
		timings["3: Iterate"].push_back(elapsed(getticks(), start));
		
		start = getticks();
		// Erase the entries
		for (int i = 0 ; i < n ; i += 2)
		{
			hsh.erase(i);
		}
		timings["4: Erase"].push_back(elapsed(getticks(), start));
	}
	
	for (std::map<std::string, std::vector<double> >::iterator it = timings.begin() ; it != timings.end() ; ++it)
	{
		double sum = 0.0;
		std::cout << it->first << " ( ";
		for (int i = 1 ; i < r ; i++)
		{
			sum += it->second.at(i);
			std::cout << it->second.at(i) << " ";
		}
		std::cout << ") " << sum / double(r - 1) << std::endl;
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
	for(volatile unsigned int i = 0 /*запрещаем оптимизацию счётчика цикла для гарантии одинакового поведения начинки цикла*/; i < g_cnt; ++i)\
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
	if (p_alloc)
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
unsigned int g_cnt = INT_MAX; /* std::numeric_limits<unsigned int>::max(); */
const unsigned int g_size = 1000 * 1000;
const unsigned int g_ext_cnt = 1000;
#endif

DECLARE_PERFORMANCE_FILE_STREAM(profiler - swap.log, g_flylinkdc_perf_swap);
DECLARE_PERFORMANCE_CHECKER(1, g_flylinkdc_perf_swap);
DECLARE_PERFORMANCE_CHECKER(2, g_flylinkdc_perf_swap);
DECLARE_PERFORMANCE_CHECKER(3, g_flylinkdc_perf_swap);
DECLARE_PERFORMANCE_CHECKER(4, g_flylinkdc_perf_swap);

DECLARE_PERFORMANCE_FILE_STREAM(profiler - map.log, g_flylinkdc_map_insert);
DECLARE_PERFORMANCE_CHECKER(5, g_flylinkdc_map_insert);
DECLARE_PERFORMANCE_CHECKER(6, g_flylinkdc_map_insert);
DECLARE_PERFORMANCE_CHECKER(7, g_flylinkdc_map_insert);

//DECLARE_PERFORMANCE_CHECKER(5, g_flylinkdc_perf_swap);


DECLARE_PERFORMANCE_FILE_STREAM(profiler - dir - scan.log, g_dir_scan);

DECLARE_PERFORMANCE_CHECKER(101, g_dir_scan);
DECLARE_PERFORMANCE_CHECKER(102, g_dir_scan);
DECLARE_PERFORMANCE_CHECKER(103, g_dir_scan);
///////////////////////////////////////////
static int g_hash_byte = 0;
static __int64 g_sum_byte = 0;
///////////////////////////////////////////
static void Process_Files(const boost::filesystem::path &Path, bool recurse)
{
//	std::cout << "Folder: " << Path << " [scan=" << g_sum_byte/1024/1024/1024 << " gb]\n";
	boost::filesystem::directory_iterator end_itr;
	
	for (
	    boost::filesystem::directory_iterator itr(Path);
	    itr != end_itr;
	    ++itr
	)
	{
		if (recurse && boost::filesystem::is_directory(*itr))
		{
			boost::filesystem::path Deeper(*itr);
			Process_Files(Deeper, recurse);
			continue;
		}
		for (int i = 1; i < 4; ++i)
		{
			switch (i)
			{
				case 2: // atl-map
				{
					START_PERFORMANCE_CHECK(101);
					CAtlFile l_file;
					const std::wstring filename = itr->path().generic_wstring();
					const SIZE_T filesize = boost::filesystem::file_size(filename);
					HRESULT hr = l_file.Create(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
					if (SUCCEEDED(hr))
					{
						CAtlFileMapping<char> l_filemap;
						hr = l_filemap.MapFile(l_file, 0, 0, PAGE_READONLY, FILE_MAP_READ);
						if (SUCCEEDED(hr))
						{
							const auto l_size = filesize; //l_filemap.GetMappingSize();
							g_sum_byte += l_size;
							char* l_Mem = l_filemap;
							for (size_t i = 0; i < l_size; ++i)
							{
								g_hash_byte += l_Mem[i];
							}
						}
						else
						{
							std::cout << "Error l_filemap.MapFile: " << *itr << " GetLastError() = " << ::GetLastError() << std::endl;
							std::wcout << translateError(::GetLastError());
						}
					}
					else
					{
						std::cout << "Error l_file.Create[MAP]: " << *itr << " GetLastError() = " << ::GetLastError() << std::endl;
						std::wcout << translateError(::GetLastError());
					}
					FINISH_PERFORMANCE_CHECK(101);
				}
				break;
				case 1: // boost - map
				{
					START_PERFORMANCE_CHECK(102);
					boost::iostreams::mapped_file_source l_map_file;
					l_map_file.open(itr->path()); // filename.c_str() // , filesize
					if (l_map_file.is_open())
					{
						const auto l_size = l_map_file.size();
						g_sum_byte += l_size;
						const char* l_Mem = l_map_file.data();
						for (size_t i = 0; i < l_size; ++i)
						{
							g_hash_byte += l_Mem[i];
						}
					}
					else
					{
						std::cout << "Error boost::iostreams::mapped_file_source l_map_file: " << *itr << " GetLastError() = " << ::GetLastError() << std::endl;
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
					HRESULT hr = l_file.Create(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
					if (SUCCEEDED(hr))
					{
						if (filesize)
						{
							std::vector<char> l_buf(filesize);
							hr = l_file.Read(l_buf.data(), filesize);
							if (SUCCEEDED(hr))
							{
								g_sum_byte += filesize;
								for (size_t i = 0; i < filesize; ++i)
								{
									g_hash_byte += l_buf[i];
								}
							}
							else
							{
								std::cout << "Error l_file.Read: " << *itr << " GetLastError() = " << ::GetLastError() << std::endl;
								std::wcout << translateError(::GetLastError());
							}
						}
					}
					else
					{
						std::cout << "Error l_file.Create: " << *itr << " GetLastError() = " << ::GetLastError() << std::endl;
						std::wcout << translateError(::GetLastError());
					}
					FINISH_PERFORMANCE_CHECK(103);
				}
				break;
			}
		}
	}
	return;
}
///////////////////////////////////////////
std::vector<std::string> g_target_vector;
std::vector<std::string> getSourceVector()
{
	std::vector<std::string> l_vec;
	for (int j = 0; j < 10000; ++j)
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
	TEST_CODE(swap(g_target_vector, l_vec), 2);
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
	
	//TEST_CODE(BoostFastLock l(l_boost_fcs); l_count++, 5); // Это пока вешается - не понятно почему
	
	std::cout << l_count << std::endl;
}

void testFastAlloc()
{
	DECLARE_PERFORMANCE_FILE_STREAM(profiler - fastalloc.log, g_flylinkdc_perf_fast_alloc);
	
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
	//::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL); // [?] ставим повышенный приоритет процессу для исключения мнимых задержек в системной очереди.
	
	::SetThreadAffinityMask(::GetCurrentThread(), affinityMask); // устанавливаем используемый процессор (ядро).
	
	// "Греем" железо для исключения возможности работы процессора на пониженных частотах во время теста.
	volatile int heaterCount = INT_MIN; // std::numeric_limits<int>::min();
	volatile int heater = INT_MIN; // std::numeric_limits<int>::min();
	
	for (volatile int i = 0; i != 10; ++i)
	{
		std::cout << ".";
		for (; heater != INT_MIN /*std::numeric_limits<int>::max()*/; ++heater)
		{
			++heaterCount;
		}
		std::cout << ".";
		for (; heater != INT_MIN /*std::numeric_limits<int>::min() */; --heater)
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
	_snprintf(buf, _countof(buf), "%d", val);
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
	for (size_t i = 0; i < dimension; ++i)
		pixels.push_back(1);
}
void UseVectorResize(size_t dimension)
{
	TestTimer t("UseVectorResize");
	std::vector<char> pixels;
	pixels.resize(dimension);
	for (size_t i = 0; i < dimension; ++i)
		pixels[i] = 1;
}

void UseVectorPushBack(size_t dimension)
{
	TestTimer t("UseVectorPushBack");
	std::vector<char> pixels;
	pixels.reserve(dimension);
	for (size_t i = 0; i < dimension; ++i)
		pixels.push_back(1);
}
string read_file(LPCTSTR p_file)
{
	string l_data;
	CAtlFile l_file;
	HRESULT hr = l_file.Create(p_file, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
	if (SUCCEEDED(hr))
	{
		ULONGLONG len = 0;
		l_file.GetSize(len);
		l_data.resize(len);
		hr = l_file.Read((LPVOID) l_data.data(), len);
		if (SUCCEEDED(hr))
		{
		}
	}
	return l_data;
}
void convert_p2p_guard()
{
	typedef std::map<uint32_t, uint16_t> CountryList;
	typedef CountryList::iterator CountryIter;
	static CountryList countries;
	{
		string data = read_file(L"GeoIpCountryWhois.csv");
		const char* start = data.c_str();
		string::size_type linestart = 0;
		string::size_type comma1 = 0;
		string::size_type comma2 = 0;
		string::size_type comma3 = 0;
		string::size_type comma4 = 0;
		string::size_type lineend = 0;
		CountryIter last = countries.end();
		uint32_t startIP = 0;
		uint32_t endIP = 0, endIPprev = 0;
		
		for (;;)
		{
			comma1 = data.find(',', linestart);
			if (comma1 == string::npos) break;
			comma2 = data.find(',', comma1 + 1);
			if (comma2 == string::npos) break;
			comma3 = data.find(',', comma2 + 1);
			if (comma3 == string::npos) break;
			comma4 = data.find(',', comma3 + 1);
			if (comma4 == string::npos) break;
			lineend = data.find('\n', comma4);
			if (lineend == string::npos) break;
			
			startIP = atoi(start + comma2 + 2);
			endIP = atoi(start + comma3 + 2);
			uint16_t* country = (uint16_t*)(start + comma4 + 2);
			if ((startIP - 1) != endIPprev)
				last = countries.insert(last, std::make_pair((startIP - 1), (uint16_t)16191));
			last = countries.insert(last, std::make_pair(endIP, *country));
			
			endIPprev = endIP;
			linestart = lineend + 1;
		}
	}
// read p2pguard.ini
	{
		string l_data = read_file(L"P2PGuard.ini");
		std::fstream l_out("P2PGuard_flylink.ini", std::ios_base::out | std::ios_base::trunc);
		uint32_t linestart = 0;
		uint32_t lineend = 0;
		for (;;)
		{
			lineend = l_data.find('\n', linestart);
			if (lineend == string::npos)
				break;
			if (lineend == linestart)
			{
				linestart++;
				continue;
			}
			const string l_currentLine = l_data.substr(linestart, lineend - linestart - 1);
			linestart = lineend + 1;
			uint32_t a = 0, b = 0, c = 0, d = 0, a2 = 0, b2 = 0, c2 = 0, d2 = 0;
			const int l_Items = sscanf_s(l_currentLine.c_str(), "%u.%u.%u.%u-%u.%u.%u.%u", &a, &b, &c, &d, &a2, &b2, &c2, &d2);
			if (l_Items == 8)
			{
				const uint32_t l_startIP = (a << 24) + (b << 16) + (c << 8) + d;
				const uint32_t l_endIP = (a2 << 24) + (b2 << 16) + (c2 << 8) + d2;
				const auto l_pos = l_currentLine.find(' ');
				if (l_pos != string::npos)
				{
					const string l_note =  l_currentLine.substr(l_pos + 1);
					dcassert(!l_note.empty())
					CountryList::const_iterator i = countries.lower_bound(l_startIP);
					if (i != countries.end())
					{
						const string l_country = string((char*) & (i->second), 2);
						if (l_country != "RU" && l_country != "KZ" && l_country != "UZ" && l_country != "BY" && l_country != "UA" && l_country != "AZ")
						{
							l_out << l_currentLine << std::endl;
						}
					}
				}
			}
		}
	}
}
uint8_t TestFunc1(const uint8_t &ui8NickLen, const bool &bFromPM)
{
	return bFromPM ? ui8NickLen : ui8NickLen + 1;
}
uint8_t TestFunc2(const uint8_t ui8NickLen, const bool bFromPM)
{
	return bFromPM ? ui8NickLen : ui8NickLen + 1;
}
int zmq_test_client()
{
	void* context = zmq_ctx_new();
	printf("Client Starting….\n");
	int major = 0, minor = 0, patch = 0;
	zmq_version(&major, &minor, &patch);
	printf("ZeroMQ version: %d.%d.%d\n", major, minor, patch);
	
	void* request = zmq_socket(context, ZMQ_REQ);
	int ipv6 = 1;
	const auto l_result_opt = zmq_setsockopt(socket, ZMQ_IPV6, &ipv6, 4);
	if (l_result_opt != 0)
		printf("zmq_setsockopt = %d\n", errno);
	// zmq_connect(request, "tcp://51.254.84.24:4040");
	//auto l_result_connect = zmq_connect(request, "tcp://188.209.52.233:4040");
	auto l_result_connect = zmq_connect(request, "tcp://[2001:41D0:000A:1A3B:0000:0000:0000:0012]:4040");
	if (l_result_connect != 0)
		printf("zmq_connect = %d\n", errno);
		
		
	int count = 0;
	
	for (;;)
	{
		zmq_msg_t req;
		zmq_msg_init_size(&req, strlen("hello"));
		memcpy(zmq_msg_data(&req), "hello", 5);
		printf("Sending: hello - %d\n", count);
		zmq_msg_send(&req, request, 0);
		zmq_msg_close(&req);
		zmq_msg_t reply;
		zmq_msg_init(&reply);
		zmq_msg_recv(&reply, request, 0);
		printf("Received: hello - %d\n", count);
		zmq_msg_close(&reply);
		count++;
	}
	// We never get here though.
	zmq_close(request);
	zmq_ctx_destroy(context);
}
int _tmain(int argc, _TCHAR* argv[])
{
	zmq_test_client();
	
	
	std::cout << "Timing boost::unordered_map<int,int>" << std::endl;
	timemap<boost::unordered_map<int, int> >();
	std::cout << std::endl;
	
	std::cout << "Timing std::unordered_map<int,int>" << std::endl;
	timemap<std::unordered_map<int, int> >();
	std::cout << std::endl;
	
	std::cout << "Timing std::map<int,int>" << std::endl;
	timemap<std::map<int, int> >();
	std::cout << std::endl;
	
	return 0;
	
	Process_Files("Y:\\dc-test-issue-956", true);  // boost::filesystem::current_path()
	std::cout << std::endl << "g_hash_byte = " << g_hash_byte <<   " g_sum_byte = " << g_sum_byte <<  std::endl;
	return 0;
	
	
	get_adapters();
	// getmac();
	return 0;
	
	//      <wasd_remote> 20:58:16 <PPK> TestFunc1(const uint8_t &ui8NickLen, const bool &bFromPM) versus TestFunc2(const uint8_t ui8NickLen, const bool bFromPM)
	//      <wasd_remote> 20:58:26 <PPK> both called 18446744073709551615 times
	// const auto max_count = 18446744073709551615L;
	const __int64 max_count = 10000000000000L;
	uint8_t sum = 0;
	bool l_bool = false;
	START_PERFORMANCE_CHECK(1);
	for (__int64 i = 0; i < max_count; ++i)
	{
		sum += TestFunc1(sum, l_bool);
		l_bool = !l_bool;
	}
	FINISH_PERFORMANCE_CHECK(1);
	std::cout << sum << std::endl;
	sum = 0;
	l_bool = false;
	START_PERFORMANCE_CHECK(2);
	for (__int64 j = 0; j < max_count; ++j)
	{
		sum += TestFunc2(sum, l_bool);
		l_bool = !l_bool;
	}
	FINISH_PERFORMANCE_CHECK(2);
	std::cout << sum << std::endl;
	
	return 0;
	convert_p2p_guard();
	return 0;
	
	
#if 0
	CRITICAL_SECTION cs;
	InitializeCriticalSectionAndSpinCount(&cs, 100);
//	InitializeCriticalSection(&cs);
	EnterCriticalSection(&cs);
	EnterCriticalSection(&cs);
	LeaveCriticalSection(&cs);
	LeaveCriticalSection(&cs);
	DeleteCriticalSection(&cs);
	
	typedef std::pair<boost::asio::ip::address_v4, boost::asio::ip::address_v4> CFlyDDOSkey;
	boost::unordered_map<CFlyDDOSkey, int> ip4_map;
	for (size_t i = 0; i < 1 * 1024 * 1024 * 1024; i += 100 * 1024 * 1024)
	{
		std::cout << "dimension = " << i / 1024 / 1024 << " Mb: " << "\t";
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
		for (int i = 0; i < max_count; ++i)
		{
			elements.push_back(std::string());
		}
		FINISH_PERFORMANCE_CHECK(5);
	}
	
	{
		std::vector<std::string> elements;
		START_PERFORMANCE_CHECK(6);
		for (int i = 0; i < max_count; ++i)
		{
			elements.emplace_back(std::string());
		}
		FINISH_PERFORMANCE_CHECK(6);
	}
	return 0;
	{
		boost::unordered_map<std::string, double> myrecipe;
		START_PERFORMANCE_CHECK(5);
		for (int i = 0; i < max_count; ++i)
		{
			myrecipe.insert(myrecipe.end(), std::make_pair<std::string, double>(toString(i), 0)); // move insertion + hint
		}
		FINISH_PERFORMANCE_CHECK(5);
	}
	
	{
		boost::unordered_map<std::string, double> myrecipe;
		START_PERFORMANCE_CHECK(6);
		for (int i = 0; i < max_count; ++i)
		{
			const auto s = toString(i);
			if (myrecipe.find(s) == myrecipe.end())
			{
				myrecipe.insert(std::make_pair<std::string, double>(toString(i), 0)); // move insertion
			}
		}
		FINISH_PERFORMANCE_CHECK(6);
	}
	
	{
		boost::unordered_map<std::string, double> myrecipe;
		START_PERFORMANCE_CHECK(7);
		for (int i = 0; i < max_count; ++i)
		{
			std::pair<std::string, double> l_val(toString(i), 0);
			myrecipe.insert(l_val);  // copy insertion
		}
		FINISH_PERFORMANCE_CHECK(7);
	}
	
	
	return 0;
#endif
	
	//boost::unordered_map<boost::asio::ip::address_v4, int> tmp;
	boost::asio::ip::address_v4 ip = boost::asio::ip::address_v4::from_string("010.010.010.010");
	//tmp[ip] = 1;
	
	// [+] IRainman fix.
	::SetProcessPriorityBoost(::GetCurrentProcess(), FALSE); // Запрещаем системе изменять приоритет процесса.
	
	processPreparing(1); // [!] Установите разные ядра для разных потоков при тестировании параллельного кода для снижения искажений за счёт выполнения тестов на одном ядре.
	
	std::cout << "Tests started" << std::endl;
	// [~] IRainman fix.
	
	
	test_critical_section();
	//testFastAlloc();
	//test_replace_vector();
	//test_swap_vector();
	//test_swap_member_vector();
	
	
#if 0
	std::unordered_set<int> l_id;
	
	for (int i = 0; i < g_cnt; ++i)
		l_id.insert(i);
		
	DECLARE_PERFORMANCE_CHECKER(1, g_flylinkdc_perf_fast_alloc);
	DECLARE_PERFORMANCE_CHECKER(2, g_flylinkdc_perf_fast_alloc);
	DECLARE_PERFORMANCE_CHECKER(3, g_flylinkdc_perf_fast_alloc);
	
	unsigned int l_sum = 0;
	unsigned int l_sum2 = 0;
	unsigned int l_sum3 = 0;
	for (int i = 0; i < g_ext_cnt; i++)
	{
		for (int i = 0; i < g_cnt; i++)
		{
			START_PERFORMANCE_CHECK(1);
			if (l_id.count(i) == 1)
				l_sum += 1;
			FINISH_PERFORMANCE_CHECK(1);
		}
		
		for (int i = 0; i < g_cnt; i++)
		{
			START_PERFORMANCE_CHECK(2);
			if (l_id.count(i) != 0)
				l_sum2 += 1;
			FINISH_PERFORMANCE_CHECK(2);
		}
		
		for (int i = 0; i < g_cnt; i++)
		{
			START_PERFORMANCE_CHECK(3);
			if (l_id.find(i) != l_id.end())
				l_sum3 += 1;
			FINISH_PERFORMANCE_CHECK(3);
		}
	}
	std::cout << " sum1 = " << l_sum << "  sum2 = " << l_sum2 << "  sum3 = " << l_sum3 << std::endl;
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
		for (int i = 0; i < l_cnt; i++)
		{
			START_PERFORMANCE_CHECK(4);
			const std::string l_res = toString(" ", l_test, true);
			FINISH_PERFORMANCE_CHECK(4);
		}
		DECLARE_PERFORMANCE_CHECKER(5, g_flylinkdc_perf_fast_alloc);
		for (int i = 0; i < l_cnt; i++)
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

// https://msdn.microsoft.com/en-us/library/aa365915.aspx
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

int getmac()
{
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	PIP_ADAPTER_ADDRESSES pAddressesOrig = NULL;
	ULONG outBufLen = 0;
	DWORD dwRetVal = 0;
	
	pAddresses = (IP_ADAPTER_ADDRESSES*) MALLOC(sizeof(IP_ADAPTER_ADDRESSES));
	
	if (GetAdaptersAddresses(AF_INET,
	                         0,
	                         NULL,
	                         pAddresses,
	                         &outBufLen) == ERROR_BUFFER_OVERFLOW)
	{
		FREE(pAddresses);
		pAddresses = (IP_ADAPTER_ADDRESSES*) MALLOC(outBufLen);
		pAddressesOrig = pAddresses;
	}
	
	
	
	if ((dwRetVal = GetAdaptersAddresses(AF_INET, // AF_UNSPEC
	                                     0,
	                                     NULL,
	                                     pAddresses,
	                                     &outBufLen)) == NO_ERROR)
	{
		while (pAddresses)
		{
			printf("AdapterName: %S ", pAddresses->AdapterName);
			printf("Description: %S ", pAddresses->Description);
			printf("PhysicalAddress: %02x-%02x-%02x-%02x-%02x-%02x\n",
			       pAddresses->PhysicalAddress[0],
			       pAddresses->PhysicalAddress[1],
			       pAddresses->PhysicalAddress[2],
			       pAddresses->PhysicalAddress[3],
			       pAddresses->PhysicalAddress[4],
			       pAddresses->PhysicalAddress[5]);
			pAddresses = pAddresses->Next;
		}
	}
	else
	{
		printf("Get Adapter Information failed! GetlastError() = %d ", GetLastError());
	}
	if (pAddressesOrig)
	{
		FREE(pAddressesOrig);
	}
	return 0;
}

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3
void get_adapters()
{

	DWORD dwSize = 0;
	DWORD dwRetVal = 0;
	
	unsigned int i = 0;
	
	// Set the flags to pass to GetAdaptersAddresses
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	
	// default to unspecified address family (both)
	ULONG family = AF_UNSPEC;
//        family = AF_INET;
//        family = AF_INET6;

	LPVOID lpMsgBuf = NULL;
	
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	ULONG outBufLen = 0;
	ULONG Iterations = 0;
	
	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
	PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
	PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
	IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
	IP_ADAPTER_PREFIX *pPrefix = NULL;
	
	
	printf("Calling GetAdaptersAddresses function with family = ");
	if (family == AF_INET)
		printf("AF_INET\n");
	if (family == AF_INET6)
		printf("AF_INET6\n");
	if (family == AF_UNSPEC)
		printf("AF_UNSPEC\n\n");
		
	// Allocate a 15 KB buffer to start with.
	outBufLen = WORKING_BUFFER_SIZE;
	
	do
	{
	
		pAddresses = (IP_ADAPTER_ADDRESSES *) MALLOC(outBufLen);
		if (pAddresses == NULL)
		{
			printf
			("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
			exit(1);
		}
		
		dwRetVal =
		    GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
		    
		if (dwRetVal == ERROR_BUFFER_OVERFLOW)
		{
			FREE(pAddresses);
			pAddresses = NULL;
		}
		else
		{
			break;
		}
		
		Iterations++;
		
	}
	while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));
	
	if (dwRetVal == NO_ERROR)
	{
		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses;
		while (pCurrAddresses)
		{
			//printf("\tLength of the IP_ADAPTER_ADDRESS struct: %ld\n",
			//       pCurrAddresses->Length);
			//printf("\tIfIndex (IPv4 interface): %u\n", pCurrAddresses->IfIndex);
			printf("\tAdapter name: %s\n", pCurrAddresses->AdapterName);
			/*
			
			            pUnicast = pCurrAddresses->FirstUnicastAddress;
			            if (pUnicast != NULL) {
			                for (i = 0; pUnicast != NULL; i++)
			                    pUnicast = pUnicast->Next;
			                printf("\tNumber of Unicast Addresses: %d\n", i);
			            } else
			                printf("\tNo Unicast Addresses\n");
			
			            pAnycast = pCurrAddresses->FirstAnycastAddress;
			            if (pAnycast) {
			                for (i = 0; pAnycast != NULL; i++)
			                    pAnycast = pAnycast->Next;
			                printf("\tNumber of Anycast Addresses: %d\n", i);
			            } else
			                printf("\tNo Anycast Addresses\n");
			
			            pMulticast = pCurrAddresses->FirstMulticastAddress;
			            if (pMulticast) {
			                for (i = 0; pMulticast != NULL; i++)
			                    pMulticast = pMulticast->Next;
			                printf("\tNumber of Multicast Addresses: %d\n", i);
			            } else
			                printf("\tNo Multicast Addresses\n");
			
			            pDnServer = pCurrAddresses->FirstDnsServerAddress;
			            if (pDnServer) {
			                for (i = 0; pDnServer != NULL; i++)
			                    pDnServer = pDnServer->Next;
			                printf("\tNumber of DNS Server Addresses: %d\n", i);
			            } else
			                printf("\tNo DNS Server Addresses\n");
			*/
			//TOOD printf("\tDNS Suffix: %wS\n", pCurrAddresses->DnsSuffix);
			printf("\tDescription: %wS\n", pCurrAddresses->Description);
			printf("\tFriendly name: %wS\n", pCurrAddresses->FriendlyName);
			
			/*
			if (pCurrAddresses->PhysicalAddressLength != 0) {
			        printf("\tPhysical address: ");
			        for (i = 0; i < (int) pCurrAddresses->PhysicalAddressLength;
			             i++) {
			            if (i == (pCurrAddresses->PhysicalAddressLength - 1))
			                printf("%.2X\n",
			                       (int) pCurrAddresses->PhysicalAddress[i]);
			            else
			                printf("%.2X-",
			                       (int) pCurrAddresses->PhysicalAddress[i]);
			        }
			    }
			    */
			printf("\tFlags: %ld\n", pCurrAddresses->Flags);
			printf("\tFlags.Ipv6Enabled: %d\n", int(pCurrAddresses->Ipv6Enabled));
			// TODO printf("\tMtu: %lu\n", pCurrAddresses->Mtu);
			// TODO printf("\tIfType: %ld\n", pCurrAddresses->IfType);
			// TODO printf("\tOperStatus: %ld\n", pCurrAddresses->OperStatus);
			// TODO printf("\tIpv6IfIndex (IPv6 interface): %u\n",
			// TODO        pCurrAddresses->Ipv6IfIndex);
			/*
			printf("\tZoneIndices (hex): ");
			for (i = 0; i < 16; i++)
			    printf("%lx ", pCurrAddresses->ZoneIndices[i]);
			printf("\n");
			
			printf("\tTransmit link speed: %I64u\n", pCurrAddresses->TransmitLinkSpeed);
			printf("\tReceive link speed: %I64u\n", pCurrAddresses->ReceiveLinkSpeed);
			
			pPrefix = pCurrAddresses->FirstPrefix;
			if (pPrefix) {
			    for (i = 0; pPrefix != NULL; i++)
			        pPrefix = pPrefix->Next;
			    printf("\tNumber of IP Adapter Prefix entries: %d\n", i);
			} else
			    printf("\tNumber of IP Adapter Prefix entries: 0\n");
			*/
			printf("\n");
			
			pCurrAddresses = pCurrAddresses->Next;
		}
	}
	else
	{
		printf("Call to GetAdaptersAddresses failed with error: %d\n",
		       dwRetVal);
		if (dwRetVal == ERROR_NO_DATA)
			printf("\tNo addresses were found for the requested parameters\n");
		else
		{
		
			if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			                  FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			                  NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			                  // Default language
			                  (LPTSTR) & lpMsgBuf, 0, NULL))
			{
				printf("\tError: %s", lpMsgBuf);
				LocalFree(lpMsgBuf);
				if (pAddresses)
					FREE(pAddresses);
				exit(1);
			}
		}
	}
	
	if (pAddresses)
	{
		FREE(pAddresses);
	}
	
	
}
