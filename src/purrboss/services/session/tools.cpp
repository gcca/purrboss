/**
 * @file tools.cpp
 * @brief Shared time, error, and instance-tracking operations.
 *
 * @code{.text}
 * gRPC metadata --> x-machine-id present? --> active_instances upsert
 *                         |
 *                         +--> absent: skip tracking
 * @endcode
 */

#include "purrboss/services/session/tools.hpp"

#include <chrono>
#include <cstdio>
#include <print>
#include <string>

namespace {

/** Reads one lowercase gRPC metadata entry, returning empty when absent. */
std::string ClientMetadata(grpc::ServerContext *context,
                           std::string_view name) {
  const grpc::string_ref key{name.data(), name.size()};
  const auto value = context->client_metadata().find(key);
  if (value == context->client_metadata().end()) {
    return {};
  }
  return {value->second.data(), value->second.size()};
}

} // namespace

namespace purrboss::services::session {

std::int64_t UnixTime() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

grpc::Status DatabaseFailure(const std::exception &error) {
  std::println(stderr, "purrboss: database error: {}", error.what());
  return {grpc::StatusCode::INTERNAL, "database operation failed"};
}

grpc::Status TrackInstance(const storage::Database &database,
                           grpc::ServerContext *context,
                           std::string_view request_type) {
  const std::string machine_id = ClientMetadata(context, "x-machine-id");
  if (machine_id.empty()) {
    return grpc::Status::OK;
  }

  try {
    database.TouchInstance(machine_id, ClientMetadata(context, "x-internal-ip"),
                           ClientMetadata(context, "x-region"), request_type,
                           UnixTime());
    return grpc::Status::OK;
  } catch (const std::exception &error) {
    return DatabaseFailure(error);
  }
}

} // namespace purrboss::services::session
