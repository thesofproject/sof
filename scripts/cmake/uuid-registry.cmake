#
# UUID registry generation
#

# Simple target.  FOUR (really 4.5, as LIBRARY builds use the same
# CMakeLists.txt but differ significantly in how it executes)
# different cmake environments into which it needs to build.
is_zephyr(zephyr)
if(NOT DEFINED PYTHON_EXECUTABLE)
  if(DEFINED Python3_EXECUTABLE)
    set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
  else()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
  endif()
endif()
if(zephyr)
  set(TOPDIR ${sof_top_dir})
  set(UUID_REG_H ${PROJECT_BINARY_DIR}/include/generated/uuid-registry.h)
  set(DEP_TARGET zephyr_interface)
elseif(${PROJECT_NAME} STREQUAL "SOF_TOOLS")
  set(TOPDIR "${PROJECT_SOURCE_DIR}/..")
  set(UUID_REG_H "${CMAKE_CURRENT_BINARY_DIR}/uuid-registry.h")
  set(DEP_TARGET sof-logger)
elseif(${PROJECT_NAME} STREQUAL "SOF_TPLG_PARSER")
  set(TOPDIR "${PROJECT_SOURCE_DIR}/../..")
  set(UUID_REG_H "${PROJECT_BINARY_DIR}/include/uuid-registry.h")
  set(DEP_TARGET sof_tplg_parser)
else()
  # Legacy SOF, or CONFIG_LIBRARY
  set(TOPDIR ${PROJECT_SOURCE_DIR})
  set(UUID_REG_H ${PROJECT_BINARY_DIR}/generated/include/uuid-registry.h)
  set(DEP_TARGET sof_public_headers)
endif()

message(STATUS "UUID_REG_H is ${UUID_REG_H}, TOPDIR is ${TOPDIR}, PYTHON is ${PYTHON_EXECUTABLE}")

execute_process(
	COMMAND
	${PYTHON_EXECUTABLE} ${TOPDIR}/scripts/gen-uuid-reg.py
	                     ${TOPDIR}/uuid-registry.txt
			     ${UUID_REG_H}
	RESULT_VARIABLE uuid_res
	ERROR_VARIABLE uuid_err
)
message(STATUS "gen-uuid-reg.py result: ${uuid_res}, error: ${uuid_err}")


add_custom_command(
	OUTPUT ${UUID_REG_H}
	COMMAND
	${PYTHON_EXECUTABLE} ${TOPDIR}/scripts/gen-uuid-reg.py
	                     ${TOPDIR}/uuid-registry.txt
			     ${UUID_REG_H}
	DEPENDS ${TOPDIR}/uuid-registry.txt
)

add_custom_target(uuid_reg_h ALL DEPENDS ${UUID_REG_H})

if(TARGET zephyr_generated_headers)
  add_dependencies(zephyr_generated_headers uuid_reg_h)
endif()

add_dependencies(${DEP_TARGET} uuid_reg_h)

