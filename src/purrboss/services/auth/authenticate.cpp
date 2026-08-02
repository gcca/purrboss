/**
 * @file authenticate.cpp
 * @brief Authenticate handler for purrboss.v1.AuthService.
 *
 * @code{.text}
 * request --> validate --> SELECT auth_user --> Argon2d verify
 *                                                    |
 *                                     ok             | fail
 *                                     v              v
 *                        UPDATE last_logged_at   UNAUTHENTICATED
 *                                     |
 * response <-- authenticated + profile+
 * @endcode
 */

#include <chrono>
#include <cstdio>
#include <print>

#include <argon2.h>

#include "purrboss/services/auth.hpp"

namespace {

std::int64_t UnixTime() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

} // namespace

namespace purrboss::services {

grpc::Status AuthService::Authenticate(grpc::ServerContext *,
                                       const v1::AuthenticateRequest *request,
                                       v1::AuthenticateResponse *response) {
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
    response->set_authenticated(false);
    response->clear_userinfo();
    return {grpc::StatusCode::NOT_FOUND, "user not found"};
  }

  const std::string prepared_password =
      request->password() + request->username();
  const int verify =
      argon2d_verify(user->password.c_str(), prepared_password.data(),
                     prepared_password.size());
  if (verify != ARGON2_OK) {
    response->set_authenticated(false);
    response->clear_userinfo();
    return {grpc::StatusCode::UNAUTHENTICATED, "invalid password"};
  }

  const std::int64_t now = UnixTime();
  try {
    database_.UpdateLastLogin(user->username, now);
  } catch (const std::exception &error) {
    std::println(stderr, "purrboss: database error: {}", error.what());
    return {grpc::StatusCode::INTERNAL, "database operation failed"};
  }
  user->last_logged_at = now;

  response->set_authenticated(true);
  ToUserInfo(*user, response->mutable_userinfo());
  return grpc::Status::OK;
}

} // namespace purrboss::services
