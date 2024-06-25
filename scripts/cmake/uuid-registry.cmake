#
# UUID registry generation
#

# Simple target.  FOUR (really 4.5, as LIBRARY builds use the same
# CMakeLists.txt but differ significantly in how it executes)
# different cmake environments into which it needs to build.
is_zephyr(zephyr_is)
if(zephyr_is)
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

add_custom_command(
	OUTPUT ${UUID_REG_H}
	COMMAND
	${PYTHON_EXECUTABLE} ${TOPDIR}/scripts/gen-uuid-reg.py
	                     ${TOPDIR}/uuid-registry.txt
			     ${UUID_REG_H})

add_custom_target(uuid_reg_h DEPENDS ${UUID_REG_H})

add_dependencies(${DEP_TARGET} uuid_reg_h)
