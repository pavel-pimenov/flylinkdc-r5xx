#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <atlfile.h>
#include <unordered_map>
#include <set>
#include <stdint.h>
#include <boost/algorithm/string.hpp>
using namespace std;

string read_file(LPCSTR p_file)
{
    string l_data;
    CAtlFile l_file;
    HRESULT hr = l_file.Create(p_file, GENERIC_READ,FILE_SHARE_READ, OPEN_EXISTING);
    if( SUCCEEDED(hr) )
    {
        ULONGLONG len = 0;
        l_file.GetSize(len);
        l_data.resize(len);
        hr = l_file.Read((LPVOID) l_data.data(),len);
        if( SUCCEEDED(hr) )
        {
        }
    }
    return l_data;
}
typedef std::map<uint32_t, uint16_t> CountryList;
typedef CountryList::iterator CountryIter;
static CountryList countries;
static map<string,int> g_new_country;
static map<string,string> g_country_desc;

void convert_p2p_guard()
{
    {
        string data = read_file("GeoIpCountryWhois.csv");
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
            g_country_desc[string(start + comma4 + 2, 2)] = string(start + comma4, lineend-comma4);
            endIPprev = endIP;
            linestart = lineend + 1;
        }
    }
// read p2pguard.ini
    {
        string l_data = read_file("P2PGuard.ini");
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
            string l_currentLine = l_data.substr(linestart, lineend - linestart - 1);
            boost::replace_all(l_currentLine, " - ", "-");
            boost::replace_all(l_currentLine, " , 000 , ", " ");
            string l_old_line =  l_currentLine;
            for(int i=0; i<100; ++i)
            {
                char l_mark_0[4] = {0};
                char l_mark[4] = {0};
                sprintf(l_mark_0,"%03d",i);
                sprintf(l_mark,"%d",i);
                boost::replace_all(l_currentLine, l_mark_0,l_mark);
            }
            //if(l_currentLine != l_old_line)
            //   cout << l_old_line << " --> " << l_currentLine << endl;
            linestart = lineend + 1;
            if(
               l_currentLine.find("100.64.0.0-100.127.255.255") == string::npos &&
               l_currentLine.find("Beeline") == string::npos &&
               l_currentLine.find("VimpelCom") == string::npos
              )
	    {
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
                    CountryIter i = countries.lower_bound(l_startIP);
                    if (i != countries.end())
                    {
                        const string l_country = string((char*) & (i->second), 2);
                        if(l_country != "RU" && l_country != "KZ" && l_country != "UZ" && l_country != "BY" && l_country != "UA" && l_country != "AZ"  && l_country != "LV" && l_country != "AM")
                        {
                            g_new_country[l_country]++;
                            l_out << l_currentLine << std::endl;
                        }
                    }
                    else
                    {
                      l_out << l_currentLine << std::endl;
                    }  
                }
                else
                {
                    cout << "error l_pos != string::npos Line = " << l_currentLine << endl;
                }
            }
            else
            {
                cout << "error l_Items != 8 Line = " << l_currentLine << endl;
            }
          }
        }
    }

    for (auto i= g_new_country.cbegin(); i!=g_new_country.cend(); ++i)
    {
        std::cout << i->first << " = " << i->second << g_country_desc[i->first] << endl;
    }
}
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    cout << endl << "P2P Guard convertor" << endl;
    convert_p2p_guard();
    return 0;
}
