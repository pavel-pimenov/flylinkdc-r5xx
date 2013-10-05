cd compiled\Lang
rem ! call update_lang_from_site.bat
cd ..
cd ..
call clean_all_2012.bat
call UpdateGeoIP.bat
call build_flylinkdc_2012.bat
call build_flylinkdc_x64_2012.bat

move *-src-*.7z "U:\webdav\src-bin-pdb\flylinkdc-r5xx"
move *.7z "U:\webdav\src-bin-pdb\flylinkdc-r5xx"

copy changelog-flylinkdc*.txt "U:\webdav\src-bin-pdb"

cd setup 
call build_setup_base.bat
