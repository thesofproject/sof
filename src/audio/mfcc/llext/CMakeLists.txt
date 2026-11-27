# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

set(MFCC_LLEXT_SOURCES
	../mfcc.c
	../mfcc_setup.c
	../mfcc_common.c
	../mfcc_generic.c
	../mfcc_hifi3.c
	../mfcc_hifi4.c
	../mfcc_vad.c
)
if(CONFIG_IPC_MAJOR_4)
	list(APPEND MFCC_LLEXT_SOURCES ../mfcc_ipc4.c)
endif()

sof_llext_build("mfcc"
	SOURCES ${MFCC_LLEXT_SOURCES}
	LIB openmodules
)
