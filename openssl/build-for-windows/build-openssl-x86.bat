rem ====================================================
rem http://www.nasm.us/pub/nasm/releasebuilds/2.10/win32/
rem call C:\nasm\nasmpath.bat
rem ====================================================
del /S /F /Q *.obj
del /S /F /Q *.lib
del /S /F /Q tmp32dll\*
del /S /F /Q out32dll\*

rem call "%VS140COMNTOOLS%\..\..\VC\bin\vcvars32.bat"
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

perl Configure VC-WIN32 no-asm no-shared --prefix=c:\openssl-build-x86
call ms\do_ms
"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\nmake.exe" -f ms\nt.mak


