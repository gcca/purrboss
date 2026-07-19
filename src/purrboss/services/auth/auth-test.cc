/**
 * @file auth-test.cc
 * @brief End-to-end tests for purrboss.v1.AuthService over a real SQLite file.
 */

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <argon2.h>
#include <gtest/gtest.h>
#include <sqlite3.h>

#include "purrboss/services/auth.hpp"
#include "purrboss/testing/sqlite_test_database.hpp"

namespace {

using purrboss::services::AuthService;
using purrboss::testing::SqliteTestDatabase;

std::string PasswordHash(const std::string &username,
                         const std::string &password) {
  static constexpr std::uint32_t kTimeCost = 1;
  static constexpr std::uint32_t kMemoryCost = 32;
  static constexpr std::uint32_t kParallelism = 1;
  static constexpr std::uint32_t kHashLength = 32;
  static constexpr std::array<unsigned char, 16> kSalt = {
      0x70, 0x75, 0x72, 0x72, 0x62, 0x6f, 0x73, 0x73,
      0x2d, 0x61, 0x75, 0x74, 0x68, 0x2d, 0x74, 0x73,
  };

  const std::string prepared_password = password + username;
  std::vector<char> encoded(argon2_encodedlen(kTimeCost, kMemoryCost,
                                              kParallelism, kSalt.size(),
                                              kHashLength, Argon2_d));

  const int result = argon2d_hash_encoded(
      kTimeCost, kMemoryCost, kParallelism, prepared_password.data(),
      prepared_password.size(), kSalt.data(), kSalt.size(), kHashLength,
      encoded.data(), encoded.size());
  if (result != ARGON2_OK) {
    throw std::runtime_error(argon2_error_message(result));
  }

  return encoded.data();
}

class AuthServiceTest : public SqliteTestDatabase {
protected:
  void SetUp() override {
    SqliteTestDatabase::SetUp();
    service_ = std::make_unique<AuthService>(database_path_);

    InsertUser("alice", "correct horse", "alice@example.com", true, 1000, 1000);
    InsertUser("bob", "another password", "", false, 2000, 2000);
  }

  void InsertUser(const std::string &username, const std::string &password,
                  const std::string &email, bool is_active,
                  std::int64_t created_at, std::int64_t last_logged_at) {
    sqlite3 *handle = nullptr;
    ASSERT_EQ(sqlite3_open(database_path_.c_str(), &handle), SQLITE_OK);

    sqlite3_stmt *stmt = nullptr;
    ASSERT_EQ(
        sqlite3_prepare_v2(handle,
                           "INSERT INTO auth_user (username, password, email, "
                           "is_active, created_at, last_logged_at) "
                           "VALUES (?, ?, ?, ?, ?, ?)",
                           -1, &stmt, nullptr),
        SQLITE_OK);

    const std::string password_hash = PasswordHash(username, password);
    ASSERT_EQ(sqlite3_bind_text(stmt, 1, username.data(), username.size(),
                                SQLITE_TRANSIENT),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_text(stmt, 2, password_hash.data(),
                                password_hash.size(), SQLITE_TRANSIENT),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_text(stmt, 3, email.data(), email.size(),
                                SQLITE_TRANSIENT),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int(stmt, 4, is_active ? 1 : 0), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int64(stmt, 5, created_at), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_int64(stmt, 6, last_logged_at), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_DONE);
    ASSERT_EQ(sqlite3_finalize(stmt), SQLITE_OK);
    ASSERT_EQ(sqlite3_close(handle), SQLITE_OK);
  }

  grpc::Status UserInfo(const std::string &username) {
    purrboss::v1::UserInfoRequest request;
    request.set_username(username);
    user_info_response_.Clear();
    return service_->UserInfo(nullptr, &request, &user_info_response_);
  }

  grpc::Status Authenticate(const std::string &username,
                            const std::string &password) {
    purrboss::v1::AuthenticateRequest request;
    request.set_username(username);
    request.set_password(password);
    authenticate_response_.Clear();
    return service_->Authenticate(nullptr, &request, &authenticate_response_);
  }

