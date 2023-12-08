cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../../cmake/xtensa-toolchain.cmake")

project(smart_amp_test)

macro(is_zephyr ret)
        if(CONFIG_ZEPHYR_SOF_MODULE)
                set(${ret} TRUE)
        else()
                set(${ret} FALSE)
        endif()
endmacro()

# list of modules to be built and included into this loadable library
set(MODULES_LIST smart_amp_test)

# toml file for rimage to generate manifets
set(TOML "${CMAKE_CURRENT_LIST_DIR}/smart_amp_test.toml")

# TODO: Move it somewhere???
add_definitions(-DMAJOR_IADSP_API_VERSION=5)
add_definitions(-DMIDDLE_IADSP_API_VERSION=0)
add_definitions(-DMINOR_IADSP_API_VERSION=0)

include(../../cmake/build.cmake)
