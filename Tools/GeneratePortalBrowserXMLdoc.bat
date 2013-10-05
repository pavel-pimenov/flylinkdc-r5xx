SETLOCAL ENABLEDELAYEDEXPANSION
set START_DIR=%~d0%~p0%~nx0
set START_DIR=!START_DIR:%~0=!
echo %START_DIR%

cd "%~d0%~p0"
msxsl ..\compiled\PortalBrowser\PortalBrowser.xsd XMLdocTemplate.xsl -t -o ..\compiled\PortalBrowser\XMLformatDoc.htm
cd %START_DIR%