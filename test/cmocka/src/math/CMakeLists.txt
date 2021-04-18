# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(numbers)
add_subdirectory(trig)

# FFT needs maths is WIP for xtensa GCC
if(XCC AND NOT BUILD_UNIT_TESTS_HOST)
	add_subdirectory(fft)
endif()
