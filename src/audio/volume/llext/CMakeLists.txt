# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("volume"
	SOURCES ../volume_generic.c
		../volume_hifi3.c
		../volume_hifi4.c
		../volume_generic_with_peakvol.c
		../volume_hifi3_with_peakvol.c
		../volume_hifi4_with_peakvol.c
		../volume.c
		../volume_ipc4.c
)
