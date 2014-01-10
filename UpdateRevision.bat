@echo off
call svn update

set NOCOMMIT=
set VERSION_NUM=502
set BETA_STATE=
set BETA_NUM=

set RELEASE_UPDATE_FOLDER=".\update\"
set RELEASE_UPDATE_URL="http://www.fly-server.ru/update/5xx/release/"

set BETA_UPDATE_FOLDER=".\update\"
set BETA_UPDATE_URL="http://www.fly-server.ru/update/5xx/beta/"

if "%BETA_STATE%"=="1" (set SYMBOL_SERVER_APP_KEY=910F4B4D-C71C-45BC-A88D-F59FE022525B) else (set SYMBOL_SERVER_APP_KEY=D7F972BA-ACF7-451E-88D5-FF0B98BD085D)
set APP_VERSION_FOR_SYMBOL_SERVER=7.7

:Loop
IF "%1"=="" GOTO Continue
if "%1"=="nocommit" set NOCOMMIT=1
SHIFT
GOTO Loop
:Continue

for /f "tokens=1,2 delims= " %%a in ('svn.exe st -q') do (
if "%%a"=="M" echo "%%b" changed
if "%%a"=="M" echo Commit changes before making distrib!
if "%%a"=="M" rem pause
if "%%a"=="M" if not "%NOCOMMIT%"=="1" exit /b 1
if "%%a"=="M" rem goto :end
)

svn info > nul
if errorlevel 1 goto :OnError

for /F "tokens=1,2 delims=: " %%a in ('svn info') do call :InfoProc "%%a" "%%b"
goto :end

:InfoProc
if %1=="Revision" call :WriteRevision %2
goto :end

:WriteRevision
echo SYMUPLOAD %SYMBOL_SERVER_APP_KEY% %APP_VERSION_FOR_SYMBOL_SERVER%.%VERSION_NUM%.%~1 0 FlylinkDC.pdb> SymRegisterBinaries.bat
echo SYMUPLOAD %SYMBOL_SERVER_APP_KEY% %APP_VERSION_FOR_SYMBOL_SERVER%.%VERSION_NUM%.%~1 0 FlylinkDC.exe>> SymRegisterBinaries.bat
echo SYMUPLOAD %SYMBOL_SERVER_APP_KEY% %APP_VERSION_FOR_SYMBOL_SERVER%.%VERSION_NUM%.%~1 0 FlylinkDC_x64.pdb>> SymRegisterBinaries.bat
echo SYMUPLOAD %SYMBOL_SERVER_APP_KEY% %APP_VERSION_FOR_SYMBOL_SERVER%.%VERSION_NUM%.%~1 0 FlylinkDC_x64.exe>> SymRegisterBinaries.bat
move SymRegisterBinaries.bat "compiled"

if "%BETA_STATE%"=="1" (echo UpdateMaker.exe -V%~1 -D%BETA_UPDATE_FOLDER% -U%BETA_UPDATE_URL% -B -MP -P"5">> CreateUpdate.bat) else (echo UpdateMaker.exe -V%~1 -D%RELEASE_UPDATE_FOLDER% -U%RELEASE_UPDATE_URL% -R -MP -P"5">> CreateUpdate.bat)
if "%RELEASE_UPDATE_FOLDER%"=="%BETA_UPDATE_FOLDER%" echo copy %RELEASE_UPDATE_FOLDER%Update5_beta.* %RELEASE_UPDATE_FOLDER%Update5.*>> CreateUpdate.bat
move CreateUpdate.bat "compiled"

echo #ifndef FLY_REVISION_H> revision.h
echo #define FLY_REVISION_H>> revision.h
echo. >> revision.h
echo #define VERSION_NUM %VERSION_NUM%>> revision.h
echo #define REVISION_NUM %~1>> revision.h
if "%BETA_STATE%"=="1" (echo #define FLYLINKDC_BETA>> revision.h) else (echo //#define FLYLINKDC_BETA>> revision.h)
echo #define BETA_NUM   %BETA_NUM% // Number of beta. Does not matter if the #define FLYLINKDC_BETA is disabled.>> revision.h
echo. >> revision.h
echo #endif>> revision.h

goto :end

:OnError
echo Error get revision from SVN
if not "%1"=="WriteFakeHdrOnError" exit /b 1
if exist revision.h goto end
echo generating fake header

echo #ifndef FLY_REVISION_H > revision.h
echo #define REVISION_NUM 0 >> revision.h
call :WriteBody
goto end

:end