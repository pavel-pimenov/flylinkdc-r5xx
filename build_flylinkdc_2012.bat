del .\compiled\FlylinkDC.exe
del .\compiled\FlylinkDC.pdb
copy .\crash-server\crshhndl-x86.dll .\compiled

call UpdateRevision.bat %1 %2 %3 %4
if errorlevel 1 goto :error

call tools\ExtractVersion.bat %1 %2 %3 %4
if errorlevel 1 goto :error

call "%VS110COMNTOOLS%\..\..\VC\bin\vcvars32.bat"
"%VS110COMNTOOLS%..\ide\devenv" FlylinkDC_2012.sln /Rebuild "ReleaseFullOpt|Win32"

call Tools\MakePortalBrowserFileList.bat
call src_gen_filename.bat -x86
7z.exe a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on -x@src_exclude_hard.txt -i@PortalBrowserFileList.txt -ir@src_include_bin_x86.txt %FILE_NAME%.7z compiled/FlylinkDC.exe 

svn log > changelog-flylinkdc-r5xx-svn.txt

for /l %%i in (1,1,20) do echo @tools\replace_str.vbs " | %%i lines" "" changelog-flylinkdc-r5xx-svn.txt

call src_gen_filename.bat -src
7z.exe a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on %FILE_NAME%.7z -i@src_include.txt -x@src_exclude.txt -i@PortalBrowserFileList.txt -x!./setup/*

goto :end

:error
echo Can't update/extract version/revision
pause

:end
