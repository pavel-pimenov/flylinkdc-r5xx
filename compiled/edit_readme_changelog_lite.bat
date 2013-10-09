del PortalBrowser\PortalBrowser*.dll
del FlyUpdate.exe
del UpdateMaker.exe
copy ..\..\flylinkdc-update\5xx\stable-lib-dll\*.dll .\PortalBrowser
copy UpdateTools\FlyUpdate\FlyUpdate.exe .
copy UpdateTools\Updatemaker\UpdateMaker.exe .
notepad ..\changelog-flylinkdc-r5xx-svn.txt
start wordpad readme_changelog_lite.rtf
cd alluser
svn update
cd..
call CreateUpdate.bat
cd alluser
call commit_update.bat
cd..
"C:\Program Files\Araxis\Araxis Merge\compare.exe" .\update ..\..\flylinkdc-update\5xx\beta
explorer ..\..\flylinkdc-update\5xx
