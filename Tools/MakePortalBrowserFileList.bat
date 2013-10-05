set DIR_ATTRIB=

SETLOCAL ENABLEDELAYEDEXPANSION
set START_DIR=%~d0%~p0%~nx0
set START_DIR=!START_DIR:%~0=!
echo %START_DIR%

rem Write to list predefined files and Portals' resources lied under SVN
del PortalBrowserFileList.txt

if "%1" == "x64" goto :do64
echo compiled\PortalBrowser\PortalBrowser.dll>>PortalBrowserFileList.txt

if not "%1" == "x64" goto :do_rest
:do64
echo compiled\PortalBrowser\PortalBrowser_x64.dll>>PortalBrowserFileList.txt

:do_rest
echo compiled\PortalBrowser\XMLformatDoc.htm>>PortalBrowserFileList.txt

cd "%~d0%~p0\..\compiled\portalbrowser"
for /f "tokens=4 delims= " %%a in ('svn.exe st -v') do (
echo %%a
set DIR_ATTRIB=%%~aa
set DIR_ATTRIB=!DIR_ATTRIB:~0,1!
if "!DIR_ATTRIB!"=="-" echo compiled\PortalBrowser\%%a>>..\..\PortalBrowserFileList.txt
)


cd "%~d0%~p0

rem Make PortalBrowser XML documentation
call GeneratePortalBrowserXMLdoc.bat

cd %START_DIR%