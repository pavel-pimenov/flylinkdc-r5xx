del p2pguard_convert.exe
call "%VS100COMNTOOLS%\..\..\VC\bin\vcvars32.bat"
cl p2pguard_convert.cpp /EHsc /O2 -I Q:..\..\..\boost
