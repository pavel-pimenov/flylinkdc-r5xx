del /S /F /Q *.obj
del /S /F /Q *.pdb
del /S /F /Q *.lib
del /S /F /Q tmp32dll\*
del /S /F /Q out32dll\*

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

set PATH=%PATH%;"C:\Program Files (x86)\Windows Kits\8.1\bin\x86"
rem https://github.com/Microsoft/vcpkg/issues/1689

perl Configure VC-WIN64A no-asm no-shared --prefix=c:\openssl-1-1-1
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Tools\MSVC\14.16.27023\bin\Hostx86\x86\nmake.exe" -f makefile