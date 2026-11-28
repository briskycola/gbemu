# Copies the Qt plugins a program needs next to it after each build, so it
# runs from the build folder.

include_guard(GLOBAL)

# gbemu_deploy_qt(<target>)
#
# MSVC: vcpkg copies the DLLs <target> links next to it, but not Qt's plugins
# (the Windows platform plugin, styles, multimedia). windeployqt copies those.
# vcpkg's Qt has a windeployqt.debug.bat that deploys the Debug plugins for
# Debug builds. Elsewhere this does nothing: on Linux, programs find Qt's
# plugins where Qt is installed, and MSYS2 builds run from the UCRT64 shell.
function(gbemu_deploy_qt target)
    if(NOT MSVC OR NOT TARGET Qt6::windeployqt)
        return()
    endif()

    get_target_property(windeployqt Qt6::windeployqt IMPORTED_LOCATION_RELEASE)
    if(NOT windeployqt)
        get_target_property(windeployqt Qt6::windeployqt IMPORTED_LOCATION)
    endif()
    get_filename_component(windeployqt_folder "${windeployqt}" DIRECTORY)
    set(windeployqt_debug "${windeployqt_folder}/windeployqt.debug.bat")
    if(NOT EXISTS "${windeployqt_debug}")
        set(windeployqt_debug "${windeployqt}")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "$<IF:$<CONFIG:Debug>,${windeployqt_debug},${windeployqt}>"
            --no-translations --no-compiler-runtime --no-system-d3d-compiler --no-opengl-sw
            "$<TARGET_FILE:${target}>"
        COMMENT "Copying the Qt plugins ${target} needs next to it"
        VERBATIM)
endfunction()
