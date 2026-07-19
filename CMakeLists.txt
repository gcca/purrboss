cmake_minimum_required(VERSION 4.1)

if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER clang)
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER clang++)
endif()

project(purrboss LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

include(cmake/ProtoGen.cmake)
include(cmake/Argon2.cmake)

add_grpc_proto_library(purrboss_proto "${CMAKE_CURRENT_SOURCE_DIR}/protos")

add_executable(purrboss
  src/purrboss.cc
  src/purrboss/services/session/create_session.cpp
  src/purrboss/services/session/validate_session.cpp
  src/purrboss/services/session/invalidate_session.cpp
  src/purrboss/services/session/ping.cpp
  src/purrboss/services/session/tools.cpp
  src/purrboss/services/auth/authenticate.cpp
  src/purrboss/services/auth/user_info.cpp
  src/purrboss/storage/db.cpp
)

target_include_directories(purrboss SYSTEM PRIVATE 3rdparty)
target_include_directories(purrboss PRIVATE src)
target_link_libraries(purrboss PRIVATE argon2::argon2 gRPC::grpc++ purrboss_proto SQLite3::SQLite3)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(purrboss PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(PURRBOSS_TESTS "Build and enable tests" OFF)

if(PURRBOSS_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()

  add_executable(purrboss-test
    src/purrboss/storage/db.cpp
    src/purrboss/storage/db-test.cc
    src/purrboss/services/session/tools.cpp
    src/purrboss/services/session/tools-test.cc
    src/purrboss/services/session/create_session.cpp
    src/purrboss/services/session/validate_session.cpp
    src/purrboss/services/session/invalidate_session.cpp
    src/purrboss/services/session/ping.cpp
    src/purrboss/services/session/session-test.cc
    src/purrboss/services/auth/authenticate.cpp
    src/purrboss/services/auth/user_info.cpp
    src/purrboss/services/auth/auth-test.cc
  )
  target_compile_features(purrboss-test PRIVATE cxx_std_23)
  target_include_directories(purrboss-test SYSTEM PRIVATE 3rdparty)
  target_include_directories(purrboss-test PRIVATE src)
  target_compile_definitions(purrboss-test PRIVATE
    PURRBOSS_SCHEMA_SQL_PATH="${CMAKE_CURRENT_SOURCE_DIR}/db/schema.sql")
  target_link_libraries(purrboss-test PRIVATE GTest::gtest_main argon2::argon2 gRPC::grpc++ purrboss_proto SQLite3::SQLite3)
  add_test(NAME purrboss-test COMMAND purrboss-test)
endif()
