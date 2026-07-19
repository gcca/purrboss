CREATE TABLE IF NOT EXISTS "schema_migrations" (version varchar(128) primary key);
CREATE TABLE sessions (
  session_key TEXT PRIMARY KEY,
  user_id TEXT NOT NULL,
  data TEXT NOT NULL DEFAULT '{}' CHECK (json_valid(data) AND json_type(data) = 'object'),
  created_at INTEGER NOT NULL DEFAULT (unixepoch()),
  expires_at INTEGER NOT NULL,
  revoked_at INTEGER,
  reason_revoked TEXT,
  CHECK (expires_at > created_at)
);
CREATE INDEX idx_sessions_expires ON sessions(expires_at);
CREATE TABLE active_instances (
  machine_id TEXT PRIMARY KEY,
  internal_ip TEXT NOT NULL DEFAULT '',
  region TEXT NOT NULL DEFAULT '',
  last_seen INTEGER NOT NULL DEFAULT (unixepoch()),
  last_request_type TEXT NOT NULL
);
CREATE INDEX idx_active_instances_last_seen ON active_instances(last_seen);
-- Dbmate schema migrations
INSERT INTO "schema_migrations" (version) VALUES
  ('20260718191300');
