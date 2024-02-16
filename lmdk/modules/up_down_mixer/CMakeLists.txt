set(LMDK_BUILD 0)
set(SOF_SRC_DIR ../../../src)
set(LMDK_DIR_INCLUDE ../../../lmdk/include)
target_sources(up_down_mixer PRIVATE ${SOF_SRC_DIR}/audio/up_down_mixer/up_down_mixer.c ${SOF_SRC_DIR}/audio/up_down_mixer/up_down_mixer_hifi3.c)

set_target_properties(up_down_mixer PROPERTIES
  HPSRAM_ADDR "0xa06b1000"
)

target_compile_definitions(up_down_mixer PRIVATE CONFIG_XTENSA=1
												 CONFIG_IPC_MAJOR_4=1
												 CONFIG_LIBRARY=1
												 MODULE_PRIVAT=1
												 XCHAL_HAVE_HIFI3=1
												 SOF_MODULE_API_PRIVATE=1)

if(LMDK_BUILD)
target_include_directories(up_down_mixer PRIVATE "${LMDK_DIR_INCLUDE}"
												 "${LMDK_DIR_INCLUDE}/sof"
												 "${LMDK_DIR_INCLUDE}/sof/audio"
												 "${LMDK_DIR_INCLUDE}/module/audio")
else()
target_include_directories(up_down_mixer PRIVATE "${SOF_SRC_DIR}/include"
                                                 "${SOF_SRC_DIR}/include/ipc"
                                                 "${SOF_SRC_DIR}/include/ipc4"
                                                 "${SOF_SRC_DIR}/include/sof/audio/module_adapter/iadk"
												 "${SOF_SRC_DIR}/../posix/include"
												 "${SOF_SRC_DIR}/../posix/include/sof"
)
endif()