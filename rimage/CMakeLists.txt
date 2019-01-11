cmake_minimum_required(VERSION 3.10)

project(SOF_RIMAGE C)

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/..")

if(NOT DEFINED VERSION_H_PATH)
	message(FATAL_ERROR
	" Please specify version.h path.\n" 
	" Example: cmake -DVERSION_H_PATH=${VERSION_H_PATH} ...\n"
)
endif()

if(NOT DEFINED PEM_KEY_PREFIX)
	set(PEM_KEY_PREFIX "/usr/local/share/rimage")
endif()

get_filename_component(VERSION_H_DIRECTORY ${VERSION_H_PATH} DIRECTORY)

add_executable(rimage
	file_simple.c
	man_apl.c
	man_cnl.c
	man_kbl.c
	man_sue.c
	cse.c
	css.c
	plat_auth.c
	hash.c
	pkcs1_5.c
	manifest.c
	elf.c
	rimage.c
)

target_compile_options(rimage PRIVATE 
	-O2 -g -Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3
)

target_compile_definitions(rimage PRIVATE
	PEM_KEY_PREFIX="${PEM_KEY_PREFIX}"
)

target_link_libraries(rimage PRIVATE "-lcrypto")

target_include_directories(rimage PRIVATE 
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${VERSION_H_DIRECTORY}"
)
