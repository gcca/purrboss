/**
 * @file user_info.cpp
 * @brief UserInfo handler for purrboss.v1.AuthService.
 *
 * @code{.text}
 * request --> validate --> SELECT auth_user --> build profile
 *                                            |
 * response <-- non-secret profile <----------+
 * @endcode
 */

#include <cstdio>
#include <exception>
#include <optional>
#include <print>

#include "purrboss/services/auth.hpp"

namespace purrboss::services {

void AuthService::ToUserInfo(const storage::AuthUser &user,
                             v1::UserInfo *userinfo) const {
  userinfo->set_is_active(user.is_active);
  userinfo->set_email(user.email);
  userinfo->mutable_created_at()->set_seconds(user.created_at);
  userinfo->mutable_last_logged_at()->set_seconds(user.last_logged_at);
}

grpc::Status AuthService::UserInfo(grpc::ServerContext *,
                                   const v1::UserInfoRequest *request,
                                   v1::UserInfoResponse *response) {
  if (request->username().empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT, "username is required"};
  }

  std::optional<storage::AuthUser> user;
  try {
    user = database_.FindAuthUser(request->username());
  } catch (const std::exception &error) {
    std::println(stderr, "purrboss: database error: {}", error.what());
    return {grpc::StatusCode::INTERNAL, "database operation failed"};
  }

  if (!user.has_value()) {
    return {grpc::StatusCode::NOT_FOUND, "user not found"};
  }

  ToUserInfo(*user, response->mutable_userinfo());
  return grpc::Status::OK;
}

} // namespace purrboss::services
