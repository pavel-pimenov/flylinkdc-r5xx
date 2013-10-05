<?php
/* Скрипт определения внутреннего IP адреса пользователя для настройки FlylinkDC++

 Форма запроса:
 http://<сайт>/getip.php
 Автор: SkazochNik (6.01.07) skazochnik97@mail.ru
 Версия от 6.12.2008
*/

ignore_user_abort(1);
$host = $_SERVER['REMOTE_ADDR'];

print "<html><head><title>Current IP Check</title></head><body>Current IP Address: $host</body></html>
";
