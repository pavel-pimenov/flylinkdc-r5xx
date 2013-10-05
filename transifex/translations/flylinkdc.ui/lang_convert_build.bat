call "%VS100COMNTOOLS%\..\..\VC\bin\vcvars32.bat"
cl lang_convert.cpp /EHsc /O2
del *.obj
lang_convert.exe en.xml
lang_convert.exe ru_RU.xml
lang_convert.exe uk_UA.xml
lang_convert.exe fr_FR.xml
lang_convert.exe es_ES.xml
lang_convert.exe be_BY.xml
lang_convert.exe pt_BR.xml