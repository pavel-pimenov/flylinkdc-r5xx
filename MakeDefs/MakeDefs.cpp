// MakeDefs.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "../client/SimpleXML.h"
#include "../client/StringTokenizer.h"
#include "../client/ResourceManager.h"


string ResourceManager::g_strings[];
wstring ResourceManager::g_wstrings[];


int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s <HeaderFile> <OutputCppFile> <OutputXmlFile>\n", argv[0]);
		return 1;
	}
	
	try
	{
		string tmp;
		string sourceFile(argv[1]);
		string outputCppFile(argv[2]);
		string outputXmlFile(argv[3]);

		sourceFile = Text::acpToUtf8(sourceFile);
		outputCppFile = Text::acpToUtf8(outputCppFile);
		outputXmlFile = Text::acpToUtf8(outputXmlFile);
		
		File::ensureDirectory(outputCppFile);
		File::ensureDirectory(outputXmlFile);

		File src(sourceFile, File::READ, File::OPEN, false);
		File tgt(outputCppFile, File::WRITE, File::CREATE | File::TRUNCATE, false);
		File example(outputXmlFile, File::WRITE, File::CREATE | File::TRUNCATE, false);
		string x = src.read();
		x = Text::acpToUtf8(x);
		string::size_type k;
		
		while ((k = x.find('\r')) != string::npos)
			x.erase(k, 1);

		StringList l = StringTokenizer<string>(x, '\n').getTokens();

		string varStr;
		string varName;
		string start;

		SimpleXML ex;

		for (auto i = l.begin(); i != l.end();)
		{
			if ((k = i->find("// @Strings: ")) != string::npos)
			{
				varStr = i->substr(k + 13);
				i = l.erase(i);
			}
			else if ((k = i->find("// @Names: ")) != string::npos)
			{
				varName = i->substr(k + 11);
				i = l.erase(i);
			}
			else if (i->find("// @DontAdd") != string::npos)
			{
				i = l.erase(i);
			}
			else if ((k = i->find("// @Prolog: ")) != string::npos)
			{
				start += i->substr(k + 12) + "\r\n";
				i = l.erase(i);
			}
			else if (i->size() < 5)
				i = l.erase(i);
			else
				++i;
		}

		if (varStr.empty() || varName.empty())
		{
			printf("No @Strings or @Names\n");
			return 0;
		}
		
		varStr += " = {\r\n";
		varName += " = {\r\n";
/*		
		ex.addTag("Language");
		ex.addChildAttrib("Name", string("Example Language"));
		ex.addChildAttrib("Author", string("FlylinkDC++ Development Team"));
		//ex.addChildAttrib("Version", string(A_VERSIONSTRING));
		ex.addChildAttrib("Revision", string("1"));
		ex.stepIn();
*/
		ex.addTag("resources");
		ex.stepIn();
		string name;
		string def;
		string xmldef;
		string s;
		for (auto i = l.begin(); i != l.end(); i++)
		{
			name.clear();
			s = *i;

			bool u = true;
			for (k = s.find_first_not_of(" \t"); s[k] != ','; k++)
			{
				if (s[k] == '_')
				{
					u = true;
				}
				else if (u)
				{
					name += s[k];
					u = false;
				} else
					name+=(char)tolower(s[k]);
			}

			k = s.find("// ");
			def = s.substr(k + 3);
			xmldef = def.substr(1, def.size() - 2);
/*			while( (k = xmldef.find("\\t")) != string::npos) {
				xmldef.replace(k, 2, "\t");
			}
			while( (k = xmldef.find("\\r")) != string::npos) {
				xmldef.replace(k, 2, "\r");
			}
			while( (k = xmldef.find("\\n")) != string::npos) {
				xmldef.replace(k, 2, "\n");
			}

			while( (k = xmldef.find("\\\\")) != string::npos) {
				xmldef.replace(k, 2, "\\");
			}
			*/
			ex.addTag("string", xmldef);
			ex.addChildAttrib("name", name);

			varStr += def + ", \r\n";
			varName += '\"' + name + "\", \r\n";
		}

		varStr.erase(varStr.size()-2, 2);
		varName.erase(varName.size()-2, 2);

		varStr += "\r\n};\r\n";
		varName += "\r\n};\r\n";

		tgt.write(start);
		tgt.write(varStr);
		tgt.write(varName);

		example.write(SimpleXML::utf8Header);
		example.write(ex.toXML());
	}
	catch (const Exception& e)
	{
		fprintf(stderr, "%s: %s\n", argv[0], e.getError().c_str());
		return 2;
	}

	return 0;
}
