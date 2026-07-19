#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace purrboss::storage {

struct Session {
  std::string session_key;
  std::string user_id;
  std::string data;
  std::int64_t created_at;
  std::int64_t expires_at;
  std::optional<std::int64_t> revoked_at;
  std::string reason_revoked;
};

class Database {
public:
  explicit Database(std::string path);

  void Initialize() const;

  void CreateSession(const Session &session) const;

  [[nodiscard]] std::optional<Session>
  FindSession(std::string_view session_key) const;

  [[nodiscard]] bool RevokeSession(std::string_view session_key,
                                   std::int64_t revoked_at,
                                   std::string_view reason) const;

  void TouchInstance(std::string_view machine_id, std::string_view internal_ip,
                     std::string_view region, std::string_view request_type,
                     std::int64_t seen_at) const;

private:
  std::string path_;
};

} // namespace purrboss::storage
