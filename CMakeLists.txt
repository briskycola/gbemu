cmake_minimum_required(VERSION 3.23)

# Project Name
project(gbemu)

# Use C++ 14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSION OFF)

# Collect source files
file(GLOB SRC_FILES src/*.cpp)

# Create executable
add_executable(gbemu ${SRC_FILES})

# ---- SDL3 ----
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(gbemu PRIVATE SDL2::SDL2)
