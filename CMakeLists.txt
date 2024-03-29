cmake_minimum_required(VERSION 3.10)
project(TorrentRenamer)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(glfw REQUIRED)
find_package(imgui REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(rapidjson REQUIRED)
find_package(cpr REQUIRED)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(TVDB_API_DIR ${SRC_DIR}/tvdb_api)
add_library(tvdb_api STATIC 
    ${TVDB_API_DIR}/tvdb_api.cpp
    ${TVDB_API_DIR}/tvdb_api_schema.cpp
    ${TVDB_API_DIR}/tvdb_json.cpp
)
target_compile_features(tvdb_api PRIVATE cxx_std_17)
target_include_directories(tvdb_api PUBLIC ${TVDB_API_DIR} ${SRC_DIR})
target_link_libraries(tvdb_api 
    PUBLIC rapidjson fmt::fmt spdlog::spdlog 
    PRIVATE cpr::cpr)

set(APP_DIR ${SRC_DIR})
add_library(app_lib STATIC
    ${SRC_DIR}/app/app.cpp
    ${SRC_DIR}/app/app_config.cpp
    ${SRC_DIR}/app/app_credentials.cpp
    ${SRC_DIR}/app/app_schemas.cpp
    ${SRC_DIR}/app/app_folder.cpp
    ${SRC_DIR}/app/app_folder_bookmarks_json.cpp
    ${SRC_DIR}/app/app_folder_state.cpp
    ${SRC_DIR}/app/app_file_state.cpp
    ${SRC_DIR}/app/file_descriptor.cpp
    ${SRC_DIR}/app/file_intents.cpp
    ${SRC_DIR}/util/file_loading.cpp
    ${SRC_DIR}/os_dep.cpp
)
target_compile_features(app_lib PRIVATE cxx_std_17)
target_include_directories(app_lib PUBLIC ${APP_DIR} ${SRC_DIR})
target_link_libraries(app_lib PUBLIC 
    tvdb_api rapidjson fmt::fmt spdlog::spdlog)

# test a pure cli application
add_executable(cli_test ${SRC_DIR}/main_cli_test.cpp)
target_include_directories(cli_test PRIVATE ${SRC_DIR})
target_compile_features(cli_test PRIVATE cxx_std_17)
target_link_libraries(cli_test app_lib)

# imgui implementation of gui application
add_executable(main 
    ${SRC_DIR}/main_gui.cpp
    ${SRC_DIR}/gui/imgui_skeleton.cpp
    ${SRC_DIR}/gui/render_app.cpp
)
target_include_directories(main PRIVATE ${SRC_DIR})
target_compile_features(main PRIVATE cxx_std_17)
target_link_libraries(main app_lib imgui)

if(MSVC)
    add_compile_options(/MP)
endif(MSVC)
