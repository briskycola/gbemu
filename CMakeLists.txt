cmake_minimum_required(VERSION 3.22)

# Project Name
project(gbemu)

# On Windows, only MSYS2's UCRT64 environment (GCC and MSYS2's Qt) is supported.
if(WIN32 AND NOT (MINGW AND "$ENV{MSYSTEM}" STREQUAL "UCRT64"))
    message(FATAL_ERROR "On Windows, build gbemu from the MSYS2 UCRT64 shell. See README.md.")
endif()

# Release builds use -O2, Debug builds -Og (debuggable, but fast enough to play)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Og")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Use C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt6 desktop frontend
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# The emulator core, which doesn't use Qt
file(GLOB_RECURSE CORE_FILES CONFIGURE_DEPENDS Source/Core/*.cpp)
add_library(gbemu_core STATIC ${CORE_FILES})
target_include_directories(gbemu_core PUBLIC Source)

# The Qt application. WIN32 makes it a Windows GUI program, so it doesn't
# open a console window; other platforms ignore it.
file(GLOB FRONTEND_FILES CONFIGURE_DEPENDS Source/Frontend/*.cpp)
add_executable(gbemu WIN32 Source/Main.cpp ${FRONTEND_FILES})
target_link_libraries(gbemu PRIVATE gbemu_core Qt6::Widgets Qt6::Multimedia)
