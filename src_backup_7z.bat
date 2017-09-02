
call src_gen_filename.bat -src
7z.exe a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on fly-backup-%FILE_NAME%.7z -i@src_include.txt -x@src_exclude.txt

goto :end
:error
echo Some error occured
pause
:end
