if (CONFIG_SOF_BOOT_TEST)
	zephyr_library_sources_ifdef(CONFIG_VIRTUAL_HEAP
		vmh.c
	)
	zephyr_library_sources_ifdef(CONFIG_USERSPACE
		userspace/ksem.c
	)
endif()
