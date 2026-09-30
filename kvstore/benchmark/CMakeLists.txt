cmake_minimum_required(VERSION 3.20)

add_executable(bench_kvstore bench_kvstore.cpp)
target_link_libraries(bench_kvstore PRIVATE kvstore)

add_executable(bench_io bench_io.cpp)
target_link_libraries(bench_io PRIVATE kvstore)
