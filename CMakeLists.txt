# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)

project(SOF_RIMAGE C)

add_executable(rimage
	src/file_simple.c
	src/man_apl.c
	src/man_cnl.c
	src/man_kbl.c
	src/man_sue.c
	src/man_tgl.c
	src/cse.c
	src/css.c
	src/plat_auth.c
	src/hash.c
	src/pkcs1_5.c
	src/manifest.c
	src/elf.c
	src/rimage.c
)

target_compile_options(rimage PRIVATE
	-O2 -g -Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3
)

target_link_libraries(rimage PRIVATE "-lcrypto")

target_include_directories(rimage PRIVATE
	src/include/rimage
)

if(DEFINED SOF_SOURCE_DIR)
	# Use headers from SOF_SOURCE_DIR
	target_include_directories(rimage PRIVATE
		"${SOF_SOURCE_DIR}/src/include"
	)
else()
	# Use headers provided here
	target_include_directories(rimage PRIVATE
		src/include/sof
	)
endif()

install(TARGETS rimage DESTINATION bin)
