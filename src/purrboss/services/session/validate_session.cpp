/**
 * @file validate_session.cpp
 * @brief ValidateSession handler for purrboss.v1.SessionService.
 *
 * @code{.text}
 * session key --> SQLite lookup --> missing  --> not_found
 *                              +--> revoked  --> revoked
 *                              +--> expired  --> expired
 *                              +--> active   --> valid session data
 * @endcode
 */

#include <stdexcept>
#include <string>
#include <string_view>

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/json_util.h>

#include "purrboss/services/session.hpp"
#include "purrboss/services/session/tools.hpp"

namespace {

/** Restores persisted JSON string values into a generated Protobuf map. */
void JsonToMetadata(std::string_view json,
                    google::protobuf::Map<std::string, std::string> *metadata) {
  google::protobuf::Struct document;
  const auto status =
      google::protobuf::util::JsonStringToMessage(json, &document);
  if (!status.ok()) {
    throw std::runtime_error("unable to parse stored session metadata");
  }

  for (const auto &[key, value] : document.fields()) {
    if (value.kind_case() != google::protobuf::Value::kStringValue) {
      throw std::runtime_error("stored session metadata is not textual");
    }
    (*metadata)[key] = value.string_value();
  }
}

} // namespace

namespace purrboss::services {

grpc::Status
SessionService::ValidateSession(grpc::ServerContext *context,
                                const v1::ValidateSessionRequest *request,
                                v1::ValidateSessionResponse *response) {
  if (const auto status =
          session::TrackInstance(database_, context, "validate");
      !status.ok()) {
    return status;
  }
  if (request->session_key().empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT, "session_key is required"};
  }

  try {
    const auto stored_session = database_.FindSession(request->session_key());
    if (!stored_session.has_value()) {
      response->set_valid(false);
      response->set_reason("not_found");
      return grpc::Status::OK;
    }
    if (stored_session->revoked_at.has_value()) {
      response->set_valid(false);
      response->set_reason("revoked");
      return grpc::Status::OK;
    }
    if (stored_session->expires_at <= session::UnixTime()) {
      response->set_valid(false);
      response->set_reason("expired");
      return grpc::Status::OK;
    }

    response->set_valid(true);
    response->set_user_id(stored_session->user_id);
    response->mutable_expires_at()->set_seconds(stored_session->expires_at);
    JsonToMetadata(stored_session->data, response->mutable_session_data());
    response->set_reason("valid");
    return grpc::Status::OK;
  } catch (const std::exception &error) {
    return session::DatabaseFailure(error);
  }
}

} // namespace purrboss::services
