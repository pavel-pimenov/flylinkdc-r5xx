cd translations
cd flylinkdc.UI
call lang_convert_build

copy new\en.xml ..\..\..\compiled\Lang\en-US.xml
copy new\??_??.xml ..\..\..\compiled\Lang\??-??.xml
rem copy translations\FlylinkDC.custom_messages-\be_BY.ini ..\setup\custom_messages-BE-BY.isl
rem copy translations\FlylinkDC.custom_messages-\es_ES.ini ..\setup\custom_messages-ES-ES.isl
rem copy translations\FlylinkDC.custom_messages-\fr_FR.ini ..\setup\custom_messages-FR-FR.isl
rem copy translations\FlylinkDC.custom_messages-\ru_RU.ini ..\setup\custom_messages-RU-RU.isl
rem copy translations\FlylinkDC.custom_messages-\uk_UA.ini ..\setup\custom_messages-UK-UA.isl
