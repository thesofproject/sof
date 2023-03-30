
if(NOT DEFINED RIMAGE_COMMAND)
  message(FATAL_ERROR
    " Please define RIMAGE_COMMAND: path to rimage executable.\n"
    " E.g. using cmake -DRIMAGE_COMMAND=/path/rimage command line parameter."
  )
endif()

if(NOT DEFINED SIGNING_KEY)
  message(FATAL_ERROR
    " Please define SIGNING_KEY: path to signing key for rimage.\n"
    " E.g. using cmake -DSIGNING_KEY=/path/to/key.pem command line parameter."
  )
endif()

# This Loadable Modules Dev Kit root dir
set(LMDK_BASE ${CMAKE_CURRENT_LIST_DIR}/..)
cmake_path(ABSOLUTE_PATH LMDK_BASE NORMALIZE)

# thesofproject root dir
set(SOF_BASE ${LMDK_BASE}/..)
cmake_path(ABSOLUTE_PATH SOF_BASE NORMALIZE)

set(RIMAGE_INCLUDE_DIR ${SOF_BASE}/rimage/src/include)
cmake_path(ABSOLUTE_PATH RIMAGE_INCLUDE_DIR NORMALIZE)
