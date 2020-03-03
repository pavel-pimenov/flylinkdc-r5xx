rem wget -N --tries=999 http://geolite.maxmind.com/download/geoip/database/GeoIPCountryCSV.zip
rem 7z.exe x GeoIPCountryCSV.zip -y
rem del GeoIPCountryCSV.zip
rem copy GeoIPCountryWhois.csv ..
rem del CustomLocations.7z
rem wget -t1 --tries=999 http://nightorion.dyndns.ws/CustomLocations.7z
svn up
rem 7z.exe x CustomLocations.7z -y
copy CustomLocations.bmp ..
copy CustomLocations.ini ..
svn commit --file customlocations.txt
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
rem wget -N --tries=999 "http://list.iblocklist.com/?list=dufcxgnbjsdwmwctgfuj&fileformat=p2p&archiveformat=7z"
rem 7z.exe x dufcxgnbjsdwmwctgfuj.7z -y
rem ren dufcxgnbjsdwmwctgfuj.txt iblocklist-com.ini
rem copy iblocklist-com.ini ..

