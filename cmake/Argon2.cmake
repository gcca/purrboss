
find_package(Threads REQUIRED)

set(ARGON2_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/argon2")

add_library(argon2 STATIC
  "${ARGON2_VENDOR_DIR}/src/argon2.c"
  "${ARGON2_VENDOR_DIR}/src/core.c"
  "${ARGON2_VENDOR_DIR}/src/blake2/blake2b.c"
  "${ARGON2_VENDOR_DIR}/src/thread.c"
  "${ARGON2_VENDOR_DIR}/src/encoding.c"
  "${ARGON2_VENDOR_DIR}/src/ref.c"
)

add_library(argon2::argon2 ALIAS argon2)

target_include_directories(argon2
  PUBLIC "${ARGON2_VENDOR_DIR}/include"
  PRIVATE "${ARGON2_VENDOR_DIR}/src"
)

target_link_libraries(argon2 PUBLIC Threads::Threads)

set_target_properties(argon2 PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS NO
  POSITION_INDEPENDENT_CODE ON
)
