del PortalBrowser\PortalBrowser*.dll
del FlyUpdate.exe
del UpdateMaker.exe

copy FlylinkDC.exe Q:\vc15\r4xx-release\compiled
copy FlylinkDC_x64.exe Q:\vc15\r4xx-release\compiled

copy ..\..\flylinkdc-update\5xx\stable-lib-dll\*.dll .\PortalBrowser
copy UpdateTools\FlyUpdate\FlyUpdate.exe .
copy UpdateTools\Updatemaker\UpdateMaker.exe .
notepad ..\changelog-flylinkdc-r5xx-svn.txt
start /wait wordpad readme_changelog_lite.rtf
cd alluser
svn update
cd..
call CreateUpdate.bat
cd alluser
call commit_update.bat
cd..
"C:\Program Files\Araxis\Araxis Merge\compare.exe" .\update ..\..\flylinkdc-update\5xx\release
copy .\update\Update5.rtf ..\..\flylinkdc-update\5xx\release
copy .\update\FlylinkDC.exe.bz2 ..\..\flylinkdc-update\5xx\release
copy .\update\FlylinkDC_x64.exe.bz2 ..\..\flylinkdc-update\5xx\release
copy .\update\Update5.xml ..\..\flylinkdc-update\5xx\release
copy .\update\Update5.sign ..\..\flylinkdc-update\5xx\release

explorer ..\..\flylinkdc-update\5xx
cd Q:\vc15\r4xx-release\compiled
call edit_readme_changelog_lite.bat
call deploy-update-etc.bat
