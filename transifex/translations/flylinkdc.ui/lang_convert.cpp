#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <algorithm>

using namespace std;
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  cout << endl << "Use: lang_convert.exe ru-RU.xml" << endl;
  if(argc == 2)
  {
    string l_inFileName = argv[1];
    string vOutFileName = string("new\\") + l_inFileName;
    std::replace(vOutFileName.begin(), vOutFileName.end(), '_', '-');
    fstream in(l_inFileName.c_str(), ios_base::in);
    fstream out(vOutFileName.c_str(), ios_base::out | ios_base::trunc);
    string strCurLine;
    map<string, string> l_change_map;
	l_change_map["<?xml version=\'1.0\' encoding=\'UTF-8\'?>"] = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>";
    l_change_map["<resources>"] = "<Language Name=\"Lang\" Author=\"FlylinkDC++ Development Team\" Revision=\"1\">\n	<Strings>";
    l_change_map["  <string name"] = "		<String Name";
    l_change_map["</string>"] = "</String>";
    l_change_map["</resources>"] = "	</Strings>\n</Language>";
    l_change_map["\\t"] = "\t";
	l_change_map["\\r"] = "";
	l_change_map["\\n"] = "\n";
	l_change_map["\\\'"] = "\'";
	l_change_map["\\\""] = "\"";
	l_change_map["\\\\"] = "\\";
	
    while(!getline(in, strCurLine).eof() || !strCurLine.empty())
    {
      for(map<string, string>::const_iterator j = l_change_map.begin();
          j != l_change_map.end();
          ++j)
      {
        int i = strCurLine.find(j->first);
		while (i !=string::npos)
        {
          strCurLine.replace(i, j->first.length(), j->second);
          cout << "||||||||||||||| " << j->first << "  ---------->  " << j->second << endl;
		  i = strCurLine.find(j->first);
        }
      }
      out << strCurLine << endl;
      strCurLine = "";
    }
  }
  return 0;
}
