cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_list)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../..")

target_include_directories(app PRIVATE ${SOF_ROOT}/src/include)

target_sources(app PRIVATE test_list_ztest.c)
