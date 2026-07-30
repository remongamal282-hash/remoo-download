#include "storage/storage_manager.h"

#include <sqlite3.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace storage {
namespace {

constexpr int kCurrentSchemaVersion = 1;

struct StatementDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        sqlite3_finalize(stmt);
    }
};

using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

bool bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    if (value.empty()) {
        return sqlite3_bind_null(stmt, index) == SQLITE_OK;
    }
    return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool bindNullableInt64(sqlite3_stmt* stmt, int index, int64_t value) {
    if (value <= 0) {
        return sqlite3_bind_null(stmt, index) == SQLITE_OK;
    }
    return sqlite3_bind_int64(stmt, index, value) == SQLITE_OK;
}

std::string columnText(sqlite3_stmt* stmt, int index) {
    const unsigned char* value = sqlite3_column_text(stmt, index);
    return value ? reinterpret_cast<const char*>(value) : "";
}

bool exec(sqlite3* db, const char* sql) {
    char* error = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (error) {
        sqlite3_free(error);
    }
    return rc == SQLITE_OK;
}

bool stepDone(sqlite3_stmt* stmt) {
    return sqlite3_step(stmt) == SQLITE_DONE;
}

DownloadRecord readDownload(sqlite3_stmt* stmt) {
    DownloadRecord record;
    record.id = sqlite3_column_int64(stmt, 0);
    record.url = columnText(stmt, 1);
    record.finalUrl = columnText(stmt, 2);
    record.filename = columnText(stmt, 3);
    record.savePath = columnText(stmt, 4);
    record.categoryId = sqlite3_column_int64(stmt, 5);
    record.totalSizeBytes = sqlite3_column_int64(stmt, 6);
    record.downloadedBytes = sqlite3_column_int64(stmt, 7);
    record.status = columnText(stmt, 8);
    record.priority = sqlite3_column_int(stmt, 9);
    record.supportsResume = sqlite3_column_int(stmt, 10) != 0;
    record.checksumAlgorithm = columnText(stmt, 11);
    record.checksumExpected = columnText(stmt, 12);
    record.checksumActual = columnText(stmt, 13);
    record.retryCount = sqlite3_column_int(stmt, 14);
    record.maxRetries = sqlite3_column_int(stmt, 15);
    record.speedLimitBytesPerSec = sqlite3_column_int64(stmt, 16);
    record.referrerUrl = columnText(stmt, 17);
    record.authRequired = sqlite3_column_int(stmt, 18) != 0;
    record.authUsername = columnText(stmt, 19);
    record.authSecretRef = columnText(stmt, 20);
    record.scheduleId = sqlite3_column_int64(stmt, 21);
    record.sourceExtension = columnText(stmt, 22);
    record.errorMessage = columnText(stmt, 23);
    record.createdAt = columnText(stmt, 24);
    record.completedAt = columnText(stmt, 25);
    record.lastCheckpointAt = columnText(stmt, 26);
    return record;
}

SegmentRecord readSegment(sqlite3_stmt* stmt) {
    SegmentRecord record;
    record.id = sqlite3_column_int64(stmt, 0);
    record.downloadId = sqlite3_column_int64(stmt, 1);
    record.segmentIndex = sqlite3_column_int(stmt, 2);
    record.rangeStart = sqlite3_column_int64(stmt, 3);
    record.rangeEnd = sqlite3_column_int64(stmt, 4);
    record.downloadedBytes = sqlite3_column_int64(stmt, 5);
    record.status = columnText(stmt, 6);
    record.lastError = columnText(stmt, 7);
    record.updatedAt = columnText(stmt, 8);
    return record;
}

CategoryRecord readCategory(sqlite3_stmt* stmt) {
    CategoryRecord record;
    record.id = sqlite3_column_int64(stmt, 0);
    record.name = columnText(stmt, 1);
    record.defaultPath = columnText(stmt, 2);
    record.matchRule = columnText(stmt, 3);
    record.parentCategoryId = sqlite3_column_int64(stmt, 4);
    record.icon = columnText(stmt, 5);
    record.isSystemDefault = sqlite3_column_int(stmt, 6) != 0;
    record.createdAt = columnText(stmt, 7);
    return record;
}

} // namespace

class StorageManager::Impl {
public:
    sqlite3* db = nullptr;
    std::string dbPath;

    bool prepare(const char* sql, StatementPtr& stmt) const {
        sqlite3_stmt* raw = nullptr;
        if (!db || sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
            return false;
        }
        stmt.reset(raw);
        return true;
    }

