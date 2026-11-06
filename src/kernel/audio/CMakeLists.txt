# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_BASEFW_IPC4 AND NOT CONFIG_LIBRARY)
	add_local_sources(sof base_fw.c)
endif()
add_local_sources_ifdef(CONFIG_IPC4_BASE_FW_INTEL sof base_fw_intel.c)
if(CONFIG_INTEL_ADSP_MIC_PRIVACY)
	add_subdirectory(mic_privacy_manager)
endif()
