@echo off
rem Parse input parameters.
rem Valid values: norev, nover, nobeta

set NOREV=
set NOVER=
set NOBETA=

set CUR_DIR=%~d0%~p0

:Loop
IF "%1"=="" GOTO Continue
if "%1"=="norev" set NOREV=1
if "%1"=="nover" set NOVER=1
if "%1"=="nobeta" set NOBETA=1
SHIFT
GOTO Loop
:Continue

set REVISION_NUM=
set REVISION_SVN=
set REVISION_NUM_UI=

if "%NOVER%"=="1" goto :SkipVer
set VERSION_NUM=
for /F "tokens=1,2,3 delims= " %%a in ('type "%CUR_DIR%..\revision.h"') do (
if "%%a"=="#define" if "%%b"=="VERSION_NUM" set VERSION_NUM=%%~c
)
if "%VERSION_NUM%"=="" goto :err
:SkipVer

if "%NOBETA%"=="1" goto :SkipBeta
set BETA_STATE=
for /F "tokens=1,2,3 delims= " %%a in ('type "%CUR_DIR%..\revision.h"') do (
if "%%a"=="//#define" if "%%b"=="FLYLINKDC_BETA" goto :SkipBeta
)
for /F "tokens=1,2,3 delims= " %%a in ('type "%CUR_DIR%..\revision.h"') do (
if "%%a"=="#define" if "%%b"=="BETA_NUM" set BETA_STATE=-rc
if "%%a"=="#define" if "%%b"=="BETA_NUM" set BETA_STATE_UI=rc %%~c
)
:SkipBeta

if "%NOREV%"=="1" goto :SkipRev
for /F "tokens=1,2,3 delims= " %%a in ('type "%CUR_DIR%..\revision.h"') do (
if "%%a"=="#define" if "%%b"=="REVISION_NUM" set REVISION_NUM=-build-%%~c
if "%%a"=="#define" if "%%b"=="REVISION_NUM" set REVISION_NUM_UI= build %%~c
if "%%a"=="#define" if "%%b"=="REVISION_NUM" set REVISION_SVN=%%~c
)

if "%REVISION_NUM%"=="-build-" goto :err
:SkipRev

goto :end

:err
echo Error extracting version/revision
exit /b 1

:end
echo Extracted version: %VERSION_NUM%%BETA_STATE%%REVISION_NUM%
exit /b 0