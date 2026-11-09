# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("mfcc"
	SOURCES ../mfcc.c
		../mfcc_setup.c
		../mfcc_common.c
		../mfcc_generic.c
		../mfcc_hifi3.c
		../mfcc_hifi4.c
		../mfcc_vad.c
		../mfcc_agc8.c
		../mfcc_ipc4.c
	LIB openmodules
)
