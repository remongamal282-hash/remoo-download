-- Initial Remoo Download schema.
-- The runtime currently embeds this migration for desktop reliability; this file
-- is the auditable source copy required by SDS-04.

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS categories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    default_path TEXT NOT NULL,
    match_rule TEXT,
    parent_category_id INTEGER NULL REFERENCES categories(id),
    icon TEXT,
    is_system_default BOOLEAN NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_categories_name ON categories(name);

CREATE TABLE IF NOT EXISTS schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    schedule_type TEXT NOT NULL CHECK(schedule_type IN ('one_time','recurring')),
    start_at DATETIME NULL,
    cron_expression TEXT NULL,
    quiet_hours_start TEXT NULL,
    quiet_hours_end TEXT NULL,
    post_action TEXT NOT NULL DEFAULT 'none'
        CHECK(post_action IN ('none','shutdown','sleep','close_app','run_script')),
    post_action_script_path TEXT NULL,
    enabled BOOLEAN NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS downloads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    url TEXT NOT NULL,
    final_url TEXT NULL,
    file_name TEXT NOT NULL,
    save_path TEXT NOT NULL,
    category_id INTEGER NULL REFERENCES categories(id) ON DELETE SET NULL,
    total_size_bytes INTEGER NULL,
    downloaded_bytes INTEGER NOT NULL DEFAULT 0,
    status TEXT NOT NULL DEFAULT 'queued'
        CHECK(status IN ('queued','downloading','paused','completed','failed','cancelled','reconnecting')),
    priority INTEGER NOT NULL DEFAULT 0,
    supports_resume BOOLEAN NOT NULL DEFAULT 0,
    checksum_algorithm TEXT NULL CHECK(checksum_algorithm IN ('md5','sha256',NULL)),
    checksum_expected TEXT NULL,
    checksum_actual TEXT NULL,
    retry_count INTEGER NOT NULL DEFAULT 0,
    max_retries INTEGER NOT NULL DEFAULT 10,
    speed_limit_bytes_per_sec INTEGER NULL,
    referrer_url TEXT NULL,
    auth_required BOOLEAN NOT NULL DEFAULT 0,
    auth_username TEXT NULL,
    auth_secret_ref TEXT NULL,
    schedule_id INTEGER NULL REFERENCES schedules(id) ON DELETE SET NULL,
    source_extension TEXT NULL,
    error_message TEXT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    completed_at DATETIME NULL,
    last_checkpoint_at DATETIME NULL
);
CREATE INDEX IF NOT EXISTS idx_downloads_status ON downloads(status);
CREATE INDEX IF NOT EXISTS idx_downloads_category ON downloads(category_id);
CREATE INDEX IF NOT EXISTS idx_downloads_priority ON downloads(priority DESC, created_at ASC);

CREATE TABLE IF NOT EXISTS segments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    download_id INTEGER NOT NULL REFERENCES downloads(id) ON DELETE CASCADE,
    segment_index INTEGER NOT NULL,
    range_start INTEGER NOT NULL,
    range_end INTEGER NOT NULL,
    downloaded_bytes INTEGER NOT NULL DEFAULT 0,
    status TEXT NOT NULL DEFAULT 'pending'
        CHECK(status IN ('pending','active','paused','completed','failed')),
    last_error TEXT NULL,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_segments_download_index ON segments(download_id, segment_index);

CREATE TABLE IF NOT EXISTS download_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    download_id INTEGER NOT NULL REFERENCES downloads(id) ON DELETE CASCADE,
    event_type TEXT NOT NULL
        CHECK(event_type IN ('created','started','paused','resumed','reconnect_attempt',
                             'failed','completed','cancelled','checksum_failed')),
    details TEXT NULL,
    occurred_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_events_download ON download_events(download_id, occurred_at);

CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    value_type TEXT NOT NULL CHECK(value_type IN ('int','bool','string','json')),
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS plugins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    library_path TEXT NOT NULL,
    version TEXT NOT NULL,
    enabled BOOLEAN NOT NULL DEFAULT 1,
    trust_level TEXT NOT NULL DEFAULT 'community' CHECK(trust_level IN ('official','community')),
    installed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS plugin_execution_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id INTEGER NOT NULL REFERENCES plugins(id) ON DELETE CASCADE,
    download_id INTEGER REFERENCES downloads(id) ON DELETE SET NULL,
    status TEXT NOT NULL CHECK(status IN ('success','timeout','error')),
    duration_ms INTEGER NOT NULL,
    error_message TEXT NULL,
    executed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS extension_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    browser_name TEXT NOT NULL CHECK(browser_name IN ('chrome','firefox','edge','safari')),
    extension_version TEXT NOT NULL,
    connected_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_seen_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

INSERT OR IGNORE INTO settings (key, value, value_type) VALUES
    ('max_concurrent_downloads', '3', 'int'),
    ('max_segments_per_download', '16', 'int'),
    ('theme', 'system', 'string'),
    ('language', 'ar', 'string'),
    ('clipboard_monitoring_enabled', 'false', 'bool'),
    ('auto_start_with_os', 'false', 'bool');

INSERT OR IGNORE INTO schema_version (version) VALUES (1);
