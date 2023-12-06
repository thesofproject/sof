if (CONFIG_ACE_VERSION_1_5)
       zephyr_library_sources_ifdef(CONFIG_SOF_BOOT_TEST
               vmh.c
       )
endif()
