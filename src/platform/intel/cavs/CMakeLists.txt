add_local_sources(sof
	platform.c
	dai.c
	dma.c
	memory.c
	pm_runtime.c
)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/include)