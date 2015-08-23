if not exist tx.exe goto :GETTX
goto :END
:GETTX
wget --tries=999 -c -t1 --no-check-certificate http://files.transifex.com/transifex-client/0.9/tx.exe
:END
tx.exe pull -s -l ru_RU,uk_UA,fr_FR,be_BY,es_ES,lv_LV,it_IT,pl_PL,de_DE,zh_CN,pt_BR
rem --mode reviewed