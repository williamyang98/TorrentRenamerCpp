cmake_minimum_required(VERSION 3.16)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/vendor/rapidjson)
add_library(rapidjson INTERFACE IMPORTED)
target_include_directories(rapidjson INTERFACE ${SRC_DIR}/include)