    bool beginTransaction() const {
        return exec(db, "BEGIN IMMEDIATE TRANSACTION;");
    }

    bool commit() const {
        return exec(db, "COMMIT;");
    }

    void rollback() const {
        exec(db, "ROLLBACK;");
    }

    int schemaVersion() const {
        if (!db) {
            return 0;
        }
        const char* sql = "SELECT version FROM schema_version ORDER BY version DESC LIMIT 1;";
        StatementPtr stmt;
        if (!prepare(sql, stmt)) {
            return 0;
        }
        if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
            return 0;
        }
        return sqlite3_column_int(stmt.get(), 0);
    }

    bool applyMigration0001() const {
        static constexpr const char* migration = R"sql(
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
        )sql";

        return exec(db, migration);
    }

    bool migrate() const {
        const int existingVersion = schemaVersion();
        if (existingVersion >= kCurrentSchemaVersion) {
            return true;
        }
        if (existingVersion > 0 && std::filesystem::exists(dbPath)) {
            std::error_code ec;
            const std::filesystem::path backupPath =
                std::filesystem::path(dbPath).string() + ".bak-v" + std::to_string(existingVersion);
            std::filesystem::copy_file(dbPath, backupPath,
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                return false;
            }
        }
        if (!beginTransaction()) {
            return false;
        }
        if (!applyMigration0001()) {
            rollback();
            return false;
        }
        return commit();
    }
};

StorageManager::StorageManager(const std::string& dbPath)
    : d(std::make_unique<Impl>())
{
    d->dbPath = dbPath;
}

StorageManager::~StorageManager() {
    close();
}

bool StorageManager::open() {
    if (d->db) {
        return true;
    }

    if (sqlite3_open(d->dbPath.c_str(), &d->db) != SQLITE_OK) {
        d->db = nullptr;
        return false;
    }

    if (!exec(d->db, "PRAGMA foreign_keys=ON;") ||
        !exec(d->db, "PRAGMA journal_mode=WAL;") ||
        !exec(d->db, "PRAGMA synchronous=NORMAL;")) {
        close();
        return false;
    }

    if (!d->migrate()) {
        close();
        return false;
    }

    return true;
}

void StorageManager::close() {
    if (d->db) {
        sqlite3_close(d->db);
        d->db = nullptr;
    }
}

bool StorageManager::isOpen() const {
    return d->db != nullptr;
}

int StorageManager::getSchemaVersion() const {
    return d->schemaVersion();
}

int64_t StorageManager::saveDownload(const DownloadRecord& record) {
    const char* sql = R"sql(
        INSERT INTO downloads (
            url, final_url, file_name, save_path, category_id, total_size_bytes,
            downloaded_bytes, status, priority, supports_resume, checksum_algorithm,
            checksum_expected, checksum_actual, retry_count, max_retries,
            speed_limit_bytes_per_sec, referrer_url, auth_required, auth_username,
            auth_secret_ref, schedule_id, source_extension, error_message,
            completed_at, last_checkpoint_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )sql";

    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return -1;
    }

    bindText(stmt.get(), 1, record.url);
    bindText(stmt.get(), 2, record.finalUrl);
    bindText(stmt.get(), 3, record.filename);
    bindText(stmt.get(), 4, record.savePath);
    bindNullableInt64(stmt.get(), 5, record.categoryId);
    bindNullableInt64(stmt.get(), 6, record.totalSizeBytes);
    sqlite3_bind_int64(stmt.get(), 7, record.downloadedBytes);
    bindText(stmt.get(), 8, record.status.empty() ? std::string("queued") : record.status);
    sqlite3_bind_int(stmt.get(), 9, record.priority);
    sqlite3_bind_int(stmt.get(), 10, record.supportsResume ? 1 : 0);
    bindText(stmt.get(), 11, record.checksumAlgorithm);
    bindText(stmt.get(), 12, record.checksumExpected);
    bindText(stmt.get(), 13, record.checksumActual);
    sqlite3_bind_int(stmt.get(), 14, record.retryCount);
    sqlite3_bind_int(stmt.get(), 15, record.maxRetries);
    bindNullableInt64(stmt.get(), 16, record.speedLimitBytesPerSec);
    bindText(stmt.get(), 17, record.referrerUrl);
    sqlite3_bind_int(stmt.get(), 18, record.authRequired ? 1 : 0);
    bindText(stmt.get(), 19, record.authUsername);
    bindText(stmt.get(), 20, record.authSecretRef);
    bindNullableInt64(stmt.get(), 21, record.scheduleId);
    bindText(stmt.get(), 22, record.sourceExtension);
    bindText(stmt.get(), 23, record.errorMessage);
    bindText(stmt.get(), 24, record.completedAt);
    bindText(stmt.get(), 25, record.lastCheckpointAt);

    if (!stepDone(stmt.get())) {
        return -1;
    }
    const int64_t id = sqlite3_last_insert_rowid(d->db);
    logDownloadEvent(id, "created");
    return id;
}

