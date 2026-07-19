#include <chrono>
#include <stdexcept>

#include <gtest/gtest.h>

#include "purrboss/services/session/tools.hpp"
#include "purrboss/storage/database.hpp"

namespace {

using purrboss::services::session::DatabaseFailure;
using purrboss::services::session::TrackInstance;
using purrboss::services::session::UnixTime;

TEST(UnixTimeTest, ReturnsCurrentEpochSeconds) {
  const auto before = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
  const auto observed = UnixTime();
  const auto after = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

  EXPECT_GE(observed, before);
  EXPECT_LE(observed, after);
}

TEST(DatabaseFailureTest, ReturnsSanitizedInternalStatusWithoutLeakingDetail) {
  const grpc::Status status =
      DatabaseFailure(std::runtime_error("disk full at /secret/path"));

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
  EXPECT_EQ(status.error_message(), "database operation failed");
}

TEST(TrackInstanceTest, SkipsTrackingWhenMachineIdMetadataAbsent) {
  purrboss::storage::Database database{"/nonexistent/purrboss-unused.db"};
  grpc::ServerContext context;

  const grpc::Status status = TrackInstance(database, &context, "ping");

  EXPECT_TRUE(status.ok());
}

} // namespace
