cmake_minimum_required(VERSION 3.20)
project(simple-web-proxy)

set(CMAKE_BUILD_TYPE "Debug")

add_executable(simple-web-proxy src/main.c src/pool.c)
target_link_libraries(simple-web-proxy uv)
if(WIN32)
    target_link_libraries(simple-web-proxy ws2_32)
endif()

add_executable(test-pool src/pool_test.c src/pool.c)
