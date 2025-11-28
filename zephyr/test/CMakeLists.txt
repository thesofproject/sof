if (CONFIG_SOF_BOOT_TEST)
       zephyr_library_sources_ifdef(CONFIG_VIRTUAL_HEAP
               vmh.c
       )
endif()

if (CONFIG_SOF_BOOT_TEST_STANDALONE)
       if (CONFIG_DT_HAS_INTEL_ADSP_HDA_HOST_IN_ENABLED AND CONFIG_SOF_USERSPACE_INTERFACE_DMA)
              zephyr_library_sources(userspace/test_intel_hda_dma.c)
       endif()
endif()
