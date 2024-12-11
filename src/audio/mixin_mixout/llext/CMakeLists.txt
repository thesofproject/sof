# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("mixin_mixout"
	SOURCES ../mixin_mixout.c
		../mixin_mixout_hifi3.c
		../mixin_mixout_hifi5.c
		../mixin_mixout_generic.c
	LIB openmodules
)
