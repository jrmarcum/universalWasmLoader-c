cmake_minimum_required(VERSION 3.15)
project(uwl_consumer C)

set(CMAKE_C_STANDARD 11)

find_package(universal-wasm-loader-c CONFIG REQUIRED)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE unofficial::universal-wasm-loader-c)
