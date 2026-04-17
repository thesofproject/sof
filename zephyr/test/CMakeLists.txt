if(CONFIG_SOF_BOOT_TEST)
  zephyr_library_sources_ifdef(CONFIG_VIRTUAL_HEAP
    vmh.c
  )
  zephyr_library_sources_ifdef(CONFIG_SOF_VREGIONS
    vpage.c vregion.c
  )
  zephyr_library_sources_ifdef(CONFIG_USERSPACE
    userspace/ksem.c
  )
  if(CONFIG_USERSPACE AND CONFIG_SOF_USERSPACE_INTERFACE_ALLOC)
    zephyr_library_sources(userspace/test_heap_alloc.c)
  endif()
endif()

if(CONFIG_SOF_BOOT_TEST_STANDALONE AND CONFIG_SOF_USERSPACE_INTERFACE_DMA)
  if(CONFIG_DT_HAS_INTEL_ADSP_HDA_HOST_IN_ENABLED)
    zephyr_library_sources(userspace/test_intel_hda_dma.c)
  endif()
  if(CONFIG_DT_HAS_INTEL_ADSP_HDA_SSP_CAP_ENABLED)
    zephyr_library_sources(userspace/test_intel_ssp_dai.c)
  endif()
endif()

if(CONFIG_SOF_BOOT_TEST_STANDALONE AND CONFIG_SOF_USERSPACE_LL)
  zephyr_library_sources(userspace/test_mailbox.c)
endif()

if(CONFIG_SOF_BOOT_TEST_STANDALONE AND CONFIG_SOF_USERSPACE_LL)
  zephyr_library_sources(userspace/test_ipc4_pipeline_util.c
  userspace/test_ipc4_pipeline_native.c
  userspace/test_ipc4_pipeline_user.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_SRC audio/test_src.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_ASRC audio/test_asrc.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_TONE audio/test_tone.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_MIXIN_MIXOUT audio/test_mixin_mixout.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_UP_DOWN_MIXER audio/test_up_down_mixer.c)
  # Codec requires specific Kconfig depending on adapter type, omitted selectively or defaulted to adapter:
  zephyr_library_sources_ifdef(CONFIG_COMP_CODEC_ADAPTER audio/test_codec.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_ARIA audio/test_aria.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_MFCC audio/test_mfcc.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_CROSSOVER audio/test_crossover.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_DCBLOCK audio/test_dcblock.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_MULTIBAND_DRC audio/test_multiband_drc.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_LEVEL_MULTIPLIER audio/test_level_multiplier.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_RTNR audio/test_rtnr.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_SMART_AMP audio/test_smart_amp.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_TDFB audio/test_tdfb.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_STFT_PROCESS audio/test_stft_process.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_GOOGLE_HOTWORD_DETECT audio/test_google.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_IGO_NR audio/test_igo_nr.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_NXP_EAP audio/test_nxp.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_TENSORFLOW audio/test_tensorflow.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_MIC_PRIVACY_MANAGER audio/test_mic_privacy_manager.c)
  zephyr_library_sources_ifdef(CONFIG_COMP_SOUND_DOSE audio/test_sound_dose.c)
endif()

if(CONFIG_SOF_BOOT_TEST_STANDALONE AND CONFIG_ZTEST)
  set(MATH_ZTEST_SOURCES
    ../../test/ztest/unit/math/basic/arithmetic/test_crc32_ztest.c
    ../../test/ztest/unit/math/basic/arithmetic/test_find_equal_int16_ztest.c
    ../../test/ztest/unit/math/basic/arithmetic/test_find_max_abs_int32_ztest.c
    ../../test/ztest/unit/math/basic/arithmetic/test_find_min_int16_ztest.c
    ../../test/ztest/unit/math/basic/arithmetic/test_gcd_ztest.c
    ../../test/ztest/unit/math/basic/arithmetic/test_norm_int32_ztest.c
  )

  zephyr_library_sources(${MATH_ZTEST_SOURCES})

  set_source_files_properties(${MATH_ZTEST_SOURCES}
    PROPERTIES COMPILE_DEFINITIONS "CONFIG_NUMBERS_VECTOR_FIND=1;CONFIG_NUMBERS_NORM=1;UNIT_TEST=1"
  )
endif()
