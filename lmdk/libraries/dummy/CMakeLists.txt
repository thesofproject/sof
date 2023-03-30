
cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../../cmake/xtensa-toolchain.cmake")

project(dummy)

# list of modules to be built and included into this loadable library
set(MODULES_LIST dummy)

# toml file for rimage to generate manifets
set(TOML "${CMAKE_CURRENT_LIST_DIR}/dummy_mtl.toml")

# TODO: Move it somewhere?! This probably should be defined in some API header file!
# SOF loadable modules API version
add_definitions(-DMAJOR_IADSP_API_VERSION=5)
add_definitions(-DMIDDLE_IADSP_API_VERSION=0)
add_definitions(-DMINOR_IADSP_API_VERSION=0)

include(../../cmake/build.cmake)
