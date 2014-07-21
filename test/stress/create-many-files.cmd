@echo off 
setlocal enableDelayedExpansion On

rem =======================================
rem ==================OPTIONS==============
set /a FILE_SIZE=1100
set /a NUM_FILES=1000
set DIR_NAME=1000_files
rem ==================OPTIONS==============
rem =======================================

IF NOT EXIST %DIR_NAME% ( MKDIR %DIR_NAME% )
cd %DIR_NAME%

set /a max_rnd = %FILE_SIZE% / 70
rem goto :main
FOR /l %%j in ( 1, 1, %NUM_FILES% ) do (
	call :func_make_rndstr
	call :create_file %%j
)
goto :exit

:func_make_rndstr
set rndstr=RNDSTR
FOR /l %%x in ( 1 , 1, %max_rnd% ) do (
	CALL :concat %%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%%%random%%
)
goto :eof

:create_file
	echo file %1
	echo %rndstr% > %random%%random%%random%%random%.txt
goto :eof

:concat
set rndstr=%rndstr%%1
rem echo %rndstr%
goto :eof

:exit