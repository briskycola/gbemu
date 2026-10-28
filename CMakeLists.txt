cmake_minimum_required(VERSION 3.23)

# Project Name
project(gbemu)

# Use C++ 17 (needed for std::filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- SDL2 ----
find_package(SDL2 CONFIG REQUIRED)

# ---- Dear ImGui (git submodule) ----
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(FATAL_ERROR "Dear ImGui was not found. Run: git submodule update --init --recursive")
endif()

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2)

# Collect source files
file(GLOB SRC_FILES src/*.cpp)

# Create executable
add_executable(gbemu ${SRC_FILES})
target_link_libraries(gbemu PRIVATE imgui SDL2::SDL2)