bool StorageManager::updateDownload(int64_t id, const DownloadRecord& record) {
    const char* sql = R"sql(
        UPDATE downloads SET
            url = ?, final_url = ?, file_name = ?, save_path = ?, category_id = ?,
            total_size_bytes = ?, downloaded_bytes = ?, status = ?, priority = ?,
            supports_resume = ?, checksum_algorithm = ?, checksum_expected = ?,
            checksum_actual = ?, retry_count = ?, max_retries = ?,
            speed_limit_bytes_per_sec = ?, referrer_url = ?, auth_required = ?,
            auth_username = ?, auth_secret_ref = ?, schedule_id = ?,
            source_extension = ?, error_message = ?, completed_at = ?,
            last_checkpoint_at = ?
        WHERE id = ?;
    )sql";

    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return false;
    }

    bindText(stmt.get(), 1, record.url);
    bindText(stmt.get(), 2, record.finalUrl);
    bindText(stmt.get(), 3, record.filename);
    bindText(stmt.get(), 4, record.savePath);
    bindNullableInt64(stmt.get(), 5, record.categoryId);
    bindNullableInt64(stmt.get(), 6, record.totalSizeBytes);
    sqlite3_bind_int64(stmt.get(), 7, record.downloadedBytes);
    bindText(stmt.get(), 8, record.status.empty() ? std::string("queued") : record.status);
    sqlite3_bind_int(stmt.get(), 9, record.priority);
    sqlite3_bind_int(stmt.get(), 10, record.supportsResume ? 1 : 0);
    bindText(stmt.get(), 11, record.checksumAlgorithm);
    bindText(stmt.get(), 12, record.checksumExpected);
    bindText(stmt.get(), 13, record.checksumActual);
    sqlite3_bind_int(stmt.get(), 14, record.retryCount);
    sqlite3_bind_int(stmt.get(), 15, record.maxRetries);
    bindNullableInt64(stmt.get(), 16, record.speedLimitBytesPerSec);
    bindText(stmt.get(), 17, record.referrerUrl);
    sqlite3_bind_int(stmt.get(), 18, record.authRequired ? 1 : 0);
    bindText(stmt.get(), 19, record.authUsername);
    bindText(stmt.get(), 20, record.authSecretRef);
    bindNullableInt64(stmt.get(), 21, record.scheduleId);
    bindText(stmt.get(), 22, record.sourceExtension);
    bindText(stmt.get(), 23, record.errorMessage);
    bindText(stmt.get(), 24, record.completedAt);
    bindText(stmt.get(), 25, record.lastCheckpointAt);
    sqlite3_bind_int64(stmt.get(), 26, id);

    return stepDone(stmt.get()) && sqlite3_changes(d->db) > 0;
}

DownloadRecord StorageManager::getDownload(int64_t id) const {
    const char* sql = R"sql(
        SELECT id, url, final_url, file_name, save_path, category_id,
               total_size_bytes, downloaded_bytes, status, priority,
               supports_resume, checksum_algorithm, checksum_expected,
               checksum_actual, retry_count, max_retries,
               speed_limit_bytes_per_sec, referrer_url, auth_required,
               auth_username, auth_secret_ref, schedule_id, source_extension,
               error_message, created_at, completed_at, last_checkpoint_at
        FROM downloads WHERE id = ?;
    )sql";

    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return {};
    }
    sqlite3_bind_int64(stmt.get(), 1, id);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return {};
    }
    return readDownload(stmt.get());
}

std::vector<DownloadRecord> StorageManager::getAllDownloads() const {
    const char* sql = R"sql(
        SELECT id, url, final_url, file_name, save_path, category_id,
               total_size_bytes, downloaded_bytes, status, priority,
               supports_resume, checksum_algorithm, checksum_expected,
               checksum_actual, retry_count, max_retries,
               speed_limit_bytes_per_sec, referrer_url, auth_required,
               auth_username, auth_secret_ref, schedule_id, source_extension,
               error_message, created_at, completed_at, last_checkpoint_at
        FROM downloads ORDER BY created_at DESC, id DESC;
    )sql";

    StatementPtr stmt;
    std::vector<DownloadRecord> records;
    if (!d->prepare(sql, stmt)) {
        return records;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        records.push_back(readDownload(stmt.get()));
    }
    return records;
}