  std::unique_ptr<AuthService> service_;
  purrboss::v1::UserInfoResponse user_info_response_;
  purrboss::v1::AuthenticateResponse authenticate_response_;
};

TEST_F(AuthServiceTest, UsesVersionedWireServiceName) {
  EXPECT_STREQ(purrboss::v1::AuthService::service_full_name(),
               "purrboss.v1.AuthService");
}

TEST_F(AuthServiceTest, UserInfoReturnsActiveUserProfile) {
  const grpc::Status status = UserInfo("alice");

  ASSERT_TRUE(status.ok()) << status.error_message();
  ASSERT_TRUE(user_info_response_.has_userinfo());
  EXPECT_TRUE(user_info_response_.userinfo().is_active());
  EXPECT_EQ(user_info_response_.userinfo().email(), "alice@example.com");
  EXPECT_EQ(user_info_response_.userinfo().created_at().seconds(), 1000);
  EXPECT_EQ(user_info_response_.userinfo().last_logged_at().seconds(), 1000);
}

TEST_F(AuthServiceTest, UserInfoReturnsInactiveUser) {
  const grpc::Status status = UserInfo("bob");

  ASSERT_TRUE(status.ok()) << status.error_message();
  ASSERT_TRUE(user_info_response_.has_userinfo());
  EXPECT_FALSE(user_info_response_.userinfo().is_active());
  EXPECT_TRUE(user_info_response_.userinfo().email().empty());
}

TEST_F(AuthServiceTest, UserInfoReturnsNotFoundForMissingUser) {
  const grpc::Status status = UserInfo("carol");

  EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
  EXPECT_EQ(status.error_message(), "user not found");
  EXPECT_FALSE(user_info_response_.has_userinfo());
}

TEST_F(AuthServiceTest, UserInfoRejectsEmptyUsername) {
  const grpc::Status status = UserInfo("");

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_F(AuthServiceTest, AuthenticateReturnsProfileForValidPassword) {
  const grpc::Status status = Authenticate("alice", "correct horse");

  ASSERT_TRUE(status.ok()) << status.error_message();
  EXPECT_TRUE(authenticate_response_.authenticated());
  ASSERT_TRUE(authenticate_response_.has_userinfo());
  EXPECT_TRUE(authenticate_response_.userinfo().is_active());
  EXPECT_EQ(authenticate_response_.userinfo().email(), "alice@example.com");
}

TEST_F(AuthServiceTest, AuthenticateAdvancesLastLoggedAt) {
  const grpc::Status status = Authenticate("alice", "correct horse");

  ASSERT_TRUE(status.ok()) << status.error_message();
  // The seeded value was 1000; a successful login stamps the current time.
  EXPECT_GT(authenticate_response_.userinfo().last_logged_at().seconds(), 1000);

  // The advance is persisted and visible to a later UserInfo read.
  ASSERT_TRUE(UserInfo("alice").ok());
  EXPECT_EQ(user_info_response_.userinfo().last_logged_at().seconds(),
            authenticate_response_.userinfo().last_logged_at().seconds());
}

TEST_F(AuthServiceTest, AuthenticateOmitsProfileForInvalidPassword) {
  const grpc::Status status = Authenticate("alice", "wrong password");

  EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAUTHENTICATED);
  EXPECT_FALSE(authenticate_response_.authenticated());
  EXPECT_FALSE(authenticate_response_.has_userinfo());
}

TEST_F(AuthServiceTest, AuthenticateDoesNotAdvanceLastLoggedAtOnFailure) {
  ASSERT_EQ(Authenticate("alice", "wrong password").error_code(),
            grpc::StatusCode::UNAUTHENTICATED);

  ASSERT_TRUE(UserInfo("alice").ok());
  EXPECT_EQ(user_info_response_.userinfo().last_logged_at().seconds(), 1000);
}

TEST_F(AuthServiceTest, AuthenticateReturnsNotFoundForMissingUser) {
  const grpc::Status status = Authenticate("carol", "password");

  EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
  EXPECT_FALSE(authenticate_response_.authenticated());
  EXPECT_FALSE(authenticate_response_.has_userinfo());
}

} // namespace
