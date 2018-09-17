@echo off
setlocal

set hdrpath=git-version

set remote=origin
set branch=master

for /f "tokens=2" %%i in ('git branch') do set current_branch=%%i
if %current_branch% neq %branch% echo ERROR: Must be on %branch% branch, not %current_branch%. && exit
git fetch
for /f "tokens=1" %%i in ('git rev-parse HEAD') do set head_rev=%%i
for /f "tokens=1" %%i in ('git rev-parse %remote%/%branch%') do set remote_rev=%%i
if %head_rev% neq %remote_rev% echo ERROR: Your branch and '%remote%/%branch%' have diverged. && exit
rem git add %1 && git commit %1 -m %2 && git push %remote% %branch%

echo %head_rev%
echo %remote_rev%

echo %remote%/%branch%

goto :eof


endlocal
