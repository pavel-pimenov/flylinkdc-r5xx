call clear_installers.bat
call update_version.bat %1 %2 %3 %4

cd innosetup_lang
call get_lang.bat
cd..

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

md "output\r%VERSION_NUM%%BETA_STATE%%REVISION_NUM%"

if "%1"=="" goto :ProcessAllScripts
for %%w in (%1) do (
"C:\Program Files (x86)\Inno Setup 5\ISCC.exe" /Q /O"output\r%VERSION_NUM%%BETA_STATE%%REVISION_NUM%" /F"Setup%%~nw-r%VERSION_NUM%%BETA_STATE%%REVISION_NUM%" "%%w"
)
goto :end

:ProcessAllScripts
for %%w in (*.iss) do (
"C:\Program Files (x86)\Inno Setup 5\ISCC.exe" /Q /O"output\r%VERSION_NUM%%BETA_STATE%%REVISION_NUM%" /F"Setup%%~nw-r%VERSION_NUM%%BETA_STATE%%REVISION_NUM%" "%%w"
)

goto :end

:err
echo Error get version!!!
pause

:end