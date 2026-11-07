/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef PLATFORM_POSIX_LIB_DMA_H
#define PLATFORM_POSIX_LIB_DMA_H

#define FIXME_DMA_IRQ 999
#define dma_chan_irq(dma, chan) FIXME_DMA_IRQ
#define dma_chan_irq_name(dma, chan) "FIXME_DMA_NAME"

#define PLATFORM_NUM_DMACS 6
#define PLATFORM_MAX_DMA_CHAN 9

#endif /* PLATFORM_POSIX_LIB_DMA_H */
