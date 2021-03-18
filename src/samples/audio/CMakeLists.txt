# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_SAMPLE_SMART_AMP)
	add_local_sources(sof
		smart_amp_test.c
	)
endif()

if(CONFIG_SAMPLE_KEYPHRASE)
	add_local_sources(sof
		detect_test.c
	)
endif()

if(CONFIG_KWD_NN_SAMPLE_KEYPHRASE)
	add_local_sources(sof kwd_nn_detect_test.c)
endif()
