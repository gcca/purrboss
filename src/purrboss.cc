#include <CLI11.hpp>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "purrboss/services/auth.hpp"
#include "purrboss/services/session.hpp"
#include "purrboss/storage/db.hpp"

namespace {

struct Options {
  std::uint16_t port = 50051;
  std::string database_path = "data/purrboss.db";
  std::int64_t default_ttl_seconds = 30LL * 24 * 60 * 60;
};

inline void InitializeDatabase(const Options &options) {
  const std::filesystem::path database_path{options.database_path};
  if (database_path.has_parent_path()) {
    std::filesystem::create_directories(database_path.parent_path());
  }

  purrboss::storage::Database database =
      purrboss::storage::Database::ReadWrite(options.database_path);
  database.Initialize();
}

} // namespace

int main(int argc, const char *argv[]) {
  Options options;
  CLI::App app{"purrboss"};

  app.add_option("-p,--port", options.port, "Listening port")
      ->check(CLI::Range(1, 65535))
      ->capture_default_str();
  app.add_option("-d,--database", options.database_path, "SQLite database path")
      ->capture_default_str();
  app.add_option("-t,--default-ttl-seconds", options.default_ttl_seconds,
                 "Session TTL when request TTL is zero")
      ->check(CLI::PositiveNumber)
      ->capture_default_str();

  CLI11_PARSE(app, argc, argv);

  try {
    InitializeDatabase(options);

    purrboss::services::SessionService session_service{
        options.database_path, options.default_ttl_seconds};
    purrboss::services::AuthService auth_service{options.database_path};

    const std::string address = "0.0.0.0:" + std::to_string(options.port);
    grpc::EnableDefaultHealthCheckService(true);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&session_service);
    builder.RegisterService(&auth_service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (server == nullptr) {
      std::println(stderr, "purrboss: unable to listen on {}", address);
      return EXIT_FAILURE;
    }

    if (auto *health_service = server->GetHealthCheckService();
        health_service != nullptr) {
      health_service->SetServingStatus(true);
    }

    std::println("purrboss listening on {} using {}", address,
                 options.database_path);

    server->Wait();

    return EXIT_SUCCESS;

  } catch (const std::exception &error) {
    std::println(stderr, "purrboss: {}", error.what());

    return EXIT_FAILURE;
  }
}
