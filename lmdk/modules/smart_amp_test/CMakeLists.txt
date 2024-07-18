target_sources(smart_amp_test PRIVATE ${SOF_BASE}/src/samples/audio/smart_amp_test_ipc4.c)

set_target_properties(smart_amp_test PROPERTIES
	HPSRAM_ADDR "0xa06c1000"
)

target_compile_definitions(smart_amp_test PRIVATE
	__SOF_MODULE_SERVICE_BUILD__=1
	CONFIG_XTENSA=1
	CONFIG_IPC_MAJOR_4=1
)

target_include_directories(smart_amp_test PRIVATE
    "${SOF_BASE}/src/include"
    "${SOF_BASE}/posix/include"
)
