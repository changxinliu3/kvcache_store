cmake_minimum_required(VERSION 3.20)

function(kvstore_add_test name src)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE kvstore)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

kvstore_add_test(test_hash_index unit/test_hash_index.cpp)
kvstore_add_test(test_allocator unit/test_allocator.cpp)
kvstore_add_test(test_record unit/test_record.cpp)
kvstore_add_test(test_request unit/test_request.cpp)
kvstore_add_test(test_kvstore integration/test_kvstore.cpp)
kvstore_add_test(test_io integration/test_io.cpp)

add_executable(test_nixl_md nixl/test_nixl_backend.cpp)
target_include_directories(test_nixl_md PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_nixl_md PRIVATE kvstore)
add_test(NAME test_nixl_md COMMAND test_nixl_md)
