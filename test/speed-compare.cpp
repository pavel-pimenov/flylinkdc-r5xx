#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <vector>
#include <windows.h>
#include <bcrypt.h>

#include <boost/unordered/unordered_map.hpp>
#include <unordered_set>
#include <unordered_map>


const int smallCount = 100;
const int largeCount = 10000000;
using duration_seconds = std::chrono::duration<double>;
using time_provider = std::chrono::high_resolution_clock;

HCRYPTPROV globalCryptoApiProvider{ 0 };
time_provider::time_point globalStartMoment, globalFinishMoment;


void StartMeasure() {
	globalStartMoment = time_provider::now();
}

void FinishMeasure(const std::string &name) {
	globalFinishMoment = time_provider::now();
	duration_seconds duration = globalFinishMoment - globalStartMoment;
        //printf("%s: "): 
	std::cout << name.c_str() << ":   ";
        std::cout << std::fixed << std::setw(8) << std::setprecision(2) << duration.count() << " sec\n";
}

std::string toString(int val)
{
	char buf[16];
	_snprintf(buf, _countof(buf), "%d", val);
	return buf;
}


int main() {

    std::cout << "Start\n";
    int max_count = 100000000;
    

	{
		StartMeasure();
		std::unordered_map<std::string, int> m;
		for (int i = 0; i < max_count; ++i)
		{
		   m[toString(i)] = i;
		} 
		FinishMeasure("std::unordered_map");
	}

	{
		StartMeasure();
		boost::unordered_map<std::string, int> m;
		for (int i = 0; i < max_count; ++i)
		{
		   m[toString(i)] = i;
		} 
		FinishMeasure("boost::unordered_map");
	}

	return 0;
}
