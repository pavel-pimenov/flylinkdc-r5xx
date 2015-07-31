del .\compiled\FlylinkDC.exe
del .\compiled\FlylinkDC.pdb
call UpdateRevision.bat %1 %2 %3 %4
if errorlevel 1 goto :error

call tools\ExtractVersion.bat %1 %2 %3 %4
if errorlevel 1 goto :error

call "%VS140COMNTOOLS%\..\..\VC\bin\vcvars32.bat"
"%VS140COMNTOOLS%..\ide\devenv" FlylinkDC_2015.sln /Rebuild "Release|Win32"

if not exist .\compiled\FlylinkDC.exe goto :builderror

"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Bin\signtool.exe" sign /a /d "FlylinkDC++ r5xx" /du "http://flylinkdc.blogspot.com" /t http://timestamp.verisign.com/scripts/timstamp.dll ".\compiled\FlylinkDC.exe"

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
goto :end

:builderror
echo Compilation error. Building terminated.
pause

:end
