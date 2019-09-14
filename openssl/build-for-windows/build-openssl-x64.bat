rem ====================================================
rem http://www.nasm.us/pub/nasm/releasebuilds/2.10/win32/
rem call C:\nasm\nasmpath.bat
rem ====================================================
del /S /F /Q *.obj
del /S /F /Q *.lib
del /S /F /Q tmp32dll\*
del /S /F /Q out32dll\*

call "%VS140COMNTOOLS%\..\..\VC\bin\amd64\vcvars64.bat"
perl Configure VC-WIN64A no-asm no-shared --prefix=c:\openssl-build-x64
call ms\do_win64a
nmake.exe -f ms\nt.mak


