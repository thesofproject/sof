
target_sources(dummy PRIVATE dummy.c)

set_target_properties(dummy PROPERTIES
	HPSRAM_ADDR "0xa06a1000"
)

target_compile_definitions(dummy PRIVATE CONFIG_XTENSA=1
					 CONFIG_IPC_MAJOR_4=1
					 CONFIG_LIBRARY=1
					 XCHAL_HAVE_HIFI3=1
					 SOF_MODULE_API_PRIVATE=1)

set(LMDK_DIR_INCLUDE ../../../lmdk/include)

target_include_directories(dummy PRIVATE "${LMDK_DIR_INCLUDE}"
                                                 "${LMDK_DIR_INCLUDE}/src/include"
                                                 "${LMDK_DIR_INCLUDE}/src/include/sof/audio/module_adapter/iadk"
                                                 "${LMDK_DIR_INCLUDE}/posix/include"
                                                 "${LMDK_DIR_INCLUDE}/posix/include/sof"
)
