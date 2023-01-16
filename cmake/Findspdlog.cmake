cmake_minimum_required(VERSION 3.16)

option(SPDLOG_FMT_EXTERNAL "" ON)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/spdlog)