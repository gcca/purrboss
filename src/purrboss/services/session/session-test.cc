#include <memory>

#include <gtest/gtest.h>

#include "purrboss/services/session.hpp"
#include "purrboss/testing/sqlite_test_database.hpp"

namespace {

using purrboss::services::SessionService;
using purrboss::testing::SqliteTestDatabase;

class SessionServiceTest : public SqliteTestDatabase {
protected:
  void SetUp() override {
    SqliteTestDatabase::SetUp();
    service_ = std::make_unique<SessionService>(*database_, 3600);
  }

  std::unique_ptr<SessionService> service_;
};

TEST_F(SessionServiceTest, PingReturnsOkAndServerTime) {
  grpc::ServerContext context;
  purrboss::v1::PingRequest request;
  purrboss::v1::PingResponse response;

  const grpc::Status status = service_->Ping(&context, &request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(response.ok());
  EXPECT_GT(response.server_time().seconds(), 0);
}

TEST_F(SessionServiceTest, CreateSessionRejectsEmptyUserId) {
  grpc::ServerContext context;
  purrboss::v1::CreateSessionRequest request;
  purrboss::v1::CreateSessionResponse response;

  const grpc::Status status =
      service_->CreateSession(&context, &request, &response);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_F(SessionServiceTest, CreateSessionThenValidateSessionSucceeds) {
  grpc::ServerContext create_context;
  purrboss::v1::CreateSessionRequest create_request;
  create_request.set_user_id("user-1");
  (*create_request.mutable_metadata())["role"] = "admin";
  purrboss::v1::CreateSessionResponse create_response;

  ASSERT_TRUE(
      service_
          ->CreateSession(&create_context, &create_request, &create_response)
          .ok());
  EXPECT_FALSE(create_response.session_key().empty());
  EXPECT_EQ(create_response.session_data().at("role"), "admin");

  grpc::ServerContext validate_context;
  purrboss::v1::ValidateSessionRequest validate_request;
  validate_request.set_session_key(create_response.session_key());
  purrboss::v1::ValidateSessionResponse validate_response;

  ASSERT_TRUE(service_
                  ->ValidateSession(&validate_context, &validate_request,
                                    &validate_response)
                  .ok());
  EXPECT_TRUE(validate_response.valid());
  EXPECT_EQ(validate_response.reason(), "valid");
  EXPECT_EQ(validate_response.user_id(), "user-1");
  EXPECT_EQ(validate_response.session_data().at("role"), "admin");
}

TEST_F(SessionServiceTest, ValidateSessionReturnsNotFoundForUnknownKey) {
  grpc::ServerContext context;
  purrboss::v1::ValidateSessionRequest request;
  request.set_session_key("sess_unknown");
  purrboss::v1::ValidateSessionResponse response;

  ASSERT_TRUE(service_->ValidateSession(&context, &request, &response).ok());
  EXPECT_FALSE(response.valid());
  EXPECT_EQ(response.reason(), "not_found");
}

TEST_F(SessionServiceTest, InvalidateSessionThenValidateReturnsRevoked) {
  grpc::ServerContext create_context;
  purrboss::v1::CreateSessionRequest create_request;
  create_request.set_user_id("user-2");
  purrboss::v1::CreateSessionResponse create_response;
  ASSERT_TRUE(
      service_
          ->CreateSession(&create_context, &create_request, &create_response)
          .ok());

  grpc::ServerContext invalidate_context;
  purrboss::v1::InvalidateSessionRequest invalidate_request;
  invalidate_request.set_session_key(create_response.session_key());
  invalidate_request.set_reason("logout");
  purrboss::v1::InvalidateSessionResponse invalidate_response;
  ASSERT_TRUE(service_
                  ->InvalidateSession(&invalidate_context, &invalidate_request,
                                      &invalidate_response)
                  .ok());
  EXPECT_TRUE(invalidate_response.success());

  // A repeated invalidation with a different reason must not overwrite the
  // first recorded reason.
  grpc::ServerContext second_context;
  purrboss::v1::InvalidateSessionRequest second_request;
  second_request.set_session_key(create_response.session_key());
  second_request.set_reason("security");
  purrboss::v1::InvalidateSessionResponse second_response;
  ASSERT_TRUE(service_
                  ->InvalidateSession(&second_context, &second_request,
                                      &second_response)
                  .ok());
  EXPECT_TRUE(second_response.success());

  grpc::ServerContext validate_context;
  purrboss::v1::ValidateSessionRequest validate_request;
  validate_request.set_session_key(create_response.session_key());
  purrboss::v1::ValidateSessionResponse validate_response;
  ASSERT_TRUE(service_
                  ->ValidateSession(&validate_context, &validate_request,
                                    &validate_response)
                  .ok());
  EXPECT_FALSE(validate_response.valid());
  EXPECT_EQ(validate_response.reason(), "revoked");
}

TEST_F(SessionServiceTest, InvalidateSessionReturnsFalseForUnknownKey) {
  grpc::ServerContext context;
  purrboss::v1::InvalidateSessionRequest request;
  request.set_session_key("sess_unknown");
  purrboss::v1::InvalidateSessionResponse response;

  ASSERT_TRUE(service_->InvalidateSession(&context, &request, &response).ok());
  EXPECT_FALSE(response.success());
}

} // namespace
