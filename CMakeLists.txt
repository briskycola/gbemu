cmake_minimum_required(VERSION 3.23)

# Project Name
project(gbemu)

# Use C++ 17 (needed for std::filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Qt6 desktop frontend ----
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia OpenGL OpenGLWidgets)

# Collect source files
file(GLOB SRC_FILES CONFIGURE_DEPENDS src/*.cpp)

# Create executable
add_executable(gbemu ${SRC_FILES})
target_link_libraries(gbemu PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::OpenGL Qt6::OpenGLWidgets)
