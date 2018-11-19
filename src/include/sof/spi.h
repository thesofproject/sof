/*
 * Copyright (c) 2017, Intel Corporation
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
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#ifndef __INCLUDE_SPI_H__
#define __INCLUDE_SPI_H__

enum IPC_STATE {
	SUE_IPC_IDLE		= 0,
	SUE_IPC_WRITE_DATA,	/* receive the data from HOST */
	SUE_IPC_READ_DATA,	/* transmit the data to HOST */
	SUE_IPC_ERROR,
};

#define SUE_SPI_READ		1
#define SUE_SPI_WRITE		0

#define SUE_SPI_BASEADDRESS(x)	(x + 0x00080000)
#define	SUE_SPI_REG_CTRLR0		SUE_SPI_BASEADDRESS(0x00)
#define	SUE_SPI_REG_CTRLR1		SUE_SPI_BASEADDRESS(0x04)
#define	SUE_SPI_REG_SSIENR		SUE_SPI_BASEADDRESS(0x08)
#define	SUE_SPI_REG_MWCR		SUE_SPI_BASEADDRESS(0x0C)
#define	SUE_SPI_REG_SER			SUE_SPI_BASEADDRESS(0x10)
#define	SUE_SPI_REG_BAUDR		SUE_SPI_BASEADDRESS(0x14)
#define	SUE_SPI_REG_TXFTLR		SUE_SPI_BASEADDRESS(0x18)
#define	SUE_SPI_REG_RXFTLR		SUE_SPI_BASEADDRESS(0x1C)
#define	SUE_SPI_REG_TXFLR		SUE_SPI_BASEADDRESS(0x20)
#define	SUE_SPI_REG_RXFLR		SUE_SPI_BASEADDRESS(0x24)
#define	SUE_SPI_REG_SR			SUE_SPI_BASEADDRESS(0x28)
#define	SUE_SPI_REG_IMR			SUE_SPI_BASEADDRESS(0x2C)
#define	SUE_SPI_REG_ISR			SUE_SPI_BASEADDRESS(0x30)
#define	SUE_SPI_REG_RISR		SUE_SPI_BASEADDRESS(0x34)
#define	SUE_SPI_REG_TXOICR		SUE_SPI_BASEADDRESS(0x38)
#define	SUE_SPI_REG_RXOICR		SUE_SPI_BASEADDRESS(0x3C)
#define	SUE_SPI_REG_RXUICR		SUE_SPI_BASEADDRESS(0x40)
#define	SUE_SPI_REG_ICR			SUE_SPI_BASEADDRESS(0x48)
#define	SUE_SPI_REG_DMACR		SUE_SPI_BASEADDRESS(0x4C)
#define	SUE_SPI_REG_DMATDLR		SUE_SPI_BASEADDRESS(0x50)
#define	SUE_SPI_REG_DMARDLR		SUE_SPI_BASEADDRESS(0x54)
#define	SUE_SPI_REG_DR			SUE_SPI_BASEADDRESS(0x60)
#define	SUE_SPI_REG_SPICTRLR0	SUE_SPI_BASEADDRESS(0xF4)


enum sspi_type {
	SPI_DIR_RX  = 0,
	SPI_DIR_TX,
};


enum {
	SSPI_TRIGGER_START = 0,
	SSPI_TRIGGER_STOP,
};

enum sof_spi_type {
	SOF_SPI_INTEL_SLAVE = 0,
	SOF_SPI_INTEL_MASTER,
};

struct spi_dma_config {
	enum sspi_type type;
	uint32_t src_width;
	uint32_t dest_width;
	uint32_t src_msize;
	uint32_t dest_msize;
	uint32_t src_buf;
	uint32_t dest_buf;
	uint32_t transfer_len;
	uint32_t lbm;	/* loopback mode */
	uint32_t cyclic;
};

struct sspi_plat_fifo_data {
	uint32_t offset;
	uint32_t width;
	uint32_t depth;
	uint32_t watermark;
	uint32_t handshake;
};

struct sspi_plat_data {
	uint32_t base;
	uint32_t irq;
	uint32_t flags;
	struct sspi_plat_fifo_data fifo[2];
	/* 00--standard spi; 01--dual spi; 10--quad spi */
	uint32_t spi_format;
	/* 00011-4bit; 00100-5bit; ... 11111-32bit */
	uint32_t spi_dfs_32;
};

struct spi_reg_list {
	uint32_t ctrlr0;
	uint32_t ctrlr1;
	uint32_t ssienr;
	uint32_t txftlr;
	uint32_t rxftlr;
	uint32_t imr;
	uint32_t dmacr;		/* dma control register */
	uint32_t dmatdlr;	/* dma transmit data level */
	uint32_t dmardlr;	/* dma receive data level */
};

#define SPI_BUFFER_SIZE		256

struct sspi {
	uint32_t type;
	uint32_t index;
	uint32_t chan[2];	/* spi-slave rx/tx dma */
	uint32_t rx_size;
	uint32_t tx_size;
	uint8_t *rx_buffer;
	uint8_t *tx_buffer;
	struct dma *dma;
	struct spi_reg_list reg;
	struct sspi_plat_data plat_data;
	struct spi_dma_config config[2];
	const struct sspi_ops *ops;
	uint32_t ipc_status;

	struct sof_ipc_hdr hdr;
};


struct sspi_ops {
	int (*set_config)(struct sspi *spi, struct spi_dma_config *spi_dma_cfg);
	int (*trigger)(struct sspi *spi, int cmd, int direction);
	int (*probe)(struct sspi *spi);
	int (*set_loopback_mode)(struct sspi *spi, uint32_t lbm);
};

#define sspi_set_drvdata(spi, data) \
	spi->private = data
#define sspi_get_drvdata(spi) \
	spi->private
#define sspi_base(spi) \
	spi->plat_data.base
#define sspi_irq(spi) \
	spi->plat_data.irq
#define sspi_fifo(spi, direction) \
	spi->plat_data.fifo[direction].offset


struct sspi *sspi_get(uint32_t type);
int spi_slave_init(struct sspi *spi, uint32_t type);

/* SPI-slave interface formatting */
static inline int sspi_set_config(struct sspi *spi,
								struct spi_dma_config *spi_cfg)
{
	return spi->ops->set_config(spi, spi_cfg);
}

/* SPI-slave interface formatting */
static inline int sspi_set_loopback_mode(struct sspi *spi, uint32_t lbm)
{
	return spi->ops->set_loopback_mode(spi, lbm);
}

/* SPI-slave interface trigger */
static inline int sspi_trigger(struct sspi *spi, int cmd, int direction)
{
	return spi->ops->trigger(spi, cmd, direction);
}

/* SPI-slave interface Probe */
static inline int sspi_probe(struct sspi *spi)
{
	return spi->ops->probe(spi);
}
#endif
