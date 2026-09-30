# NIXL Backend Adapter (V1 skeleton)
#
# IMPORTANT (per coding spec §45):
# Do NOT guess NIXL ABI from this repository alone.
# Before enabling KVSTORE_BUILD_NIXL=ON:
#   1. Point NIXL_ROOT at a real NIXL checkout
#   2. Confirm nixlBackendEngine method signatures from that tree
#   3. Adapt headers/sources under this directory to match
#
# KVStore Core must remain free of NIXL headers.

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED NIXL_ROOT)
  message(FATAL_ERROR "KVSTORE_BUILD_NIXL=ON requires -DNIXL_ROOT=/path/to/nixl")
endif()

add_library(nixl_kvstore_backend SHARED
  kvstore_backend.cpp
  kvstore_md.cpp
  kvstore_req.cpp
  kvstore_plugin.cpp
)

target_include_directories(nixl_kvstore_backend
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NIXL_ROOT}/src/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(nixl_kvstore_backend PRIVATE kvstore)

# Link against the NIXL libraries discovered in NIXL_ROOT as needed.
