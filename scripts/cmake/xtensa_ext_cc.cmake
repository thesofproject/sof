# SPDX-License-Identifier: BSD-3-Clause
#
# Resolve compiler target flags and archiver for external pure-C static
# libraries built via shell scripts under standalone LLEXT targets.
#
# Usage:
#   include("${sof_top_dir}/scripts/cmake/xtensa_ext_cc.cmake")
#   xtensa_ext_cc_get_flags(_ext_arch_cflags _ext_ar)
#
# When building Xtensa with Clang/LLVM, the generic Clang driver defaults to
# the host target unless passed explicit --target, -mcpu, --sysroot, and
# -fintegrated-as flags matching Zephyr's toolchain configuration.

function(xtensa_ext_cc_get_flags out_cflags out_ar)
  if(DEFINED CMAKE_AR)
    set(_ar "${CMAKE_AR}")
  else()
    set(_ar "ar")
  endif()

  if("${ARCH}" STREQUAL "xtensa")
    if(CONFIG_LLEXT_BUILD_PIC)
      set(_pic_flags "-fPIC")
    else()
      set(_pic_flags "-fno-pic -fno-pie")
    endif()

    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
      set(_sdk_prefix "")
      get_filename_component(_ar_name "${_ar}" NAME)
      if(_ar_name MATCHES "^xtensa-.+_zephyr-elf-ar$")
        string(REGEX REPLACE "ar$" "" _sdk_prefix "${_ar}")
      elseif(CMAKE_EXE_LINKER_FLAGS MATCHES "--ld-path=([^ ]+xtensa-[^ /]+_zephyr-elf)-ld")
        set(_sdk_prefix "${CMAKE_MATCH_1}-")
        set(_ar "${_sdk_prefix}ar")
      endif()

      set(_core "${XTENSA_CORE_ID}")
      if(NOT _core)
        set(_core "$ENV{XTENSA_CORE_ID}")
      endif()
      if(NOT _core AND _sdk_prefix)
        get_filename_component(_triple_dash "${_sdk_prefix}" NAME)
        string(REGEX REPLACE "^xtensa-" "" _sdk_core "${_triple_dash}")
        string(REGEX REPLACE "_zephyr-elf-$" "" _sdk_core "${_sdk_core}")
        if(_sdk_core STREQUAL "intel_ace30_ptl")
          set(_core "intel_ace30_adsp")
        elseif(_sdk_core STREQUAL "intel_ace40")
          set(_core "intel_ace40_adsp")
        elseif(_sdk_core STREQUAL "intel_ace15_mtpm")
          set(_core "intel_ace15_adsp")
        else()
          set(_core "${_sdk_core}")
        endif()
      endif()

      set(_cflags "--target=xtensa -fintegrated-as ${_pic_flags}")
      if(_core)
        set(_cflags "${_cflags} -mcpu=${_core}")
      endif()
      if(_sdk_prefix)
        get_filename_component(_tc_bin "${_sdk_prefix}" DIRECTORY)
        get_filename_component(_gnu_root "${_tc_bin}" DIRECTORY)
        get_filename_component(_triple "${_sdk_prefix}" NAME)
        string(REGEX REPLACE "-$" "" _triple "${_triple}")
        set(_sysroot "${_gnu_root}/${_triple}")
        if(IS_DIRECTORY "${_sysroot}")
          set(_cflags "${_cflags} --sysroot=${_sysroot}")
        endif()
      endif()
    else()
      set(_cflags "${_pic_flags} -mlongcalls -mtext-section-literals")
    endif()
  else()
    set(_cflags "-fPIC")
  endif()

  set(${out_cflags} "${_cflags}" PARENT_SCOPE)
  set(${out_ar} "${_ar}" PARENT_SCOPE)
endfunction()
