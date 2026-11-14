cmake_minimum_required(VERSION 3.23)

# vcpkg (Windows MSVC)
# Uses the vcpkg bundled with Visual Studio, but keeps everything it fetches
# or builds inside this project instead of %LOCALAPPDATA%:
#   - port recipes:   .vcpkg/registries/
#   - source/tools:   .vcpkg/downloads/
#   - binary cache:   .vcpkg/archives/   (new build dirs reuse built packages)
#   - installed libs: <build>/vcpkg_installed/
if(CMAKE_HOST_WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_vcpkg_root "$ENV{VCPKG_ROOT}") # set by the Developer Command Prompt
    if(NOT _vcpkg_root)
        set(_pf86 "ProgramFiles(x86)")
        execute_process(
            COMMAND "$ENV{${_pf86}}/Microsoft Visual Studio/Installer/vswhere.exe"
                    -latest -products * -property installationPath
            OUTPUT_VARIABLE _vs_path OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(_vcpkg_root "${_vs_path}/VC/vcpkg")
    endif()
    if(NOT EXISTS "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
        message(FATAL_ERROR "Visual Studio's vcpkg was not found. Install the "
                            "\"vcpkg package manager\" component in the VS Installer.")
    endif()

    set(_local "${CMAKE_CURRENT_SOURCE_DIR}/.vcpkg")
    file(MAKE_DIRECTORY "${_local}/registries" "${_local}/downloads" "${_local}/archives")
    set(ENV{X_VCPKG_REGISTRIES_CACHE} "${_local}/registries")
    set(ENV{VCPKG_DOWNLOADS} "${_local}/downloads")
    set(ENV{VCPKG_BINARY_SOURCES} "clear;files,${_local}/archives,readwrite")
    set(ENV{VCPKG_DISABLE_METRICS} 1)
    set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
    # Build dependencies (and vcpkg's host tools) in Release only.
    set(VCPKG_TARGET_TRIPLET "x64-windows-release" CACHE STRING "")
    set(VCPKG_HOST_TRIPLET "x64-windows-release" CACHE STRING "")
endif()

# Release-only builds: the dependencies have no Debug libraries to link against.
set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

# Project Name
project(gbemu)

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

# Copy the Qt DLLs and plugins next to gbemu.exe so it runs from the build dir.
if(WIN32 AND TARGET Qt6::windeployqt)
    add_custom_command(TARGET gbemu POST_BUILD
        COMMAND Qt6::windeployqt --no-translations --no-compiler-runtime
                "$<TARGET_FILE:gbemu>"
        COMMENT "Deploying Qt runtime next to gbemu.exe")
endif()
