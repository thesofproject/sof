# SPDX-License-Identifier: BSD-3-Clause

if (CONFIG_DUMMY_DMA)
	add_local_sources(sof dummy-dma.c)
endif()

add_local_sources(sof
	buf-copier-dma.c
	echo-ref.c)
