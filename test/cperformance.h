#pragma once

#include <windows.h>
#include <fstream>
#include <string>

namespace performance
{
	// [+] IRainman fix.
//#define USE_THREAD_TIMES_FOR_PERF_CHECK
#ifdef USE_THREAD_TIMES_FOR_PERF_CHECK
	class ThreadsTimes
	{
			int64_t m_kernelTime;
			int64_t m_userTime;
		public:
			void start()
			{
				getThreadTime(m_kernelTime, m_userTime);
			}
			double finish()
			{
				int64_t currentKernelTime, currentUserTime;

				getThreadTime(currentKernelTime, currentUserTime);

				m_kernelTime = currentKernelTime - m_kernelTime;

				m_userTime = currentUserTime - m_userTime;

				return double(m_kernelTime + m_userTime);
			}
		private:
			static void getThreadTime(int64_t& p_kernelTime, int64_t& p_userTime)
			{
				FILETIME creationTime;
				FILETIME exitTime;
				FILETIME kernelTime;
				FILETIME userTime;

				GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime);

				p_kernelTime = kernelTime.dwHighDateTime;
				p_kernelTime <<= 32;
				p_kernelTime += kernelTime.dwLowDateTime;

				p_userTime = userTime.dwHighDateTime;
				p_userTime <<= 32;
				p_userTime += userTime.dwLowDateTime;
			}
	};
#else USE_THREAD_TIMES_FOR_PERF_CHECK
	// [~] IRainman fix.

/// Класс замера времени.
class timer
{
		LARGE_INTEGER m_start;
		
	public:
	
		void start()
		{
			QueryPerformanceCounter(&m_start);
		}
		
		double finish()
		{
			LARGE_INTEGER finish, freq;
			QueryPerformanceCounter(&finish);
			QueryPerformanceFrequency(&freq);
			return double(finish.QuadPart - m_start.QuadPart) / double(freq.QuadPart);
		}
		
		template <class TStream> void finish_output(TStream & out)
		{
			const double l_finish = finish();
			out << l_finish << std::endl;
#ifdef USE_FLY_CONSOLE_TEST
			std::cout << l_finish << std::endl;
#endif
		}
};

#endif // USE_THREAD_TIMES_FOR_PERF_CHECK

/// Класс замера производительности многократного выполнения некого куска кода.
/// Накапливает статистику по времени ее выполнения и количества выполнения.
class instruction_checker
{
#ifdef USE_THREAD_TIMES_FOR_PERF_CHECK
	ThreadsTimes m_timer;
#else
		timer m_timer;
#endif
		
		class instruction_info
		{
				double m_time;
				size_t m_count;
				
			public:
			
				instruction_info() : m_time(0.0), m_count(0) {}
				void add_time(double time)
				{
					m_time += time;
				}
				void inc_count()
				{
					++m_count;
				}
				
				double get_time() const
				{
					return m_time;
				}
				size_t get_count() const
				{
					return m_count;
				}
				double get_average_time() const
				{
					return m_time / double(m_count);
				}
		} m_info;
		
	public:
	
		void start()
		{
			m_timer.start();
		}
		void finish()
		{
			m_info.add_time(m_timer.finish());
			m_info.inc_count();
		}
		
		double get_time() const
		{
			return m_info.get_time();
		}
		size_t get_count() const
		{
			return m_info.get_count();
		}
		double get_average_time() const
		{
			return m_info.get_average_time();
		}
};

/// Класс - guard для замера многократно-вызываемого участка кода.
/// Имеет интерфейс класса performance_instruction_checker и дополнительно
/// в деструкторе выводит в поток накопленную информацию.
/// Должен использоваться как статический экземпляр в функции.
class stream_guard
{
		instruction_checker m_checker;
		int m_instructionNumber;
		std::ofstream & m_outFile;
	public:
		stream_guard(int instructionNumber, std::ofstream & outFile)
			: m_instructionNumber(instructionNumber), m_outFile(outFile)
		{
		}
		void start()
		{
			m_checker.start();
		}
		void finish()
		{
			m_checker.finish();
		}
		~stream_guard()
		{
#ifdef USE_FLY_CONSOLE_TEST
			std::cout << "Instruction number = " << m_instructionNumber << std::endl;
			std::cout << "Hole time = " << m_checker.get_time() << std::endl;
			std::cout << "Call count = " << static_cast<unsigned int>(m_checker.get_count()) << std::endl;
			std::cout << "Average time = " << m_checker.get_average_time() << std::endl;
			std::cout << "===============================================" << std::endl;
#endif
			m_outFile << "Instruction number = " << m_instructionNumber << std::endl;
			m_outFile << "Hole time = " << m_checker.get_time() << std::endl;
			m_outFile << "Call count = " << static_cast<unsigned int>(m_checker.get_count()) << std::endl;
			m_outFile << "Average time = " << m_checker.get_average_time() << std::endl;
			m_outFile << "===============================================" << std::endl;
		}
};
}

/// Макрос задания файла для накопления статистики
#define DECLARE_PERFORMANCE_FILE_STREAM(fileName, varName)    \
	static std::ofstream varName(#fileName)

/// Макрос задания статического экземпляра checkera
#define DECLARE_PERFORMANCE_CHECKER(index, outFile)    \
	static performance::stream_guard performance_checker_##index(index, outFile)

/// Макрос начала замера
#define START_PERFORMANCE_CHECK(index)    \
	performance_checker_##index.start()

/// Макрос конца замера
#define FINISH_PERFORMANCE_CHECK(index)    \
	performance_checker_##index.finish()
