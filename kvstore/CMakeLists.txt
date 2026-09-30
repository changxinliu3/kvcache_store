cmake_minimum_required(VERSION 3.20)
project(kvstore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KVSTORE_BUILD_TESTS "Build unit and integration tests" ON)
option(KVSTORE_BUILD_EXAMPLES "Build examples" ON)
option(KVSTORE_BUILD_BENCHMARK "Build benchmarks" ON)
option(KVSTORE_BUILD_NIXL "Build NIXL backend adapter (requires NIXL)" OFF)
option(KVSTORE_ENABLE_IO_URING "Enable io_uring engine (Linux)" ON)

add_library(kvstore
  src/core/kvstore.cpp
  src/core/request.cpp
  src/core/transaction.cpp
  src/index/hash_index.cpp
  src/allocator/block_allocator.cpp
  src/io/io_engine.cpp
  src/io/file_io_engine.cpp
  src/metadata/record.cpp
  src/metadata/journal.cpp
)

target_include_directories(kvstore
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(UNIX AND NOT APPLE AND KVSTORE_ENABLE_IO_URING)
  target_sources(kvstore PRIVATE src/io/io_uring_engine.cpp)
  target_compile_definitions(kvstore PUBLIC KVSTORE_HAS_IO_URING=1)
  find_library(LIBURING_LIB uring)
  if(LIBURING_LIB)
    target_link_libraries(kvstore PUBLIC ${LIBURING_LIB})
  else()
    message(WARNING "liburing not found; IoUringEngine will fail at link/runtime")
  endif()
else()
  target_compile_definitions(kvstore PUBLIC KVSTORE_HAS_IO_URING=0)
endif()

if(UNIX)
  target_link_libraries(kvstore PUBLIC pthread)
endif()

if(KVSTORE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(KVSTORE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(KVSTORE_BUILD_BENCHMARK)
  add_subdirectory(benchmark)
endif()

if(KVSTORE_BUILD_NIXL)
  add_subdirectory(nixl)
endif()
