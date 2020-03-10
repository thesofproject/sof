# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(lib)


if(CONFIG_CAVS_LPS)
add_library(lps_vector STATIC "")
target_link_libraries(lps_vector sof_options)
target_compile_options(lps_vector PRIVATE -mtext-section-literals)

add_local_sources(lps_vector lps_pic_restore_vector.S)

target_link_libraries(sof_static_libraries INTERFACE lps_vector)

add_local_sources(sof lps_wait.c)
endif()

if(CONFIG_NO_SLAVE_CORE_ROM)
    add_library(altreset STATIC "")
    target_link_libraries(altreset sof_options)
    target_compile_options(altreset PRIVATE -mtext-section-literals)

    add_local_sources(altreset alternate_reset_vector.S)

    target_link_libraries(sof_static_libraries INTERFACE altreset)
endif()

add_local_sources(sof platform.c)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/include)
