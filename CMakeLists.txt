cmake_minimum_required(VERSION 3.23)

# Project Name
project(gbemu)

# On Windows, only MSYS2's UCRT64 environment (GCC and MSYS2's Qt) is supported.
if(WIN32 AND NOT (MINGW AND "$ENV{MSYSTEM}" STREQUAL "UCRT64"))
    message(FATAL_ERROR "On Windows, build gbemu from the MSYS2 UCRT64 shell. See README.md.")
endif()

# Use C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt6 desktop frontend
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# Collect source files
file(GLOB SRC_FILES CONFIGURE_DEPENDS src/*.cpp)

# Create executable
add_executable(gbemu ${SRC_FILES})
target_link_libraries(gbemu PRIVATE Qt6::Widgets Qt6::Multimedia)
