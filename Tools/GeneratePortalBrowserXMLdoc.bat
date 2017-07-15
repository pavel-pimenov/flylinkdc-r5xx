rem SETLOCAL ENABLEDELAYEDEXPANSION
rem set START_DIR=%~d0%~p0%~nx0
rem set START_DIR=!START_DIR:%~0=!
rem echo %START_DIR%
rem cd "%~d0%~p0"
rem msxsl ..\compiled\PortalBrowser\PortalBrowser.xsd XMLdocTemplate.xsl -t -o ..\compiled\PortalBrowser\XMLformatDoc.htm
rem cd %START_DIR%