@echo off 

rem =======================================
rem ==================OPTIONS==============
set NUM_DIRS=1000
set FILE_SIZE=1100
rem ==================OPTIONS==============
rem =======================================

setlocal enableDelayedExpansion

set DIR_NAME=%NUM_DIRS%_dirs
echo Directory: %DIR_NAME%
set dcount=0
IF NOT EXIST %DIR_NAME% ( MKDIR %DIR_NAME% ) ELSE ( 
	cd %DIR_NAME% && cd
	FOR /D %%a in (*) DO set /a dcount+=1 
	ECHO Warning. have !dcount! dirs already.
	)

set /a max_rnd = %FILE_SIZE% / 70
rem goto :main
FOR /l %%j in ( 1, 1, %NUM_DIRS% ) do (
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
	set rdir=%random%%random%%random%%random%
	IF NOT EXIST %rdir% ( 
		echo Dir:%1 %rdir%
		mkdir %rdir%
		echo %rndstr% > %rdir%\%rdir%.dat
	)
goto :eof

:concat
set rndstr=%rndstr%%1
rem echo %rndstr%
goto :eof

:exit
echo Directory: %DIR_NAME%
echo Done!
pause