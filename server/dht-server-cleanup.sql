delete from dhtInfo where dht_time < '2013-06-01';
update dhtInfo set udp = 6250 where udp = 0;
