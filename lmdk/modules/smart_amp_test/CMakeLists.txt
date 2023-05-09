target_sources(smart_amp_test PRIVATE smart_amp_test.c)

set_target_properties(smart_amp_test PROPERTIES
  HPSRAM_ADDR "0xa06c1000"
)

target_compile_definitions(smart_amp_test PRIVATE __ZEPHYR__=1
												 CONFIG_XTENSA=1
												 CONFIG_IPC_MAJOR_4=1)

target_include_directories(smart_amp_test PRIVATE "../../include"
												 "../../include/ipc"
												 "../../include/ipc4"
 												 "../../include/math"
 												 "../../include/lib")