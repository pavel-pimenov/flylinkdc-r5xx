call "%VS140COMNTOOLS%....\VC\bin\vcvars32.bat"
rem SET CL=/DZENLIB_DEBUG
"%VS140COMNTOOLS%..\ide\devenv" MediaInfoLib_2015.vcxproj /Build "Debug|Win32"