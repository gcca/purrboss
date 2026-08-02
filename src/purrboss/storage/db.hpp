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

struct AuthUser {
  std::string username;
  std::string password;
  std::string email;
  bool is_active;
  std::int64_t created_at;
  std::int64_t last_logged_at;
};

class Database {
public:
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

  [[nodiscard]] std::optional<AuthUser>
  FindAuthUser(std::string_view username) const;

  void UpdateLastLogin(std::string_view username, std::int64_t when) const;

  [[nodiscard]] static Database ReadOnly(std::string path);
  [[nodiscard]] static Database ReadWrite(std::string path);

private:
  Database(std::string path, int open_flags, const char *setup_pragmas);

  std::string path_;
  int open_flags_;
  const char *setup_pragmas_;
};

} // namespace purrboss::storage
