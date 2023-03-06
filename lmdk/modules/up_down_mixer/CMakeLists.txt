target_sources(up_down_mixer PRIVATE up_down_mixer.c up_down_mixer_hifi3.c)

set_target_properties(up_down_mixer PROPERTIES
  HPSRAM_ADDR "0xa06b1000"
)

target_compile_definitions(up_down_mixer PRIVATE __ZEPHYR__=1
												 CONFIG_XTENSA=1
												 CONFIG_IPC_MAJOR_4=1)

target_include_directories(up_down_mixer PRIVATE "../../include"
												 "../../include/coefficients"
												 "../../include/ipc"
												 "../../include/ipc4"
 												 "../../include/math"
 												 "../../include/lib")