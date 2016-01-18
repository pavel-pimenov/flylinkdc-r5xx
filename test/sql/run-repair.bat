echo PRAGMA integrity_check; | sqlite3.exe users.sqlite
sqlite3.exe users.sqlite .dump > users.sql
sqlite3.exe users-new.sqlite < users.sql
echo PRAGMA integrity_check; | sqlite3.exe users-new.sqlite
ren users.sqlite users-old.sqlite
ren users-new.sqlite users.sqlite
del users.sql
