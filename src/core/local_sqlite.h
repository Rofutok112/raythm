#pragma once

#include <optional>
#include <string>

#include "sqlite3.h"

namespace local_sqlite {

class statement {
public:
    statement(sqlite3* database, const char* sql);
    ~statement();

    statement(const statement&) = delete;
    statement& operator=(const statement&) = delete;

    bool valid() const;
    sqlite3_stmt* get() const;

private:
    sqlite3* database_ = nullptr;
    sqlite3_stmt* statement_ = nullptr;
};

class database {
public:
    database();
    ~database();

    database(const database&) = delete;
    database& operator=(const database&) = delete;
    database(database&& other) noexcept;
    database& operator=(database&& other) noexcept;

    bool valid() const;
    sqlite3* get() const;

private:
    void close();

    sqlite3* database_ = nullptr;
};

class transaction {
public:
    explicit transaction(sqlite3* database);
    ~transaction();

    transaction(const transaction&) = delete;
    transaction& operator=(const transaction&) = delete;

    bool active() const;
    bool commit();

private:
    sqlite3* database_ = nullptr;
    bool active_ = false;
};

database open_local_content_database();
bool exec(sqlite3* database, const char* sql);
bool ensure_common_schema(sqlite3* database);
void bind_text(sqlite3_stmt* statement, int index, const std::string& value);
std::string column_text(sqlite3_stmt* statement, int index);
bool step_done(sqlite3_stmt* statement);
std::optional<std::string> metadata_value(sqlite3* database, const std::string& key);
void put_metadata(sqlite3* database, const std::string& key, const std::string& value);

}  // namespace local_sqlite