std::vector<DownloadRecord> StorageManager::getDownloadsByStatus(const std::string& status) const {
    const char* sql = R"sql(
        SELECT id, url, final_url, file_name, save_path, category_id,
               total_size_bytes, downloaded_bytes, status, priority,
               supports_resume, checksum_algorithm, checksum_expected,
               checksum_actual, retry_count, max_retries,
               speed_limit_bytes_per_sec, referrer_url, auth_required,
               auth_username, auth_secret_ref, schedule_id, source_extension,
               error_message, created_at, completed_at, last_checkpoint_at
        FROM downloads
        WHERE status = ?
        ORDER BY priority DESC, created_at ASC;
    )sql";

    StatementPtr stmt;
    std::vector<DownloadRecord> records;
    if (!d->prepare(sql, stmt)) {
        return records;
    }
    bindText(stmt.get(), 1, status);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        records.push_back(readDownload(stmt.get()));
    }
    return records;
}

std::vector<SegmentRecord> StorageManager::getSegments(int64_t downloadId) const {
    const char* sql = R"sql(
        SELECT id, download_id, segment_index, range_start, range_end,
               downloaded_bytes, status, last_error, updated_at
        FROM segments
        WHERE download_id = ?
        ORDER BY segment_index ASC;
    )sql";

    StatementPtr stmt;
    std::vector<SegmentRecord> records;
    if (!d->prepare(sql, stmt)) {
        return records;
    }
    sqlite3_bind_int64(stmt.get(), 1, downloadId);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        records.push_back(readSegment(stmt.get()));
    }
    return records;
}

bool StorageManager::saveSegment(const SegmentRecord& segment) {
    const char* sql = R"sql(
        INSERT INTO segments (
            download_id, segment_index, range_start, range_end,
            downloaded_bytes, status, last_error
        ) VALUES (?, ?, ?, ?, ?, ?, ?);
    )sql";

    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return false;
    }
    sqlite3_bind_int64(stmt.get(), 1, segment.downloadId);
    sqlite3_bind_int(stmt.get(), 2, segment.segmentIndex);
    sqlite3_bind_int64(stmt.get(), 3, segment.rangeStart);
    sqlite3_bind_int64(stmt.get(), 4, segment.rangeEnd);
    sqlite3_bind_int64(stmt.get(), 5, segment.downloadedBytes);
    bindText(stmt.get(), 6, segment.status.empty() ? std::string("pending") : segment.status);
    bindText(stmt.get(), 7, segment.lastError);
    return stepDone(stmt.get());
}

bool StorageManager::updateSegment(int64_t id, const SegmentRecord& segment) {
    const char* sql = R"sql(
        UPDATE segments SET
            segment_index = ?, range_start = ?, range_end = ?,
            downloaded_bytes = ?, status = ?, last_error = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?;
    )sql";

    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return false;
    }
    sqlite3_bind_int(stmt.get(), 1, segment.segmentIndex);
    sqlite3_bind_int64(stmt.get(), 2, segment.rangeStart);
    sqlite3_bind_int64(stmt.get(), 3, segment.rangeEnd);
    sqlite3_bind_int64(stmt.get(), 4, segment.downloadedBytes);
    bindText(stmt.get(), 5, segment.status.empty() ? std::string("pending") : segment.status);
    bindText(stmt.get(), 6, segment.lastError);
    sqlite3_bind_int64(stmt.get(), 7, id);
    return stepDone(stmt.get()) && sqlite3_changes(d->db) > 0;
}

bool StorageManager::saveCheckpoint(int64_t downloadId, int64_t segmentId, const std::string& data) {
    const char* sql = R"sql(
        UPDATE segments
        SET downloaded_bytes = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ? AND download_id = ?;
    )sql";

    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return false;
    }
    sqlite3_bind_int64(stmt.get(), 1, std::stoll(data.empty() ? "0" : data));
    sqlite3_bind_int64(stmt.get(), 2, segmentId);
    sqlite3_bind_int64(stmt.get(), 3, downloadId);
    if (!stepDone(stmt.get())) {
        return false;
    }

    const char* downloadSql = R"sql(
        UPDATE downloads
        SET last_checkpoint_at = CURRENT_TIMESTAMP,
            downloaded_bytes = (
                SELECT COALESCE(SUM(downloaded_bytes), 0)
                FROM segments
                WHERE download_id = ?
            )
        WHERE id = ?;
    )sql";
    StatementPtr downloadStmt;
    if (!d->prepare(downloadSql, downloadStmt)) {
        return false;
    }
    sqlite3_bind_int64(downloadStmt.get(), 1, downloadId);
    sqlite3_bind_int64(downloadStmt.get(), 2, downloadId);
    return stepDone(downloadStmt.get());
}

