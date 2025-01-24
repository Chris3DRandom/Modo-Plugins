cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

include("cmake/utils.cmake")

# Define the project.
project(modo_plugins)
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
add_compile_definitions("$<$<NOT:$<CONFIG:Debug>>:NDEBUG>")

# Don't build the thickness checker by default.
set(BUILD_THICKNESS_CHECKER 0)

# Optionally set the target output dir for kits
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    file(TO_CMAKE_PATH "$ENV{APPDATA}" USER_APP_DATA)
    set(CMAKE_INSTALL_PREFIX "${USER_APP_DATA}/Luxology/Kits")
endif()

add_subdirectory("src")