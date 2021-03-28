del .\compiled\FlylinkDC_x64.exe
del .\compiled\FlylinkDC_x64.pdb
call update_revision.bat %1 %2 %3 %4
if errorlevel 1 goto :error

call tools\ExtractVersion.bat %1 %2 %3 %4
if errorlevel 1 goto :error

rem call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

chcp 437
rem "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" FlylinkDC_2017.sln /m /t:Rebuild /p:COnfiguration="Release" /p:Platform="x64"
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" FlylinkDC_2017.sln /m /t:Rebuild /p:COnfiguration="Release" /p:Platform="x64"

if not exist .\compiled\FlylinkDC_x64.exe goto :builderror

"C:\Program Files (x86)\Windows Kits\10\bin\10.0.17134.0\x86\signtool.exe" sign /v /d "FlylinkDC++ r5xx" /du "http://flylinkdc.blogspot.com" /fd sha1 /t http://timestamp.verisign.com/scripts/timstamp.dll ".\compiled\FlylinkDC_x64.exe"
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.17134.0\x86\signtool.exe" sign /as /v /d "FlylinkDC++ r5xx" /du "http://flylinkdc.blogspot.com" /fd sha256 /tr http://timestamp.comodoca.com/rfc3161 /td sha256 ".\compiled\FlylinkDC_x64.exe"

call src_gen_filename.bat -x64
7z.exe a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on -x@src_exclude_hard.txt  -ir@src_include_bin_x64.txt %FILE_NAME%.7z compiled/FlylinkDC_x64.exe 

svn log > changelog-flylinkdc-r5xx-x64-svn.txt

for /l %%i in (1,1,20) do echo @tools\replace_str.vbs " | %%i lines" "" changelog-flylinkdc-r5xx-x64-svn.txt

call src_gen_filename.bat -debug-info
7z.exe a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on -x@src_exclude_hard.txt %FILE_NAME%.7z *.pdb

call src_gen_filename.bat -src
7z.exe a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on %FILE_NAME%.7z -i@src_include.txt -x@src_exclude.txt -x!./setup/*

goto :end

:error
echo Can't update/extract version/revision
pause
goto :end

:builderror
echo Compilation error. Building terminated.
pause

:end
