cmake_minimum_required(VERSION 3.22)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# With MSVC, Qt comes from Visual Studio's vcpkg. This has to be set up
# before project(), which runs vcpkg install.
include(VcpkgSetup)

project(gbemu)

include(DeployQt)

# On Windows, gbemu builds with MSVC (Qt from vcpkg) or with GCC from MSYS2's
# UCRT64 environment (MSYS2's Qt).
if(WIN32 AND NOT MSVC AND NOT (MINGW AND "$ENV{MSYSTEM}" STREQUAL "UCRT64"))
    message(FATAL_ERROR "On Windows, build gbemu with MSVC or from the MSYS2 UCRT64 shell. See BUILD_WINDOWS.md.")
endif()

# Compiler settings

# C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Release by default where the build folder holds one build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    # Read the sources as UTF-8 and follow the standard strictly. Release and
    # Debug keep CMake's defaults (/O2 and /Od).
    add_compile_options(/utf-8 /permissive-)
else()
    # Release builds use -O2, Debug builds -Og (debuggable, but fast enough to play)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -Og")
endif()

# Dependencies

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# Targets

# The emulator core, which doesn't use Qt
file(GLOB_RECURSE CORE_FILES CONFIGURE_DEPENDS Source/Core/*.cpp)
add_library(gbemu_core STATIC ${CORE_FILES})
target_include_directories(gbemu_core PUBLIC Source)

# The Qt application. WIN32 makes it a Windows GUI program, so it doesn't open
# a console window; other platforms ignore it.
file(GLOB FRONTEND_FILES CONFIGURE_DEPENDS Source/Frontend/*.cpp)
add_executable(gbemu WIN32 Source/Main.cpp ${FRONTEND_FILES})
target_link_libraries(gbemu PRIVATE gbemu_core Qt6::Widgets Qt6::Multimedia)
gbemu_deploy_qt(gbemu)

# A Visual Studio solution starts gbemu when debugging, instead of ALL_BUILD
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT gbemu)
