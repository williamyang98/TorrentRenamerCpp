cmake_minimum_required(VERSION 3.10)
project(TorrentRenamer)


find_package(cpr CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

set(SRC_FILES 
    src/tvdb_api.cpp
    src/se_regex.cpp
    src/file_loading.cpp
    src/scanner.cpp
    src/renamer.cpp
)

add_executable(cli_test src/main_cli_test.cpp ${SRC_FILES})
include_directories(cli_test src/)

set(GUI_FILES
    src/app.cpp
    src/gui_ext.cpp)

add_executable(main src/main_gui.cpp ${GUI_FILES} ${SRC_FILES})
include_directories(main src/)

set_target_properties(cli_test PROPERTIES CXX_STANDARD 17)
target_link_libraries(cli_test PRIVATE cpr::cpr rapidjson fmt::fmt)

set_target_properties(main PROPERTIES CXX_STANDARD 17)
target_link_libraries(main PRIVATE cpr::cpr rapidjson fmt::fmt imgui::imgui opengl32)