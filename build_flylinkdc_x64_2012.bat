del .\compiled\FlylinkDC_x64.exe
del .\compiled\FlylinkDC_x64.pdb
copy .\crash-server\crshhndl-x64.dll .\compiled

rem call UpdateRevision.bat %1 %2 %3 %4
if errorlevel 1 goto :error

call tools\ExtractVersion.bat %1 %2 %3 %4
if errorlevel 1 goto :error

call "%VS110COMNTOOLS%\..\..\VC\bin\amd64\vcvars64.bat"
"%VS110COMNTOOLS%..\ide\devenv" FlylinkDC_2012.sln /Rebuild "Release|x64"

call Tools\MakePortalBrowserFileList.bat x64
call src_gen_filename.bat -x64
7z.exe a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on -x@src_exclude_hard.txt -i@PortalBrowserFileList.txt -ir@src_include_bin_x64.txt %FILE_NAME%.7z compiled/FlylinkDC_x64.exe 

call src_gen_filename.bat -debug-info
7z.exe a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on -x@src_exclude_hard.txt %FILE_NAME%.7z *.pdb

goto :end

:error
echo Can't update/extract version/revision
pause

:end
