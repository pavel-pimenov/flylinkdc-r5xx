.echo ON
--PRAGMA recursive_triggers = true;
--.stats ON

EXPLAIN QUERY PLAN UPDATE userinfo set last_updated = DATETIME('now'), ip_address == 'ip', share = 'share', description = 'description', tag = 'tag', connection = 'connection', email = 'email' where nick = 'nick1';
EXPLAIN QUERY PLAN UPDATE userinfo set last_updated = DATETIME('now'), ip_address == 'ip', share = 'share', description = 'description', tag = 'tag', connection = 'connection', email = 'email' where LOWER(nick) = 'nick1';
 