call "%VS150COMNTOOLS%....\VC\bin\vcvars32.bat"
rem SET CL=/DZENLIB_DEBUG
"%VS150COMNTOOLS%..\ide\devenv" MediaInfoLib_2017.vcxproj /Build "Debug|Win32"