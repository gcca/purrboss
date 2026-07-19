/**
 * @file invalidate_session.cpp
 * @brief InvalidateSession handler for purrboss.v1.SessionService.
 *
 * @code{.text}
 * session key --> soft revoke in SQLite --> success or not found
 * @endcode
 */

#include <string>

#include "purrboss/services/session.hpp"
#include "purrboss/services/session/tools.hpp"

namespace purrboss::services {

grpc::Status
SessionService::InvalidateSession(grpc::ServerContext *context,
                                  const v1::InvalidateSessionRequest *request,
                                  v1::InvalidateSessionResponse *response) {
  if (const auto status =
          session::TrackInstance(database_, context, "invalidate");
      !status.ok()) {
    return status;
  }
  if (request->session_key().empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT, "session_key is required"};
  }

  const std::string reason =
      request->reason().empty() ? "unspecified" : request->reason();
  try {
    response->set_success(database_.RevokeSession(request->session_key(),
                                                  session::UnixTime(), reason));
    return grpc::Status::OK;
  } catch (const std::exception &error) {
    return session::DatabaseFailure(error);
  }
}

} // namespace purrboss::services
