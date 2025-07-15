if (CONFIG_ACE_VERSION_1_5 OR CONFIG_ACE_VERSION_2_0 OR CONFIG_ACE_VERSION_3_0)
	zephyr_library_sources_ifdef(CONFIG_SOF_BOOT_TEST
		vmh.c
	)
endif()