std::string StorageManager::restoreCheckpoint(int64_t downloadId, int64_t segmentId) const {
    const char* sql = "SELECT downloaded_bytes FROM segments WHERE download_id = ? AND id = ?;";
    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return "";
    }
    sqlite3_bind_int64(stmt.get(), 1, downloadId);
    sqlite3_bind_int64(stmt.get(), 2, segmentId);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return "";
    }
    return std::to_string(sqlite3_column_int64(stmt.get(), 0));
}

int64_t StorageManager::saveCategory(const CategoryRecord& category) {
    const char* sql = R"sql(
        INSERT INTO categories (
            name, default_path, match_rule, parent_category_id, icon, is_system_default
        ) VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(name) DO UPDATE SET
            default_path = excluded.default_path,
            match_rule = excluded.match_rule,
            parent_category_id = excluded.parent_category_id,
            icon = excluded.icon,
            is_system_default = excluded.is_system_default;
    )sql";

    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return -1;
    }
    bindText(stmt.get(), 1, category.name);
    bindText(stmt.get(), 2, category.defaultPath);
    bindText(stmt.get(), 3, category.matchRule);
    bindNullableInt64(stmt.get(), 4, category.parentCategoryId);
    bindText(stmt.get(), 5, category.icon);
    sqlite3_bind_int(stmt.get(), 6, category.isSystemDefault ? 1 : 0);
    if (!stepDone(stmt.get())) {
        return -1;
    }
    return sqlite3_last_insert_rowid(d->db);
}

std::vector<CategoryRecord> StorageManager::getAllCategories() const {
    const char* sql = R"sql(
        SELECT id, name, default_path, match_rule, parent_category_id, icon,
               is_system_default, created_at
        FROM categories
        ORDER BY is_system_default DESC, name ASC;
    )sql";

    StatementPtr stmt;
    std::vector<CategoryRecord> records;
    if (!d->prepare(sql, stmt)) {
        return records;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        records.push_back(readCategory(stmt.get()));
    }
    return records;
}

bool StorageManager::logDownloadEvent(int64_t downloadId, const std::string& eventType, const std::string& details) {
    const char* sql = "INSERT INTO download_events (download_id, event_type, details) VALUES (?, ?, ?);";
    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return false;
    }
    sqlite3_bind_int64(stmt.get(), 1, downloadId);
    bindText(stmt.get(), 2, eventType);
    bindText(stmt.get(), 3, details);
    return stepDone(stmt.get());
}

bool StorageManager::setSetting(const std::string& key, const std::string& value, const std::string& valueType) {
    const char* sql = R"sql(
        INSERT INTO settings (key, value, value_type, updated_at)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP)
        ON CONFLICT(key) DO UPDATE SET
            value = excluded.value,
            value_type = excluded.value_type,
            updated_at = CURRENT_TIMESTAMP;
    )sql";
    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return false;
    }
    bindText(stmt.get(), 1, key);
    bindText(stmt.get(), 2, value);
    bindText(stmt.get(), 3, valueType);
    return stepDone(stmt.get());
}

std::string StorageManager::getSetting(const std::string& key) const {
    const char* sql = "SELECT value FROM settings WHERE key = ?;";
    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return "";
    }
    bindText(stmt.get(), 1, key);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return "";
    }
    return columnText(stmt.get(), 0);
}

bool StorageManager::deleteDownload(int64_t id) {
    const char* sql = "DELETE FROM downloads WHERE id = ?;";
    StatementPtr stmt;
    if (!d->prepare(sql, stmt)) {
        return false;
    }
    sqlite3_bind_int64(stmt.get(), 1, id);
    return stepDone(stmt.get()) && sqlite3_changes(d->db) > 0;
}

bool StorageManager::cleanupCompleted() {
    const char* sql = "DELETE FROM download_events WHERE occurred_at < datetime('now', '-90 days');";
    return exec(d->db, sql);
}

} // namespace storage
} // namespace remo
