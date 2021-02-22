wget -N --tries=999 http://upd.emule-security.org/ipfilter.zip
7z.exe x ipfilter.zip -y
del ipfilter.zip
del P2PGuard.ini
ren guarding.p2p P2PGuard.ini
call p2pguard_convert_build.bat
p2pguard_convert.exe
del P2PGuard.ini
ren P2PGuard_flylink.ini P2PGuard.ini
copy P2PGuard.ini ..
