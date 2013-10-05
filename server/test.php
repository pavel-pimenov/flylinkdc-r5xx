<?php

/* Сценарий тестирования портов для программы FlylinkDC++
 * Формат запроса:
 * 
 * http://<сайт>/test.php?port_IP=16211&port_PI=16212&ver=192
 * где
 * - port_IP - номер тестируемого TCP-порта, например, 16211
 * - port_PI - номер тестируемого UDP-порта, например, 16212
 * - ver - версия программы или что-то еще
 * 
 * В случае, если параметр ver не указан, будет проведена проверка
 * только TCP порта.
 * 
 * Тест udp не будет проходить при прямом соединении! Только в случае
 * модема/роутера/фаервола либо NAT.
 * 
 * Автор: SkazochNik (11.01.07) skazochnik97@mail.ru
 * Правил: a.rainman
 * Правил: JhaoDa
 * 
 * Правка от 04.06.07
 * [-] Убрал запись в counter.dat (кстати, зачем она была нужна?)
 * [*] Пофиксил системный вывод ошибок для fsockopen, теперь
 * ошибку ловим только переменными
 * 
 * Правка от 6.12.2008
 * [*] Пофиксил мелкие баги
 * [*] Вытащил HTML из кода
 * [*] Перевел кодировку на Unicode
 * [+] Страница теперь не попадает в кэш
 * 
 * Правка от 16.12.2008
 * [+] Добавил удобные настройки для конечного и начального порта
 * [*] Привел кое-где в соответствие к стандартам
 * [*] Устранил косяки с кодировкой
 * 
 * Правка от 4.01.2009
 * [*] Исправил ошибку в слове "кое где" на кое-где" %)
 * [*] Убрал хэдер "text/html charset=utf-8" в по умолчанию, ибо IE7 такого не знает
 * [!] Валидацию проходит успешно (Level of HTML: XHTML 1.1)
 * [+] Добавил флаг показа ошибки сервера
 * [*] Поправил баг с тестом UDP, когда TCP не прошел тест (нашел polet2000)
 * [!] Добавил в описание "Тест udp не будет проходить при прямом соединении! Только в случае
 *     модема/роутера/фаервола либо NAT."
 * 
 * Правка от 21.04.2011
 * [*] Уменьшил таймаут ожидания ответа в 25 раз
 * [*] Обновил ссылки и включил игнорирование выполнения скрипта при отключении пользователя
 * 
 * Правка от 17.07.2011
 * [+] добавлен флаг &result=xml для вывода отчета мастеру настроек via Sergey Stolper
 *
 * Правка от 28.12.2012
 * [*] Удалён xml-пролог формируемого документа, из-за которого документ не понимал IE7
 * [*] Вернул Content-Type: text/html; charset=urf-8
 * [*] Изменен доктайп, выкинуты meta-тэги запрета кэширования (это делают заголовки)
 */

// Настройка
$log_sc = 0; // Вести лог (0 - нет, 1 - да) Отключен по умолчанию
$port_start = 1023; // Начальный возможный порт
$port_end = 65536; // Конечный возможный порт
$print_err_mess = 0; // Флаг показа ошибки сервера (0 - не показывать, 1 - показывать) Отключен по умолчанию
$socket_timeout = 0.1; // Таймаут соединения
$scriptver = "28.12.2012"; // Версия
$error_mess ="";	// Готовим строку под описание ошибок.

// Дальше лучше ничего не трогать
ignore_user_abort(0); // не продолжать работу скрипта, даже если это никому не надо
$xml_output_buffer = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<testResult>\n"; // отдельный буфер для создания xml'я

if (!isset($_POST["port"])) {
	$port = null;
} else {
	$port = $_POST["port"];
}
// Придумаем переменную, если ее нет. Совместимость между версиями PHP (?)
if (!isset($PHP_SELF)){
	$PHP_SELF = basename($_SERVER["PHP_SELF"]);
}

if (!isset($_POST["ver"])) {
	$ver = null;
} else {
	$ver = $_POST["ver"];
}

if(!isset($_POST["port"]) && isset($_GET["ver"])){
	$ver = $_GET["ver"];
	$port = $_GET["port_IP"];
	$port_udp = $_GET["port_PI"];
}
ob_start();
$host = "[{$_SERVER['REMOTE_ADDR']}]";
header('Content-Type: text/html; charset=urf-8');
header("Cache-Control: no-cache, must-revalidate"); // HTTP/1.1
header("Expires: Mon, 26 Jul 1997 05:00:00 GMT"); // Дата из прошлого

echo <<<END
<!DOCTYPE html>
<html>
<head>
<title>Тест портов для программы FlylinkDC++</title>
<meta charset="utf-8" />
<style type="text/css">
<!--
body {
	background-color: #fff;
	margin-top: 40px;
}
table {
	font-family: Verdana, Arial, Helvetica, sans-serif;
	font-size: 11px;
	margin-left: auto;
	margin-right: auto;
}
td {
	padding: 1px;
}
button {
	font-family: Verdana, Arial, Helvetica, sans-serif;
	font-size: 11px;
}
a {
	color: #000;
}
.message_alert {
	color: #990000;
}
.message_grant {
	color: #009900;
}
.red_box {
	height: 8px;
	width: 18px;
	margin-right: 5px;
	font-size: 10px;
	background-color: #990000;
}
.mess_row {
	background-color: #eee;
	font-weight: bold;
}
.copyright {
	background-color: #eee;
}
-->
</style>
</head>
<body>
<table width="600"><tr><td>
END;

