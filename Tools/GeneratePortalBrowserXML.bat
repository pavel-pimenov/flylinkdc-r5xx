@echo off

cd "%~d0%~p0"

set MERGE_CMD_LINE=

rem [!] PPA for /f "tokens=1,* delims=:" %%a in ('svn.exe info ..\PortalBrowser\*.xml') do if "%%a"=="Path" call :FormCmdLine "%%b"

XMLMerge.exe%MERGE_CMD_LINE% -o:"..\compiled\PortalBrowser\PortalBrowser.xml"

for /F "tokens=1,* delims= " %%a in ('xmlvalid ..\compiled\PortalBrowser\PortalBrowser.xml /s:..\compiled\PortalBrowser\PortalBrowser.xsd') do (
echo PortalBrowser.xml %%b
if "%%b" == "is XML well-formed and schema valid" exit /b 0
)

exit /b 1

:FormCmdLine
set FILE_NAME=%~1
set FILE_NAME=%FILE_NAME:~1%
set MERGE_CMD_LINE=%MERGE_CMD_LINE% -i:"%FILE_NAME%"
goto :end

:end