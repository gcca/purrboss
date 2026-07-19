/**
 * @file create_session.cpp
 * @brief CreateSession handler for purrboss.v1.SessionService.
 *
 * @code{.text}
 * request --> validate --> generate key --> serialize metadata --> SQLite
 *                                                               |
 * response <-- key + expiration + metadata <--------------------+
 * @endcode
 */

#include <array>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/json_util.h>

#include "purrboss/services/session.hpp"
#include "purrboss/services/session/tools.hpp"

namespace {

/**
 * Generates an opaque key with a purrboss.v1_ prefix and 256 bits of source
 * data.
 *
 * The target runtime must provide an OS-backed std::random_device before this
 * implementation is considered production-ready.
 */
std::string GenerateSessionKey() {
  constexpr char digits[] = "0123456789abcdef";
  std::array<unsigned char, 32> random_bytes{};
  std::random_device random_source;
  for (auto &byte : random_bytes) {
    byte = static_cast<unsigned char>(random_source());
  }

  std::string key = "purrboss.v1_";
  key.reserve(key.size() + random_bytes.size() * 2);
  for (const unsigned char byte : random_bytes) {
    key.push_back(digits[byte >> 4]);
    key.push_back(digits[byte & 0x0f]);
  }
  return key;
}

/** Converts string metadata to the JSON object persisted in SQLite. */
std::string MetadataToJson(
    const google::protobuf::Map<std::string, std::string> &metadata) {
  google::protobuf::Struct document;
  for (const auto &[key, value] : metadata) {
    (*document.mutable_fields())[key].set_string_value(value);
  }

  std::string json;
  const auto status =
      google::protobuf::util::MessageToJsonString(document, &json);
  if (!status.ok()) {
    throw std::runtime_error("unable to serialize session metadata");
  }
  return json;
}

} // namespace

namespace purrboss::services {

grpc::Status
SessionService::CreateSession(grpc::ServerContext *context,
                              const v1::CreateSessionRequest *request,
                              v1::CreateSessionResponse *response) {
  if (const auto status = session::TrackInstance(database_, context, "create");
      !status.ok()) {
    return status;
  }
  if (request->user_id().empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT, "user_id is required"};
  }

  const std::int64_t ttl = request->ttl_seconds() == 0 ? default_ttl_seconds_
                                                       : request->ttl_seconds();
  if (ttl <= 0 || ttl > kMaximumTtlSeconds) {
    return {grpc::StatusCode::INVALID_ARGUMENT,
            "ttl_seconds must be between 1 and 315360000"};
  }

  const std::int64_t now = session::UnixTime();
  storage::Session stored_session{
      .session_key = GenerateSessionKey(),
      .user_id = request->user_id(),
      .data = {},
      .created_at = now,
      .expires_at = now + ttl,
      .revoked_at = std::nullopt,
      .reason_revoked = {},
  };

  try {
    stored_session.data = MetadataToJson(request->metadata());
    database_.CreateSession(stored_session);
  } catch (const std::exception &error) {
    return session::DatabaseFailure(error);
  }

  response->set_session_key(stored_session.session_key);
  response->mutable_expires_at()->set_seconds(stored_session.expires_at);
  *response->mutable_session_data() = request->metadata();
  return grpc::Status::OK;
}

} // namespace purrboss::services
