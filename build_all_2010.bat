echo Use VC+2013
goto :builderror

call clean_all_2010.bat
call update_geo_ip.bat
call build_flylinkdc_2010.bat
call build_flylinkdc_x64_2010.bat

if not exist .\compiled\FlylinkDC*.exe goto :builderror

move *-src-*.7z "U:\webdav\src-bin-pdb\flylinkdc-r5xx\r503"
move *.7z "U:\webdav\src-bin-pdb\flylinkdc-r5xx\r503"

rem copy changelog-flylinkdc*.txt "U:\webdav\src-bin-pdb"

copy ..\flylinkdc-update\5xx\stable-lib-dll\*.dll .\compiled\PortalBrowser

cd setup 
call build_setup_base.bat
cd ..
cd compiled
call SymRegisterBinaries.bat
goto:end

:builderror
echo Compilation error. Building terminated.
pause

:end
