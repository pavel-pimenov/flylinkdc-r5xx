call update_version.bat %1 %2 %3 %4

:Loop
IF "%1"=="" GOTO Continue
if "%1"=="norev" goto :ValidParam
if "%1"=="nover" goto :ValidParam
if "%1"=="nobeta" goto :ValidParam
rem Found parameter that is no "norev" nor "nover" nor "nobeta". It must be filename mask
goto :Continue
:ValidParam
SHIFT
GOTO Loop
:Continue

:ProcessAllScripts
for %%w in (flylinkdc-Install-*.iss) do (
"C:\Program Files (x86)\Inno Setup 5\ISCC.exe" /Q /O"output" /F"%%~nw" "%%w"
)
call clear_installers.bat
cd Output
rem [!] call install-copy-www.bat

goto :end

:err
echo Error get version!!!
pause

:end