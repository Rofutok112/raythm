#include "local_sqlite.h"

#include <filesystem>

#include "app_paths.h"

namespace local_sqlite {

statement::statement(sqlite3* database, const char* sql) : database_(database) {
    sqlite3_prepare_v2(database_, sql, -1, &statement_, nullptr);
}

statement::~statement() {
    if (statement_ != nullptr) {
        sqlite3_finalize(statement_);
    }
}

bool statement::valid() const {
    return statement_ != nullptr;
}

sqlite3_stmt* statement::get() const {
    return statement_;
}

database::database() {
    app_paths::ensure_directories();
    const std::filesystem::path db_path = app_paths::local_content_db_path();
    if (sqlite3_open(db_path.string().c_str(), &database_) != SQLITE_OK) {
        close();
    }
    if (database_ != nullptr) {
        sqlite3_busy_timeout(database_, 5000);
    }
}

database::~database() {
    close();
}

database::database(database&& other) noexcept : database_(other.database_) {
    other.database_ = nullptr;
}

database& database::operator=(database&& other) noexcept {
    if (this != &other) {
        close();
        database_ = other.database_;
        other.database_ = nullptr;
    }
    return *this;
}

bool database::valid() const {
    return database_ != nullptr;
}

sqlite3* database::get() const {
    return database_;
}

void database::close() {
    if (database_ != nullptr) {
        sqlite3_close(database_);
        database_ = nullptr;
    }
}

transaction::transaction(sqlite3* database) : database_(database) {
    active_ = database_ != nullptr && exec(database_, "BEGIN IMMEDIATE;");
}

transaction::~transaction() {
    if (active_) {
        exec(database_, "ROLLBACK;");
    }
}

bool transaction::active() const {
    return active_;
}

bool transaction::commit() {
    if (!active_) {
        return false;
    }
    active_ = !exec(database_, "COMMIT;");
    return !active_;
}

database open_local_content_database() {
    database db;
    if (db.valid()) {
        ensure_common_schema(db.get());
    }
    return db;
}

bool exec(sqlite3* database, const char* sql) {
    return sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool ensure_common_schema(sqlite3* database) {
    return exec(database, "PRAGMA foreign_keys = ON;") &&
           exec(database, "PRAGMA journal_mode = WAL;") &&
           exec(database,
                "CREATE TABLE IF NOT EXISTS metadata ("
                "key TEXT PRIMARY KEY,"
                "value TEXT NOT NULL"
                ");");
}

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
    sqlite3_bind_text(statement, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

std::string column_text(sqlite3_stmt* statement, int index) {
    const unsigned char* text = sqlite3_column_text(statement, index);
    return text == nullptr ? std::string() : reinterpret_cast<const char*>(text);
}

bool step_done(sqlite3_stmt* statement) {
    return sqlite3_step(statement) == SQLITE_DONE;
}

std::optional<std::string> metadata_value(sqlite3* database, const std::string& key) {
    statement query(database, "SELECT value FROM metadata WHERE key = ?;");
    if (!query.valid()) {
        return std::nullopt;
    }
    bind_text(query.get(), 1, key);
    if (sqlite3_step(query.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return column_text(query.get(), 0);
}

void put_metadata(sqlite3* database, const std::string& key, const std::string& value) {
    statement query(database,
                    "INSERT INTO metadata(key, value) VALUES(?, ?) "
                    "ON CONFLICT(key) DO UPDATE SET value = excluded.value;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, key);
    bind_text(query.get(), 2, value);
    sqlite3_step(query.get());
}

}  // namespace local_sqlite
