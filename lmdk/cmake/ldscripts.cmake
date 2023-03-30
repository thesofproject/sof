# Linker scripts generator

# These linker scripts are based on those found in https://github.com/thesofproject/converged-sof-modules
#
# There are few things these scripts ensure:
#
# (1) Each module has its own reserved (virtual) address space. This is specified as
# HPSRAM_ADDR in module's CMakeLists.txt and goes to memory_header_linker_script.txt.
#
# (2) .buildinfo section must be put at 0 offset of .text section. .buildinfo contains
# module loadable libraries API version. That API version is verified by base FW.
# Base FW accesses it simply as 0 offset from module's code.
#
# (3) .module section contains module manifest description. This section is used by
# rimage to generate module manifest. Scripts ensure that this section is not garbage
# collected by linker.

# Required parameters MODULE and HPSRAM_ADDR should be specified in command line.

if(NOT DEFINED MODULE)
  message(FATAL_ERROR "MODULE not defined")
endif()

if(NOT DEFINED HPSRAM_ADDR)
  message(FATAL_ERROR "HPSRAM_ADDR not defined")
endif()

# reserve space for manifest?
math(EXPR HPSRAM "${HPSRAM_ADDR} + 9 * 4096" OUTPUT_FORMAT HEXADECIMAL)

set(LDSCRIPTS_DIR ${MODULE}_ldscripts)
set(LDSCRIPT_FILE ${LDSCRIPTS_DIR}/elf32xtensa.x)

file(MAKE_DIRECTORY ${LDSCRIPTS_DIR})
file(WRITE ${LDSCRIPT_FILE} "")

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/${LDSCRIPTS_DIR}/memory_header_linker_script.txt\n")
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/ldscripts/memory_header_linker_script.txt.in
  ${CMAKE_CURRENT_BINARY_DIR}/${LDSCRIPTS_DIR}/memory_header_linker_script.txt
)

file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/text_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/common_text_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/data_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/common_rodata_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/bss_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/xt_linker_script.txt\n")
file(APPEND ${LDSCRIPT_FILE} "INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ldscripts/guard_linker_script.txt\n")
