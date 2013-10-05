SETLOCAL ENABLEDELAYEDEXPANSION
set START_DIR=%~d0%~p0%~nx0
set START_DIR=!START_DIR:%~0=!
echo %START_DIR%

cd "%~d0%~p0"

astyle --indent=tab=4 --brackets=break --indent-classes --indent-switches --indent-preprocessor --break-closing-brackets --pad-oper --pad-header --unpad-paren --convert-tabs --fill-empty-lines --max-instatement-indent=79 --suffix=none ..\client\*.cpp ..\client\*.h ..\windows\*.cpp ..\windows\*.h ..\PortalBrowser\*.cpp ..\PortalBrowser\*.h ..\XMLParser\*.cpp ..\XMLParser\*.h ..\XMLMerge\*.cpp ..\XMLMerge\*.h ..\GdiOle\*.cpp ..\GdiOle\*.h ..\dht\*.cpp ..\dht\*.h ..\FlyFeatures\*.h ..\FlylinkExt\*.h ..\flyupdate\*.h ..\UpdateMaker\*.h ..\FlyFeatures\*.cpp ..\FlylinkExt\*.cpp ..\flyupdate\*.cpp ..\UpdateMaker\*.cpp ..\test\*.cpp

cd %START_DIR%