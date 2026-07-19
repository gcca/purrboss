/**
 * @file ping.cpp
 * @brief Ping handler for purrboss.v1.SessionService.
 *
 * @code{.text}
 * caller --> track instance --> ok + server time
 * @endcode
 */

#include "purrboss/services/session.hpp"
#include "purrboss/services/session/tools.hpp"

namespace purrboss::services {

grpc::Status SessionService::Ping(grpc::ServerContext *context,
                                  const v1::PingRequest *,
                                  v1::PingResponse *response) {
  if (const auto status = session::TrackInstance(database_, context, "ping");
      !status.ok()) {
    return status;
  }
  response->set_ok(true);
  response->mutable_server_time()->set_seconds(session::UnixTime());
  return grpc::Status::OK;
}

} // namespace purrboss::services