if (isset($port) && intval($port) > $port_start && intval($port) < $port_end)// && $port != $port_udp)
{

if (!isset($ver)){
	echo <<<END
	<p>Идет тест для <strong>$host</strong> порт TCP <strong>$port</strong> ...</p>
END;
} else {
	echo <<<END
	<p>Идет тест для <strong>$host</strong> порт TCP <strong>$port</strong> порт UDP <strong>$port_udp</strong> ...</p>
END;
}

if (!@fsockopen ($host, $port, $errno, $errstr, $socket_timeout))
{
		if ($errno=="60" || $errno=="10060"){
			$error_mess = "TCP не прошел проверку. Конечный компьютер не ответил на соединение. Может следует <a href=\"http://flylinkdc.ru/portforward.htm\" target=\"_blank\">почитать</a> как настроить портфорвардиг?";
		}
		if ($errno=="10061" || $errno=="61" || $errno=="111"){
			$error_mess = "TCP не прошел проверку. Соединение отклонено. Может следует <a href=\"http://flylinkdc.com/doku.php?id=ru:portforward\" target=\"_blank\">почитать</a> как настроить портфорвардиг?";
		}

		if ($print_err_mess && (int)$errno > 0) {
			$error_mess .= "<br>Server Error: <p>$errno - $errstr</p>";
		}
		echo <<<END
		</td></tr><tr><td class="mess_row"><strong>Тест TCP:</strong><br /><span class="red_box">&nbsp;&nbsp;&nbsp;</span><span class="message_alert">$error_mess</span>
END;
		$isok = $errno; // флаг состояния
		$xml_output_buffer .= "<tcp>false</tcp>\n";
	} else {
		echo <<<END
		</td></tr><tr><td class="mess_row"><strong>Тест TCP:</strong><br /><span class="red_box">&nbsp;&nbsp;&nbsp;</span><span class="message_grant">Проверка TCP-порта пройдена.</span>
END;
		$isok = "ok";
		$xml_output_buffer .= "<tcp>ok</tcp>\n";
	}

/*	Идет запрос из программы
	Проверяем порт udp
*/
if (isset($ver)) {
$fa = @fsockopen ($host, $port_udp, $errnov, $errstrv, $socket_timeout); # Правка от 04.06.07

	if ($errnov=="60" || $errnov=="0") {
		$error_mess = "<span class=\"message_grant\">Очевидно предположить, что UDP-порт настроен верно. Поздравляю :)</span>";
		$xml_output_buffer .= "<udp>ok</udp>\n";
	}
	if ($errnov=="61" || $errnov=="111" || $errnov== "110") {
		$error_mess = "<span class=\"message_alert\"><font color=\"#990000\">UDP не прошел проверку. Может следует <a href=\"http://flylinkdc.com/doku.php?id=ru:portforward\" target=\"_blank\">почитать</a> как настроить портфорвардиг? Хотя если соединение прямое, то это сообщение нормально и для правильно настроенного порта.</span>";
		$xml_output_buffer .= "<udp>false</udp>\n";
	}

	$xml_output_buffer .= "</testResult>";
	if(isset($_GET["result"])){ob_end_clean();die($xml_output_buffer);}
if($isok == "ok") {

	if ($print_err_mess) {
		$error_mess .= "<p>$errnov - $errstrv</p>";
	}

	echo <<<END
	<br />Teст UDP:<br />
	<span class="red_box">&nbsp;&nbsp;&nbsp;</span>$error_mess</td></tr>
	<tr><td><br />
END;
	} else {
	echo <<<END
	<br />Teст UDP:<br />
	<span class="red_box">&nbsp;&nbsp;&nbsp;</span>Нет смысла проводить тест UDP.</td></tr>
	<tr><td><br />
END;
}
	}

echo <<<END
	</td></tr>
	<tr><td>
	<p>Что можно еще сделать:</p>
	<ul>
		<li><a href="javascript: window.close();">Закрыть страницу</a></li>
		<li><a href="http://flylinkdc.ru/portforward.htm" target="_blank">Почитать о портфорвардинге</a></li>
		<li><a href="$PHP_SELF">Вернуться назад</a></li>
	</ul>
	<br />
END;
	if ($log_sc) {
		file_put_contents("test.log", "$host:$port:$port_udp:$ver:$isok\n", FILE_APPEND);
	}
}

if(!intval($port)>$port_start && !intval($port)<$port_end || intval($port)=="" && isset($port))
{
echo <<<END
	<span style="color:#FF0000;"><strong>Не выполнено:</strong> не введен номер порта.</span>
END;
}

if (!isset($ver)){
echo <<<END
<br />Введите номер TCP порта для проверки ($port_start &#8249; PORT &#8249; $port_end):
<form action="$PHP_SELF" method="post">
	<table width="100%">
	<tr><td><b>Порт:</b></td>
	<td><input type="text" name="port" value="$port" maxlength="5" /></td></tr>
	<tr><td></td><td><input type="submit" name="Submit" value="Нажать" /></td></tr>
	</table>
</form>
END;
}

if ($port == $port_udp & 0) {
echo <<<END
</td></tr><tr><td class="mess_row"><div style="margin: 0 auto; width: 270px;"><span class="message_alert"><font color="#990000">Порты TCP и UDP не должны совпадать.</font></span></div>
END;
} else {
echo <<<END
</td></tr><tr><td class="copyright"><strong>Кодинг:</strong> SkazochNik (skazochnik9&#55;&#64;mail&#46;ru)<br /><strong>Мысли:</strong> PPA (p&#97;vel.pimenov&#64;gmail&#46;com) <br /><strong>Версия от:</strong> $scriptver</td></tr></table>
</body></html>
END;
}

ob_end_flush();
?>