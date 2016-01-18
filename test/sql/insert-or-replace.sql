.echo ON
--PRAGMA recursive_triggers = true;

CREATE TABLE IF NOT EXISTS userinfo (nick VARCHAR(64) NOT NULL,last_updated DATETIME NOT NULL,ip_address VARCHAR(39) NOT NULL,share VARCHAR(24) NOT NULL,description VARCHAR(192),tag VARCHAR(192),connection VARCHAR(32),email VARCHAR(96),UNIQUE (nick COLLATE NOCASE));
CREATE TABLE IF NOT EXISTS userinfo_log(id integer PRIMARY KEY AUTOINCREMENT,time_operation DATETIME, operation VARCHAR(1));

--CREATE TRIGGER tr_u_userinfo BEFORE UPDATE ON userinfo FOR EACH ROW
--BEGIN INSERT into userinfo_log(time_operation, operation) VALUES(DATETIME('now'),'U'); END;
--CREATE TRIGGER tr_d_userinfo BEFORE DELETE ON userinfo FOR EACH ROW
--BEGIN INSERT into userinfo_log(time_operation, operation) VALUES(DATETIME('now'),'D'); END;
--CREATE TRIGGER tr_i_userinfo BEFORE INSERT ON userinfo FOR EACH ROW
--BEGIN INSERT into userinfo_log(time_operation, operation) VALUES(DATETIME('now'),'I'); END;

.stats ON
INSERT OR REPLACE INTO userinfo (nick, last_updated, ip_address, share, description, tag, connection, email) VALUES('nick1',DATETIME('now'),'ip','share','description','tag','connection','email');
INSERT OR REPLACE INTO userinfo (nick, last_updated, ip_address, share, description, tag, connection, email) VALUES('nick1',DATETIME('now'),'ip','share','description','tag','connection','email');
UPDATE userinfo set last_updated = DATETIME('now'), ip_address == 'ip', share = 'share', description = 'description', tag = 'tag', connection = 'connection', email = 'email' where nick = 'nick1';
UPDATE userinfo set last_updated = DATETIME('now'), ip_address == 'ip', share = 'share', description = 'description', tag = 'tag', connection = 'connection', email = 'email' where nick = 'nick1';
UPDATE userinfo set last_updated = DATETIME('now'), ip_address == 'ip', share = 'share', description = 'description', tag = 'tag', connection = 'connection', email = 'email' where nick = 'nick1';
.stats OFF

INSERT OR REPLACE INTO userinfo (nick, last_updated, ip_address, share, description, tag, connection, email) VALUES('nick1',DATETIME('now'),'ip','share','description','tag','connection','email');
INSERT OR REPLACE INTO userinfo (nick, last_updated, ip_address, share, description, tag, connection, email) VALUES('nick1',DATETIME('now'),'ip','share','description','tag','connection','email');
select * from userinfo_log order by id;
select * from userinfo;
update userinfo set ip_address = 'x';
select * from userinfo_log order by id;
delete from userinfo;
select * from userinfo;
select * from userinfo_log order by id;
 