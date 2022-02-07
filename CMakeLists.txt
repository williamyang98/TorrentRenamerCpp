cmake_minimum_required(VERSION 3.10)
project(TorrentRenamer)

list(APPEND VCPKG_FEATURE_FLAGS "versions")

find_package(cpr CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/vendor)
set(imgui_docking_DIR ${VENDOR_DIR}/imgui_docking)
find_package(imgui_docking CONFIG REQUIRED)


set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /fsanitize=address")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /fsanitize=address")

set(SRC_FILES 
    src/app_credentials_schema.cpp
    src/tvdb_api.cpp
    src/tvdb_api_schema.cpp
    src/se_regex.cpp
    src/file_loading.cpp
    src/folder_diff.cpp
    src/scanner.cpp
    # we need template definition in header file for linker to work
    # src/renamer.cpp 
) 

add_executable(cli_test src/main_cli_test.cpp ${SRC_FILES})
include_directories(cli_test src/)

set(GUI_FILES
    src/app.cpp
    src/app_folder.cpp
    src/app_config.cpp
    src/gui_ext.cpp)

add_executable(main src/main_gui.cpp ${GUI_FILES} ${SRC_FILES})
include_directories(main src/)

set_target_properties(cli_test PROPERTIES CXX_STANDARD 20)
target_link_libraries(cli_test PRIVATE cpr::cpr rapidjson fmt::fmt spdlog::spdlog spdlog::spdlog_header_only)
target_compile_options(cli_test PRIVATE "/MP")

set_target_properties(main PROPERTIES CXX_STANDARD 20)
target_link_libraries(main PRIVATE 
    cpr::cpr rapidjson fmt::fmt spdlog::spdlog spdlog::spdlog_header_only 
    imgui_docking "d3d11.lib" "dxgi.lib" "d3dcompiler.lib")
target_compile_options(main PRIVATE "/MP")