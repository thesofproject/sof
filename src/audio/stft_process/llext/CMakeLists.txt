# Copyright (c) 2025 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("stft_process_comp"
	SOURCES	../stft_process.c
		../stft_process_setup.c
		../stft_process_common.c
		../stft_process-generic.c
		../stft_process-ipc4.c
	LIB openmodules
)
