/**
 * @file auth.hpp
 * @brief C++ implementation boundary for purrboss.v1.AuthService.
 */

#pragma once

#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "auth.grpc.pb.h"
#include "purrboss/storage/db.hpp"

namespace purrboss::services {

/**
 * @brief Implements the authoritative version 1 authentication service.
 *
 * Both RPCs read the auth_user row through storage::Database. Authenticate
 * additionally verifies the supplied password against the stored Argon2d hash
 * and advances last_logged_at on success. gRPC may invoke methods
 * concurrently; the service owns no mutable request state.
 */
class AuthService final : public v1::AuthService::Service {
public:
  /**
   * @brief Creates a service that owns its own handle to the database file.
   *
   * @param db_path Path to the authoritative SQLite database file. The service
   * opens its own per-operation connections; the file must already be
   * schema-migrated and WAL-initialized (main does this at startup).
   */
  explicit AuthService(std::string db_path)
      : database_{storage::Database::ReadWrite(std::move(db_path))} {}

  /**
   * @brief Returns non-secret profile facts for a user.
   *
   * A missing user is a NOT_FOUND gRPC error; storage failures use INTERNAL.
   */
  grpc::Status UserInfo(grpc::ServerContext *context,
                        const v1::UserInfoRequest *request,
                        v1::UserInfoResponse *response) override;

  /**
   * @brief Verifies a username/password pair and returns the user's profile.
   *
   * A wrong password is UNAUTHENTICATED, a missing user is NOT_FOUND, and a
   * successful call advances last_logged_at to the current server time. The
   * supplied password is never written to logs.
   */
  grpc::Status Authenticate(grpc::ServerContext *context,
                            const v1::AuthenticateRequest *request,
                            v1::AuthenticateResponse *response) override;

private:
  /// Copies non-secret profile fields from a stored row into the wire message.
  void ToUserInfo(const storage::AuthUser &user, v1::UserInfo *userinfo) const;

  /// This service's own database handle, opened per operation from the path.
  storage::Database database_;
};

} // namespace purrboss::services
