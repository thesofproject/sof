# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("phase_vocoder"
	SOURCES	../phase_vocoder.c
		../phase_vocoder_setup.c
		../phase_vocoder_common.c
		../phase_vocoder-generic.c
		../phase_vocoder-ipc4.c
	LIB openmodules
)
