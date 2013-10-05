call ..\Tools\ExtractVersion.bat %1 %2 %3 %4
if errorlevel 1 goto :err

rem √енерируем версию дл€ файлов инсталл€ции
echo VersionInfoVersion=7.7.%VERSION_NUM%.%REVISION_SVN% > version_info.txt
echo AppVersion= 7.7.%VERSION_NUM%.%REVISION_SVN% %BETA_STATE_UI%>>version_info.txt

echo Result:='FlylinkDC++ r%VERSION_NUM%%BETA_STATE%%REVISION_NUM_UI%'; > version.txt
echo Result:='FlylinkDC++ r%VERSION_NUM%%BETA_STATE%-x64%REVISION_NUM_UI%'; > version-x64.txt

goto :end

:err
pause

:end