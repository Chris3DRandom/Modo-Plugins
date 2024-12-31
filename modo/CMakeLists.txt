cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

include("cmake/utils.cmake")

# Define the project.
project(modo_plugins)
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

add_compile_definitions("$<$<NOT:$<CONFIG:Debug>>:NDEBUG>")

add_subdirectory("src")