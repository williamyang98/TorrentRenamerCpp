cmake_minimum_required(VERSION 3.10)
project(TorrentRenamer)

list(APPEND VCPKG_FEATURE_FLAGS "versions")

find_package(cpr CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(imgui)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /fsanitize=address")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /fsanitize=address")

set(SRC_FILES 
    src/tvdb_api/tvdb_api.cpp
    src/tvdb_api/tvdb_api_schema.cpp
    src/tvdb_api/tvdb_json.cpp
    src/renaming/se_regex.cpp
    src/renaming/managed_folder.cpp
    src/renaming/scanner.cpp
    # we need template definition in header file for linker to work
    # src/renaming/renamer.cpp
    src/app/app.cpp
    src/app/app_folder.cpp
    src/app/app_config.cpp
    src/app/app_credentials_schema.cpp
    src/util/file_loading.cpp
)

set(GUI_FILES
    src/gui/gui_ext.cpp)

# test a pure cli application
add_executable(cli_test src/main_cli_test.cpp ${SRC_FILES})
include_directories(cli_test src/)
set_target_properties(cli_test PROPERTIES CXX_STANDARD 20)
target_link_libraries(cli_test PRIVATE 
    cpr::cpr rapidjson fmt::fmt spdlog::spdlog spdlog::spdlog_header_only)
target_compile_options(cli_test PRIVATE "/MP")

# imgui implementation of gui application
add_executable(main src/main_gui.cpp ${GUI_FILES} ${SRC_FILES})
include_directories(main src/)
set_target_properties(main PROPERTIES CXX_STANDARD 20)
target_link_libraries(main PRIVATE 
    cpr::cpr rapidjson fmt::fmt spdlog::spdlog spdlog::spdlog_header_only)
target_link_libraries(main PUBLIC imgui)
target_compile_options(main PRIVATE "/MP")
