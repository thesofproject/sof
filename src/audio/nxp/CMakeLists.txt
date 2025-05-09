# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_NXP_EAP)
	zephyr_library_sources(eap.c)
	if(CONFIG_COMP_NXP_EAP_STUB)
		zephyr_library_sources(eap_stub.c)
	else()
		zephyr_library_import(EAPLibrary
			${sof_top_dir}/eap_sdk/EAP_Library/libEAP16_3_0_13_FP1_RT600.a)
	endif()
endif()
