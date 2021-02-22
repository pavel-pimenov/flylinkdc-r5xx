echo on
call clean_all_2017.bat
call update_geo_ip.bat
call build_flylinkdc_x64_2017.bat

if not exist .\compiled\FlylinkDC*.exe goto :builderror

copy FlylinkDC-*-x64-*build-*-*.7z "D:\ppa-doc\mailru\flylinkdc\x64"

move *-debug-info-*.7z "D:\ppa-doc\mailru\flylinkdc-src"
move *-src-*.7z "D:\ppa-doc\mailru\flylinkdc-src"
move *.7z "D:\ppa-doc\mailru\flylinkdc-src"

cd compiled
call SymRegisterBinaries.bat
goto:end

:builderror
echo Compilation error. Building terminated.
pause

:end
