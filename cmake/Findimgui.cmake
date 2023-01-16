cmake_minimum_required(VERSION 3.16)
project(imgui)

# find_package(glfw REQUIRED)
# find_package(OpenGL REQUIRED)

if (WIN32)
set(IMGUI_EXTERNAL_LIBS opengl32.lib)
else()
set(IMGUI_EXTERNAL_LIBS GL)
endif(WIN32)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
add_library(imgui STATIC
    ${SRC_DIR}/imgui.h
    ${SRC_DIR}/imgui_internal.h
    ${SRC_DIR}/imgui.cpp
    ${SRC_DIR}/imgui_demo.cpp
    ${SRC_DIR}/imgui_draw.cpp
    ${SRC_DIR}/imgui_widgets.cpp
    ${SRC_DIR}/imgui_tables.cpp
    ${SRC_DIR}/misc/cpp/imgui_stdlib.h
    ${SRC_DIR}/misc/cpp/imgui_stdlib.cpp
    ${SRC_DIR}/backends/imgui_impl_opengl3.h
    ${SRC_DIR}/backends/imgui_impl_opengl3.cpp
    ${SRC_DIR}/backends/imgui_impl_glfw.h
    ${SRC_DIR}/backends/imgui_impl_glfw.cpp
    # ${SRC_DIR}/backends/imgui_impl_dx9.h
    # ${SRC_DIR}/backends/imgui_impl_dx9.cpp
    # ${SRC_DIR}/backends/imgui_impl_dx11.h
    # ${SRC_DIR}/backends/imgui_impl_dx11.cpp
    # ${SRC_DIR}/backends/imgui_impl_dx12.h
    # ${SRC_DIR}/backends/imgui_impl_dx12.cpp
    # ${SRC_DIR}/backends/imgui_impl_win32.h
    # ${SRC_DIR}/backends/imgui_impl_win32.cpp
    # ${SRC_DIR}/backends/imgui_impl_vulkan.h
    # ${SRC_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC "${SRC_DIR}")
target_include_directories(imgui PUBLIC "${SRC_DIR}/backends")
target_include_directories(imgui PUBLIC "${SRC_DIR}/misc/cpp")
target_link_libraries(imgui 
    PUBLIC glfw
    PRIVATE ${IMGUI_EXTERNAL_LIBS})

