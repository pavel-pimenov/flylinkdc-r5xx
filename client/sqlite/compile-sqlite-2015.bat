del *.exe
del *.obj

call "%VS140COMNTOOLS%\..\..\VC\bin\vcvars32.bat"

cl /D _CONSOLE /W4 /O2 shell.c sqlite3.c -Fesqlite3.exe

rem cl.exe /MT /W4 /O2 /D WIN32 /D NDEBUG /D _CONSOLE /D _MBCS /c shell.c sqlite3.c
rem link.exe kernel32.lib user32.lib /subsystem:console /machine:I386 /out:"sqlite3.exe" shell.obj sqlite3.obj
