function(add_grpc_proto_library target proto_dir)
  file(GLOB proto_files CONFIGURE_DEPENDS "${proto_dir}/*.proto")
  if(NOT proto_files)
    message(FATAL_ERROR "No .proto files found in ${proto_dir}")
  endif()

  find_path(protobuf_well_known_include
    NAMES google/protobuf/timestamp.proto
    REQUIRED
  )

  set(generated_sources)
  foreach(proto_file IN LISTS proto_files)
    get_filename_component(proto_name "${proto_file}" NAME_WE)

    set(pb_header "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.h")
    set(pb_source "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.cc")
    set(grpc_header "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.h")
    set(grpc_source "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.cc")

    add_custom_command(
      OUTPUT ${pb_header} ${pb_source} ${grpc_header} ${grpc_source}
      COMMAND $<TARGET_FILE:protobuf::protoc>
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        -I ${proto_dir}
        -I ${protobuf_well_known_include}
        ${proto_file}
      DEPENDS ${proto_file} protobuf::protoc gRPC::grpc_cpp_plugin
      COMMENT "Generating C++ sources for ${proto_name}.proto"
      VERBATIM
    )

    list(APPEND generated_sources ${pb_source} ${grpc_source})
  endforeach()

  add_library(${target} STATIC ${generated_sources})
  target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(${target} PUBLIC protobuf::libprotobuf gRPC::grpc++)
endfunction()
