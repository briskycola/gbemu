# Gets Qt from Visual Studio's vcpkg when building on Windows outside MSYS2.
#
# Include this before project(): project() loads vcpkg's toolchain, which runs
# vcpkg install for the packages in vcpkg.json, so everything vcpkg needs is
# set up first. Elsewhere (Linux, MSYS2), or when a toolchain file is already
# given, this does nothing beyond what that toolchain asks for.

include_guard(GLOBAL)

# Sets <out_toolchain> to the toolchain file of Visual Studio's vcpkg.
# VCPKG_ROOT is set in Visual Studio and its Developer shells; elsewhere,
# vswhere finds the newest Visual Studio and its bundled vcpkg.
function(gbemu_find_visual_studio_vcpkg out_toolchain)
    set(vcpkg_root "$ENV{VCPKG_ROOT}")
    if(NOT vcpkg_root)
        set(program_files_x86 "ProgramFiles(x86)")
        set(vswhere "$ENV{${program_files_x86}}/Microsoft Visual Studio/Installer/vswhere.exe")
        if(EXISTS "${vswhere}")
            execute_process(
                COMMAND "${vswhere}" -latest -prerelease -products * -property installationPath
                OUTPUT_VARIABLE visual_studio_folder
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            set(vcpkg_root "${visual_studio_folder}/VC/vcpkg")
        endif()
    endif()

    set(toolchain "${vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
    if(NOT EXISTS "${toolchain}")
        message(FATAL_ERROR "Visual Studio's vcpkg wasn't found. Install Visual Studio 2026 with "
            "the Desktop development with C++ workload, or build from the MSYS2 UCRT64 shell. "
            "See BUILD_WINDOWS.md.")
    endif()
    set(${out_toolchain} "${toolchain}" PARENT_SCOPE)
endfunction()

# Keeps everything vcpkg downloads, builds and caches in <vcpkg_folder>
# instead of in the user's profile. vcpkg refuses a registries cache folder
# that doesn't exist, so the folders are made first.
function(gbemu_vcpkg_use_project_folder vcpkg_folder)
    foreach(folder downloads archives registries)
        file(MAKE_DIRECTORY "${vcpkg_folder}/${folder}")
    endforeach()
    set(ENV{VCPKG_DOWNLOADS} "${vcpkg_folder}/downloads")
    set(ENV{VCPKG_BINARY_SOURCES} "clear;files,${vcpkg_folder}/archives,readwrite")
    set(ENV{X_VCPKG_REGISTRIES_CACHE} "${vcpkg_folder}/registries")
    set(VCPKG_INSTALL_OPTIONS
        "--x-buildtrees-root=${vcpkg_folder}/buildtrees"
        "--x-packages-root=${vcpkg_folder}/packages"
        CACHE STRING "")
endfunction()

# Makes vcpkg use the CMake, Ninja, Git and PowerShell 7 already installed
# instead of downloading its own. They go first on the PATH that vcpkg install
# sees, so it also works outside a Developer shell. CMake is the one running
# now; Ninja comes with Visual Studio's CMake, next to <vcpkg_root>.
function(gbemu_vcpkg_use_installed_tools vcpkg_root)
    set(visual_studio_cmake "${vcpkg_root}/../../Common7/IDE/CommonExtensions/Microsoft/CMake")
    find_program(GBEMU_NINJA ninja HINTS "${visual_studio_cmake}/Ninja")
    find_program(GBEMU_GIT git)
    find_program(GBEMU_PWSH pwsh HINTS "$ENV{ProgramFiles}/PowerShell/7")

    if(NOT GBEMU_PWSH)
        message(FATAL_ERROR "vcpkg needs PowerShell 7 (pwsh), which isn't installed. Install it with "
            "\"winget install --id Microsoft.PowerShell --source winget --installer-type wix\" "
            "and open a new terminal. See BUILD_WINDOWS.md.")
    endif()
    if(NOT GBEMU_GIT)
        message(FATAL_ERROR "vcpkg needs Git, which isn't installed. Install it with "
            "\"winget install --id Git.Git --source winget\" and open a new terminal. See BUILD_WINDOWS.md.")
    endif()
    if(NOT GBEMU_NINJA)
        message(FATAL_ERROR "vcpkg needs Ninja, which comes with Visual Studio's C++ CMake tools. "
            "See BUILD_WINDOWS.md.")
    endif()

    set(tool_folders)
    foreach(tool "${CMAKE_COMMAND}" "${GBEMU_NINJA}" "${GBEMU_GIT}" "${GBEMU_PWSH}")
        get_filename_component(folder "${tool}" DIRECTORY)
        file(TO_NATIVE_PATH "${folder}" folder)
        list(APPEND tool_folders "${folder}")
    endforeach()
    list(JOIN tool_folders ";" tool_path)
    set(ENV{PATH} "${tool_path};$ENV{PATH}")
    set(ENV{VCPKG_FORCE_SYSTEM_BINARIES} 1)
endfunction()

# Chooses the vcpkg triplet, and so the Qt that vcpkg builds, from the build
# type, and sets <out_triplet> to it:
# - None given (Visual Studio generator): the build folder can build any
#   configuration, chosen with cmake --build --config, so vcpkg builds a Debug
#   and a Release Qt (x64-windows).
# - -DCMAKE_BUILD_TYPE=Release (or RelWithDebInfo, MinSizeRel): the folder
#   builds only that configuration, and vcpkg only a Release Qt
#   (x64-windows-release).
# - -DCMAKE_BUILD_TYPE=Debug: the folder builds only Debug. vcpkg can't build
#   a Qt only for Debug, so it builds both (x64-windows).
# The host triplet matches, so vcpkg builds Qt's tools with the same Qt.
function(gbemu_vcpkg_choose_triplet out_triplet)
    set(build_type "${CMAKE_BUILD_TYPE}")
    if(CMAKE_GENERATOR MATCHES "^Visual Studio|Multi-Config$")
        if(build_type)
            set(CMAKE_CONFIGURATION_TYPES "${build_type}" CACHE STRING "" FORCE)
        endif()
    elseif(NOT build_type)
        set(build_type Release)
    endif()

    if(NOT build_type OR build_type STREQUAL "Debug")
        set(triplet x64-windows)
    else()
        set(triplet x64-windows-release)
    endif()

    # A build folder keeps the Qt it was first configured with
    if(DEFINED CACHE{VCPKG_TARGET_TRIPLET} AND NOT "$CACHE{VCPKG_TARGET_TRIPLET}" STREQUAL "${triplet}")
        message(FATAL_ERROR "This build folder was configured with vcpkg's "
            "$CACHE{VCPKG_TARGET_TRIPLET} Qt, but this build type needs ${triplet}. "
            "Add --fresh to the cmake command to start the folder over, or use another folder.")
    endif()
    set(VCPKG_TARGET_TRIPLET "${triplet}" CACHE STRING "")
    set(VCPKG_HOST_TRIPLET "${triplet}" CACHE STRING "")
    set(${out_triplet} "${triplet}" PARENT_SCOPE)
endfunction()

# vcpkg install makes its installed folder hold only what this configure asks
# for, so each triplet gets its own folder in <vcpkg_folder>. Otherwise
# configuring a Release-only build folder would uninstall the Qt the others use.
function(gbemu_vcpkg_use_installed_folder vcpkg_folder triplet)
    if(triplet)
        set(VCPKG_INSTALLED_DIR "${vcpkg_folder}/installed-${triplet}" CACHE PATH "")
    else()
        set(VCPKG_INSTALLED_DIR "${vcpkg_folder}/installed" CACHE PATH "")
    endif()
endfunction()


if(CMAKE_HOST_WIN32 AND NOT DEFINED ENV{MSYSTEM} AND NOT CMAKE_TOOLCHAIN_FILE)
    gbemu_find_visual_studio_vcpkg(CMAKE_TOOLCHAIN_FILE)
endif()
if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
    return()
endif()

get_filename_component(GBEMU_VCPKG_ROOT "${CMAKE_TOOLCHAIN_FILE}/../../.." ABSOLUTE)
set(GBEMU_VCPKG_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/.vcpkg")
set(GBEMU_VCPKG_TRIPLET "")

gbemu_vcpkg_use_project_folder("${GBEMU_VCPKG_FOLDER}")
gbemu_vcpkg_use_installed_tools("${GBEMU_VCPKG_ROOT}")
if(CMAKE_HOST_WIN32)
    gbemu_vcpkg_choose_triplet(GBEMU_VCPKG_TRIPLET)
endif()
gbemu_vcpkg_use_installed_folder("${GBEMU_VCPKG_FOLDER}" "${GBEMU_VCPKG_TRIPLET}")
