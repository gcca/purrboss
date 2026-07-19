#include "purrboss/storage/database.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <sqlite3.h>

namespace {

class Connection {
public:
  explicit Connection(const std::string &path) {
    const int flags =
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path.c_str(), &handle_, flags, nullptr) != SQLITE_OK) {
      const std::string message = handle_ == nullptr
                                      ? "unable to open SQLite database"
                                      : sqlite3_errmsg(handle_);
      if (handle_ != nullptr) {
        sqlite3_close(handle_);
        handle_ = nullptr;
      }
      throw std::runtime_error(message);
    }

    sqlite3_busy_timeout(handle_, 5000);
    Execute("PRAGMA foreign_keys = ON; "
            "PRAGMA synchronous = NORMAL; "
            "PRAGMA wal_autocheckpoint = 500; "
            "PRAGMA journal_size_limit = 8388608;");
  }

  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;

  ~Connection() {
    if (handle_ != nullptr) {
      sqlite3_close(handle_);
    }
  }

  [[nodiscard]] sqlite3 *get() const { return handle_; }

  void Execute(const char *sql) const {
    char *error = nullptr;
    if (sqlite3_exec(handle_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
      const std::string message =
          error == nullptr ? sqlite3_errmsg(handle_) : error;
      sqlite3_free(error);
      throw std::runtime_error(message);
    }
  }

private:
  sqlite3 *handle_ = nullptr;
};

class Statement {
public:
  Statement(sqlite3 *database, const char *sql) : database_{database} {
    if (sqlite3_prepare_v2(database_, sql, -1, &handle_, nullptr) !=
        SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database_));
    }
  }

  Statement(const Statement &) = delete;
  Statement &operator=(const Statement &) = delete;

  ~Statement() {
    if (handle_ != nullptr) {
      sqlite3_finalize(handle_);
    }
  }

  void BindText(int position, std::string_view value) const {
    if (sqlite3_bind_text(handle_, position, value.data(),
                          static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database_));
    }
  }

  void BindInteger(int position, std::int64_t value) const {
    if (sqlite3_bind_int64(handle_, position, value) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database_));
    }
  }

  [[nodiscard]] int Step() const { return sqlite3_step(handle_); }

  [[nodiscard]] std::string Text(int column) const {
    const auto *text = sqlite3_column_text(handle_, column);
    if (text == nullptr) {
      return {};
    }
    const auto size = sqlite3_column_bytes(handle_, column);
    return {reinterpret_cast<const char *>(text),
            static_cast<std::size_t>(size)};
  }

  [[nodiscard]] std::int64_t Integer(int column) const {
    return sqlite3_column_int64(handle_, column);
  }

  [[nodiscard]] bool IsNull(int column) const {
    return sqlite3_column_type(handle_, column) == SQLITE_NULL;
  }

private:
  sqlite3 *database_ = nullptr;
  sqlite3_stmt *handle_ = nullptr;
};

void RequireDone(sqlite3 *database, const Statement &statement) {
  if (statement.Step() != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(database));
  }
}

} // namespace

namespace purrboss::storage {

Database::Database(std::string path) : path_{std::move(path)} {}

void Database::Initialize() const {
  Connection connection{path_};
  Statement statement{connection.get(), "PRAGMA journal_mode = WAL;"};
  if (statement.Step() != SQLITE_ROW || statement.Text(0) != "wal") {
    throw std::runtime_error("unable to enable SQLite WAL journal mode");
  }
}

void Database::CreateSession(const Session &session) const {
  Connection connection{path_};
  Statement statement{connection.get(),
                      "INSERT INTO sessions "
                      "(session_key, user_id, data, created_at, expires_at) "
                      "VALUES (?, ?, ?, ?, ?)"};
  statement.BindText(1, session.session_key);
  statement.BindText(2, session.user_id);
  statement.BindText(3, session.data);
  statement.BindInteger(4, session.created_at);
  statement.BindInteger(5, session.expires_at);
  RequireDone(connection.get(), statement);
}

std::optional<Session>
Database::FindSession(std::string_view session_key) const {
  Connection connection{path_};
  Statement statement{
      connection.get(),
      "SELECT session_key, user_id, data, created_at, expires_at, revoked_at, "
      "reason_revoked FROM sessions WHERE session_key = ?"};
  statement.BindText(1, session_key);

  const int result = statement.Step();
  if (result == SQLITE_DONE) {
    return std::nullopt;
  }
  if (result != SQLITE_ROW) {
    throw std::runtime_error(sqlite3_errmsg(connection.get()));
  }

  Session session{
      .session_key = statement.Text(0),
      .user_id = statement.Text(1),
      .data = statement.Text(2),
      .created_at = statement.Integer(3),
      .expires_at = statement.Integer(4),
      .revoked_at = std::nullopt,
      .reason_revoked = statement.Text(6),
  };
  if (!statement.IsNull(5)) {
    session.revoked_at = statement.Integer(5);
  }
  return session;
}

bool Database::RevokeSession(std::string_view session_key,
                             std::int64_t revoked_at,
                             std::string_view reason) const {
  Connection connection{path_};
  Statement statement{connection.get(),
                      "UPDATE sessions SET "
                      "reason_revoked = CASE WHEN revoked_at IS NULL THEN ? "
                      "ELSE reason_revoked END, "
                      "revoked_at = COALESCE(revoked_at, ?) "
                      "WHERE session_key = ?"};
  statement.BindText(1, reason);
  statement.BindInteger(2, revoked_at);
  statement.BindText(3, session_key);
  RequireDone(connection.get(), statement);
  return sqlite3_changes(connection.get()) != 0;
}

void Database::TouchInstance(std::string_view machine_id,
                             std::string_view internal_ip,
                             std::string_view region,
                             std::string_view request_type,
                             std::int64_t seen_at) const {
  Connection connection{path_};
  Statement statement{
      connection.get(),
      "INSERT INTO active_instances "
      "(machine_id, internal_ip, region, last_seen, last_request_type) "
      "VALUES (?, ?, ?, ?, ?) "
      "ON CONFLICT(machine_id) DO UPDATE SET "
      "internal_ip = CASE WHEN excluded.internal_ip <> '' "
      "THEN excluded.internal_ip ELSE active_instances.internal_ip END, "
      "region = CASE WHEN excluded.region <> '' "
      "THEN excluded.region ELSE active_instances.region END, "
      "last_seen = excluded.last_seen, "
      "last_request_type = excluded.last_request_type"};
  statement.BindText(1, machine_id);
  statement.BindText(2, internal_ip);
  statement.BindText(3, region);
  statement.BindInteger(4, seen_at);
  statement.BindText(5, request_type);
  RequireDone(connection.get(), statement);
}

} // namespace purrboss::storage
