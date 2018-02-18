
set NOCOMMIT=
set VERSION_NUM=503
set BETA_STATE=
set BETA_NUM=

set START_REV_NUM=19650

echo #ifndef FLY_REVISION_H> revision.h
echo #define FLY_REVISION_H>> revision.h
echo. >> revision.h
echo #define VERSION_NUM %VERSION_NUM%>> revision.h

setlocal EnableDelayedExpansion
for /F "tokens=*" %%a in ('git rev-list HEAD --count') do (
set /a result=%START_REV_NUM%+%%a
echo #define REVISION_NUM !result!>> revision.h
)

for /F "tokens=*" %%a in ('git describe --abbrev"="4  --dirty"="-d') do echo #define GIT_TAG "%%a" >> revision.h
for /F "tokens=*" %%a in ('git log -1 --format"="%%at') do echo #define VERSION_DATE %%a >> revision.h

if "%BETA_STATE%"=="1" (echo #define FLYLINKDC_BETA>> revision.h) else (echo //#define FLYLINKDC_BETA>> revision.h)
echo #define BETA_NUM   %BETA_NUM% // Number of beta. Does not matter if the #define FLYLINKDC_BETA is disabled.>> revision.h
echo. >> revision.h
echo #endif>> revision.h

goto :end

:OnError
echo Error get revision
if not "%1"=="WriteFakeHdrOnError" exit /b 1
if exist revision.h goto end
echo generating fake header

echo #ifndef FLY_REVISION_H > revision.h
echo #define REVISION_NUM 0 >> revision.h
call :WriteBody
goto end

:end