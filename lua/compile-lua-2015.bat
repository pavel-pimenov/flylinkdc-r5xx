del *.exe
del *.obj

call "%VS140COMNTOOLS%\..\..\VC\bin\vcvars32.bat"

cl /D _CONSOLE /W4 /O2  -Flua53.exe
