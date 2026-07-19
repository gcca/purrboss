/**
 * @file session.hpp
 * @brief C++ implementation boundary for purrboss.v1.SessionService.
 *
 * The Protobuf package carries the major version. The implementation class
 * therefore remains SessionService instead of repeating the version as
 * SessionServiceV1.
 */

#pragma once

#include <cstdint>
#include <stdexcept>

#include "purrboss/storage/database.hpp"
#include "session.grpc.pb.h"

namespace purrboss::services {

/**
 * @brief Implements the authoritative version 1 session service.
 *
 * Each unary RPC follows the same outer request path:
 *
 * @code{.text}
 * gRPC request
 *      |
 *      +--> update active instance when x-machine-id is present
 *      |
 *      +--> validate input --> execute SQLite operation --> build response
 * @endcode
 *
 * Database connections are managed by storage::Database. gRPC may invoke
 * methods concurrently; the service itself owns no mutable request state.
 */
class SessionService final : public v1::SessionService::Service {
public:
  /**
   * @brief Creates a service backed by the given authoritative database.
   *
   * @param database Database object that outlives this service.
   * @param default_ttl_seconds Lifetime used when a create request sends zero.
   * @throws std::invalid_argument When the default TTL is outside the accepted
   * range of 1 through 315,360,000 seconds.
   */
  SessionService(const storage::Database &database,
                 std::int64_t default_ttl_seconds)
      : database_{database}, default_ttl_seconds_{default_ttl_seconds} {
    if (default_ttl_seconds_ <= 0 ||
        default_ttl_seconds_ > kMaximumTtlSeconds) {
      throw std::invalid_argument(
          "default session TTL must be between 1 and 315360000 seconds");
    }
  }

  /**
   * @brief Creates and persists a new opaque session.
   *
   * The operation is not idempotent: a repeated successful request creates a
   * second session.
   */
  grpc::Status CreateSession(grpc::ServerContext *context,
                             const v1::CreateSessionRequest *request,
                             v1::CreateSessionResponse *response) override;

  /**
   * @brief Reads authoritative session state from SQLite.
   *
   * Missing, expired, and revoked sessions return an OK gRPC status with an
   * invalid business response. Malformed input and storage failures use gRPC
   * error statuses.
   */
  grpc::Status ValidateSession(grpc::ServerContext *context,
                               const v1::ValidateSessionRequest *request,
                               v1::ValidateSessionResponse *response) override;

  /**
   * @brief Soft-revokes a session without deleting its audit row.
   *
   * Repeated calls preserve the first revocation time and reason.
   */
  grpc::Status
  InvalidateSession(grpc::ServerContext *context,
                    const v1::InvalidateSessionRequest *request,
                    v1::InvalidateSessionResponse *response) override;

  /**
   * @brief Returns application liveness and the current server time.
   *
   * This is separate from the standard gRPC health service enabled by main.
   */
  grpc::Status Ping(grpc::ServerContext *context,
                    const v1::PingRequest *request,
                    v1::PingResponse *response) override;

private:
  /// Largest accepted session lifetime: ten years in seconds.
  static constexpr std::int64_t kMaximumTtlSeconds = 10LL * 365 * 24 * 60 * 60;

  /// Non-owning reference; main keeps the database object alive.
  const storage::Database &database_;

  /// Lifetime used when CreateSessionRequest.ttl_seconds is zero.
  std::int64_t default_ttl_seconds_;
};

} // namespace purrboss::services
