rem VS150COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build
rem C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\vsdevcmd.bat"
cl.exe 2> visual-c-version.txt
rem %comspec% /k "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat"
