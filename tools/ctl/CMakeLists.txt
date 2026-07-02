# SPDX-License-Identifier: BSD-3-Clause

add_executable(sof-ctl
	ctl.c
)

target_link_directories(sof-ctl BEFORE
	PRIVATE "${SOF_ROOT_SOURCE_DIRECTORY}/../tools/lib")

target_link_libraries(sof-ctl PRIVATE
	"-lasound"
)

# tools/lib is only needed as a -L link path for a locally built ALSA.
# Do not let CMake also embed it as an ELF RPATH/RUNPATH: it points into
# the build tree and, once stripped to empty on install, leaves an empty
# DT_RUNPATH tag that trips distro ELF security scanners (scanelf reports
# "NULL DT_RUNPATH"). Skip the build RPATH and keep the install RPATH
# empty so no DT_RUNPATH tag is emitted. See thesofproject/sof#10070.
set_target_properties(sof-ctl PROPERTIES
	SKIP_BUILD_RPATH TRUE
	INSTALL_RPATH "")

target_compile_options(sof-ctl PRIVATE
	-Wall -Werror
)

target_include_directories(sof-ctl PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/../tools/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}"
)

install(TARGETS sof-ctl DESTINATION bin)
