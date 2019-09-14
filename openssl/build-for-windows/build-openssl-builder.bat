rem ====================================================
rem http://www.nasm.us/pub/nasm/releasebuilds/2.10/win32/
rem call C:\nasm\nasmpath.bat
rem ====================================================
del /S /F /Q *.obj
del /S /F /Q *.lib
del /S /F /Q tmp32dll\*
del /S /F /Q out32dll\*


perl Configure BC-32 no-asm no-shared --prefix=c:\openssl-build-x86
perl util\mk1mf.pl no-asm no-shared BC-NT > bcb.mak
make -f bcb.mak


