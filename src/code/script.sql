CREATE TABLE IF NOT EXISTS CLOUD (
server TEXT,
user TEXT,
PRIMARY KEY(server, user)
);

CREATE TABLE IF NOT EXISTS CALDAV (
url TEXT PRIMARY KEY,
username TEXT,
authType TEXT,
tokenUrl TEXT,
clientId TEXT,
authorizationUrl TEXT,
tokenExpiresAt TEXT,
enabled INTEGER NOT NULL DEFAULT 1
);
