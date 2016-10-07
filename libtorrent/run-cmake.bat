rem set OPENSSL_ROOT_DIR=C:/openssl-build-x86
rem set OPENSSL_INCLUDE_DIR=C:/openssl-build-x86/inc32
rem set OPENSSL_LIBRARIES=C:/openssl-build-x86/out32

cmake -G "Visual Studio 14 2015" -DBUILD_TESTING=OFF -DOPENSSL_ROOT_DIR=C:/openssl-build-x86 -DOPENSSL_INCLUDE_DIR=C:/openssl-build-x86/inc32 -DOPENSSL_LIBRARIES=C:/openssl-build-x86/out32 -DBOOST_ROOT=D:\boost_1_61_0 -DBOOST_LIBRARYDIR=D:\boost_1_61_0\lib

rem ubuntu
rem sudo apt-get install libboost-system-dev libssl-dev automake autoconf libtool libboost-tools-dev
rem ./autotool.sh
rem ./configure --enable-tests --enable-examples --enable-debug=yes