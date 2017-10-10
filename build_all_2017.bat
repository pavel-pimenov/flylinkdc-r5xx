call clean_all_2017.bat
call update_geo_ip.bat
call build_flylinkdc_2017.bat
call build_flylinkdc_x64_2017.bat

if not exist .\compiled\FlylinkDC*.exe goto :builderror

copy FlylinkDC-*-x64-*build-*-*.7z "D:\ppa-doc\mailru\flylinkdc\x64"
copy FlylinkDC-*-x86-*build-*-*.7z "D:\ppa-doc\mailru\flylinkdc\x86"

move *-debug-info-*.7z "U:\webdav\src-bin-pdb\flylinkdc-r5xx-pdb"
move *-src-*.7z "U:\webdav\src-bin-pdb\flylinkdc-r5xx\r504"
move *.7z "U:\webdav\src-bin-pdb\flylinkdc-r5xx\r504"

rem copy changelog-flylinkdc*.txt "U:\webdav\src-bin-pdb"

rem copy ..\flylinkdc-update\5xx\stable-lib-dll\*.dll .\compiled\PortalBrowser

cd setup 
rem call build_setup_base.bat
cd ..
cd compiled
call SymRegisterBinaries.bat
goto:end

:builderror
echo Compilation error. Building terminated.
pause

:end
