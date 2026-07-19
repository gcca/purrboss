/**
 * @file tools.hpp
 * @brief Shared internal operations used by the v1 session handlers.
 */

#pragma once

#include <cstdint>
#include <exception>
#include <string_view>

#include <grpcpp/grpcpp.h>

#include "purrboss/storage/database.hpp"

namespace purrboss::services::session {

/** Returns the current Unix timestamp in whole seconds. */
std::int64_t UnixTime();

/** Logs an internal storage error and returns a sanitized client status. */
grpc::Status DatabaseFailure(const std::exception &error);

/**
 * @brief Upserts caller activity from optional gRPC metadata.
 *
 * Tracking is skipped when x-machine-id is absent. A tracking database
 * failure fails the RPC so operational desynchronization remains visible.
 */
grpc::Status TrackInstance(const storage::Database &database,
                           grpc::ServerContext *context,
                           std::string_view request_type);

} // namespace purrboss::services::session
