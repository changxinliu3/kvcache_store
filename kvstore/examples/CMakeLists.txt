cmake_minimum_required(VERSION 3.20)

add_executable(kv_put_get kv_put_get.cpp)
target_link_libraries(kv_put_get PRIVATE kvstore)

add_executable(kv_async kv_async.cpp)
target_link_libraries(kv_async PRIVATE kvstore)
