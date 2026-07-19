/**
 * @file sqlite_test_database.hpp
 * @brief Test-only fixture that provisions a schema-migrated SQLite file.
 *
 * purrboss::storage::Database::Initialize() only tunes PRAGMAs; the intended
 * full architecture applies db/schema.sql through dbmate before the process
 * starts (see .md). This fixture reproduces that step for tests by
 * executing the same schema file against a fresh temporary database.
 */

#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <sqlite3.h>

#include "purrboss/storage/database.hpp"

#ifndef PURRBOSS_SCHEMA_SQL_PATH
#error                                                                         \
    "PURRBOSS_SCHEMA_SQL_PATH must be defined by the build (see CMakeLists.txt)"
#endif

namespace purrboss::testing {

inline std::string UniqueTempDatabasePath() {
  const auto *test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  std::string name = "purrboss-test";
  if (test_info != nullptr) {
    name += "-";
    name += test_info->test_suite_name();
    name += "-";
    name += test_info->name();
  }
  for (char &character : name) {
    if (std::isalnum(static_cast<unsigned char>(character)) == 0 &&
        character != '-') {
      character = '_';
    }
  }
  return (std::filesystem::temp_directory_path() / (name + ".db")).string();
}

inline void ApplySchema(const std::string &database_path) {
  sqlite3 *handle = nullptr;
  if (sqlite3_open(database_path.c_str(), &handle) != SQLITE_OK) {
    const std::string message =
        handle != nullptr ? sqlite3_errmsg(handle) : "unable to open";
    if (handle != nullptr) {
      sqlite3_close(handle);
    }
    throw std::runtime_error("unable to open test database: " + message);
  }

  std::ifstream schema_file(PURRBOSS_SCHEMA_SQL_PATH);
  if (!schema_file.is_open()) {
    sqlite3_close(handle);
    throw std::runtime_error("unable to open schema file: " +
                             std::string{PURRBOSS_SCHEMA_SQL_PATH});
  }
  std::stringstream buffer;
  buffer << schema_file.rdbuf();

  char *error = nullptr;
  const int result =
      sqlite3_exec(handle, buffer.str().c_str(), nullptr, nullptr, &error);
  if (result != SQLITE_OK) {
    const std::string message =
        error != nullptr ? error : sqlite3_errmsg(handle);
    sqlite3_free(error);
    sqlite3_close(handle);
    throw std::runtime_error("unable to apply schema: " + message);
  }
  sqlite3_close(handle);
}

inline void RemoveDatabaseFiles(const std::string &database_path) {
  std::filesystem::remove(database_path);
  std::filesystem::remove(database_path + "-wal");
  std::filesystem::remove(database_path + "-shm");
}

/**
 * @brief Fixture base that provisions a fresh, schema-migrated SQLite file
 * per test and exposes an initialized storage::Database over it.
 */
class SqliteTestDatabase : public ::testing::Test {
protected:
  void SetUp() override {
    database_path_ = UniqueTempDatabasePath();
    RemoveDatabaseFiles(database_path_);
    ApplySchema(database_path_);
    database_ = std::make_unique<storage::Database>(database_path_);
    database_->Initialize();
  }

  void TearDown() override {
    database_.reset();
    RemoveDatabaseFiles(database_path_);
  }

  std::string database_path_;
  std::unique_ptr<storage::Database> database_;
};

} // namespace purrboss::testing
