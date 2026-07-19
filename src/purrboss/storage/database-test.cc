#include <cstdint>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <sqlite3.h>

#include "purrboss/storage/database.hpp"
#include "purrboss/testing/sqlite_test_database.hpp"

namespace {

using purrboss::storage::Session;
using purrboss::testing::SqliteTestDatabase;

class DatabaseTest : public SqliteTestDatabase {};

TEST_F(DatabaseTest, InitializeEnablesWalJournalMode) {
  sqlite3 *handle = nullptr;
  ASSERT_EQ(sqlite3_open(database_path_.c_str(), &handle), SQLITE_OK);
  sqlite3_stmt *statement = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(handle, "PRAGMA journal_mode;", -1, &statement,
                               nullptr),
            SQLITE_OK);
  ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
  const std::string mode =
      reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));
  sqlite3_finalize(statement);
  sqlite3_close(handle);

  EXPECT_EQ(mode, "wal");
}

TEST_F(DatabaseTest, CreateSessionThenFindSessionReturnsStoredFields) {
  const Session session{
      .session_key = "purrboss.v1_test_key",
      .user_id = "user-1",
      .data = R"({"role":"admin"})",
      .created_at = 1000,
      .expires_at = 2000,
      .revoked_at = std::nullopt,
      .reason_revoked = {},
  };
  database_->CreateSession(session);

  const auto found = database_->FindSession("purrboss.v1_test_key");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->user_id, "user-1");
  EXPECT_EQ(found->data, R"({"role":"admin"})");
  EXPECT_EQ(found->created_at, 1000);
  EXPECT_EQ(found->expires_at, 2000);
  EXPECT_FALSE(found->revoked_at.has_value());
  EXPECT_EQ(found->reason_revoked, "");
}

TEST_F(DatabaseTest, FindSessionMissingReturnsNullopt) {
  EXPECT_FALSE(database_->FindSession("purrboss.v1_missing").has_value());
}

TEST_F(DatabaseTest, RevokeSessionPreservesFirstReasonAndTimestamp) {
  database_->CreateSession(Session{
      .session_key = "purrboss.v1_revoke",
      .user_id = "user-2",
      .data = "{}",
      .created_at = 1000,
      .expires_at = 5000,
      .revoked_at = std::nullopt,
      .reason_revoked = {},
  });

  EXPECT_TRUE(database_->RevokeSession("purrboss.v1_revoke", 1500, "logout"));
  EXPECT_TRUE(database_->RevokeSession("purrboss.v1_revoke", 1600, "security"));

  const auto found = database_->FindSession("purrboss.v1_revoke");
  ASSERT_TRUE(found.has_value());
  ASSERT_TRUE(found->revoked_at.has_value());
  EXPECT_EQ(*found->revoked_at, 1500);
  EXPECT_EQ(found->reason_revoked, "logout");
}

TEST_F(DatabaseTest, RevokeSessionMissingReturnsFalse) {
  EXPECT_FALSE(database_->RevokeSession("purrboss.v1_absent", 1500, "logout"));
}

TEST_F(DatabaseTest, TouchInstanceUpsertPreservesKnownFieldsWhenOmitted) {
  database_->TouchInstance("machine-1", "10.0.0.1", "us-east", "ping", 100);
  database_->TouchInstance("machine-1", "", "", "validate", 200);

  sqlite3 *handle = nullptr;
  ASSERT_EQ(sqlite3_open(database_path_.c_str(), &handle), SQLITE_OK);
  sqlite3_stmt *statement = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(
                handle,
                "SELECT internal_ip, region, last_seen, last_request_type "
                "FROM active_instances WHERE machine_id = ?",
                -1, &statement, nullptr),
            SQLITE_OK);
  sqlite3_bind_text(statement, 1, "machine-1", -1, SQLITE_TRANSIENT);
  ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
  const std::string internal_ip =
      reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));
  const std::string region =
      reinterpret_cast<const char *>(sqlite3_column_text(statement, 1));
  const std::int64_t last_seen = sqlite3_column_int64(statement, 2);
  const std::string last_request_type =
      reinterpret_cast<const char *>(sqlite3_column_text(statement, 3));
  sqlite3_finalize(statement);
  sqlite3_close(handle);

  EXPECT_EQ(internal_ip, "10.0.0.1");
  EXPECT_EQ(region, "us-east");
  EXPECT_EQ(last_seen, 200);
  EXPECT_EQ(last_request_type, "validate");
}

} // namespace
