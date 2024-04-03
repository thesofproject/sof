# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

# Hard-coded .text address to be moved to a common place
sof_llext_build("mixin_mixout"
	SOURCES ../mixin_mixout.c
		../mixin_mixout_hifi3.c
		../mixin_mixout_hifi5.c
		../mixin_mixout_generic.c
	TEXT_ADDR 0xa06aa000
)
