/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Zhigang.wu <zhigang.wu@linux.intel.com>
 */

#ifndef __INCLUDE_SPI__
#define __INCLUDE_SPI__

#include <stdint.h>

enum spi_type {
	SPI_TYPE_INTEL_RECEIVE  = 0,
	SPI_TYPE_INTEL_TRANSMIT,
};

enum {
	SPI_TRIGGER_START = 0,
	SPI_TRIGGER_STOP,
};

enum sof_spi_type {
	SOF_SPI_INTEL_SLAVE = 0,
	SOF_SPI_INTEL_MASTER,
};

/* CTRLR0 */
/* 00-standard spi; 01-dual spi; 10-quad spi */
#define FRAME_FORMAT(x)		((x) << 21)
#define DATA_FRAME_SIZE(x)	((x) << 16)
/* 0-slave txt enabled; 1-slave txt disabled */
#define SLV_OE(x)		((x) << 10)
/* 00-both; 01-transmit only;
 * 10-receive only; 11-eeprom read
 */
#define TRANSFER_MODE(x)	((x) << 8)
/* 0-inactive low; 1-inactive high */
#define SCPOL(x)		((x) << 7)
/* 0-first edge capture; 1-one cycle after cs line */
#define SCPH(x)		((x) << 6)
/* 00-moto spi; 01-ti ssp; 10-ns microwire */
#define FRAME_TYPE(x)	((x) << 4)

/* SSIENR */
#define SSIEN		1

/* IMR */
/* 0-masked; 1-unmasked;
 * receive FIFO full interrupt mask/unmask
 */
#define RXFIM(x)	((x) << 4)
/* 0-masked; 1-unmasked;
 * receive FIFO overflow interrupt mask/unmask
 */
#define RXOIM(x)	((x) << 3)
/* 0-masked; 1-unmasked;
 * transmit FIFO overflow interrupt mask/unmask
 */
#define TXOIM(x)	((x) << 1)
/* 0-masked; 1-unmasked;
 * transmit FIFO empty interrupt mask/unmask
 */
#define TXEIM(x)	((x) << 0)

/* DMACR */
/* 0-transmit DMA disable; 1-transmit DMA enable */
#define	TDMAE(x)	((x) << 1)
/* 0-receive DMA disable; 1-receive DMA enable */
#define RDMAE(x)	((x) << 0)
/* DMATDLR/DMARDLR */
/* transmit data level: 0~255 */
#define TDLR(x)		((x) << 0)
/* receive data level: 0~255 */
#define RDLR(x)		((x) << 0)

/* spi register list */
#define	CTRLR0		0x00
#define	CTRLR1		0x04
#define	SSIENR		0x08
#define	MWCR		0x0C
#define	SER		0x10
#define	BAUDR		0x14
#define	TXFTLR		0x18
#define	RXFTLR		0x1C
#define	TXFLR		0x20
#define	RXFLR		0x24
#define	SR		0x28
#define	IMR		0x2C
#define	ISR		0x30
#define	RISR		0x34
#define	TXOICR		0x38
#define	RXOICR		0x3C
#define	RXUICR		0x40
#define	ICR		0x48
#define	DMACR		0x4C
#define	DMATDLR		0x50
#define	DMARDLR		0x54
#define	DR		0x60
#define	SPICTRLR0	0xF4

/* io global control reg */
#define SPICLK_CTL	0x60

extern const struct spi_ops spi_ops;

#define spi_irq(spi) \
		spi->plat_data.irq
#define spi_fifo_offset(spi, direction) \
		spi->plat_data.fifo[direction].offset
#define spi_fifo_handshake(spi, direction) \
		spi->plat_data.fifo[direction].handshake

#define spi_set_drvdata(spi, data) \
		do { \
			spi->private = data; \
			spi->private_size = sizeof(*data); \
		} while (0)
#define spi_get_drvdata(spi) \
		spi->private
#define spi_base(spi) \
		spi->plat_data.base
#define spi_irq(spi) \
		spi->plat_data.irq

struct spi;

struct sof_spi_config {
	uint16_t format;	/* frame format */
	uint16_t size;		/* frame size */
	uint16_t tmode;		/* transmit mode */
	uint16_t type;		/* frame type */
	uint16_t mode;		/* clock mode */
	uint16_t rfifo_thd;	/* rx fifo threshold */
	uint16_t tfifo_thd;	/* tx fifo threshold */
	uint16_t rdat_level;	/* receive data level */
	uint16_t tdat_level;	/* transmit data level */
};

struct spi_plat_fifo_data {
	uint32_t offset;
	uint32_t width;
	uint32_t depth;
	uint32_t watermark;
	uint32_t handshake;
};

struct spi_plat_data {
	uint32_t base;
	uint32_t irq;
	uint32_t flags;
	struct spi_plat_fifo_data fifo[2];
};

struct spi_pdata {
	uint32_t state;
};

struct spi_ops {
	int (*set_config)(struct spi *spi, struct sof_spi_config *config);
	int (*read)(struct spi *spi, void *buf, uint32_t len);
	int (*write)(struct spi *spi, void *buf, uint32_t len);
	int (*probe)(struct spi *spi);
};

struct spi {
	uint32_t type;
	uint32_t index;
	struct spi_plat_data plat_data;
	const struct spi_ops *ops;
	void *private;
	uint32_t private_size;
};

struct spi *spi_get(uint32_t type);

static inline void spi_reg_write(struct spi *spi, uint32_t reg, uint32_t value)
{
	io_reg_write(spi_base(spi) + reg, value);
}

static inline uint32_t spi_reg_read(struct spi *spi, uint32_t reg)
{
	return io_reg_read(spi_base(spi) + reg);
}

static inline void spi_reg_update_bits(struct spi *spi, uint32_t reg,
				       uint32_t mask, uint32_t value)
{
	io_reg_update_bits(spi_base(spi) + reg, mask, value);
}

#endif
