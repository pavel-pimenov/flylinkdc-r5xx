del *.sdf
del *.suo
del .\GdiOle\GdiOle_i.c
del .\GdiOle\GdiOle_i.h
del .\GdiOle\GdiOle_p.c
del .\compiled\*.pdb
del .\compiled\*.exp
del .\compiled\FlylinkDC.exe
del .\compiled\FlylinkDC_x64.exe
del /S /F /Q vc17
rmdir /S /Q .\compiled\Settings\BackUp
rmdir /S /Q .\compiled\Settings\FileLists
rmdir /S /Q .\compiled\Settings\Logs
rmdir /S /Q .\compiled\Settings\HubLists
del /S /F /Q mrgtmp0
del /S /F /Q *.tmp
del /S /F /Q *.obj
del /S /F /Q *.ipch
del /S /F /Q *.idb
del /S /F /Q *.iobj
del /S /F /Q *.ipdb
del /S /F /Q *.lib
del /S /F /Q *.tlog
del /S /F /Q *.cpp.orig
del /S /F /Q *.h.orig
del .\compiled\*.ilk
del /S /F /Q ipch\*.ipch
del /S /F /Q compiled\update\*.bz2
del /S /F /Q compiled\update\*.xml
del /S /F /Q compiled\update\*.rtf
del /S /F /Q compiled\update\*.sign
"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\devenv.exe" FlylinkDC_2017.sln /Clean "Release|Win32"
"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\devenv.exe" FlylinkDC_2017.sln /Clean "Release|x64"
