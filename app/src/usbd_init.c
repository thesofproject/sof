/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/logging/log.h>
#include <sample_usbd.h>

LOG_MODULE_REGISTER(usbd_sample_config, LOG_LEVEL_INF);

/* By default, do not register the USB DFU class DFU mode instance. */
static const char *const blocklist[] = {
	"dfu_dfu",
	NULL,
};

/*
 * Instantiate a context named sample_usbd using the default USB device
 * controller, the Zephyr project vendor ID, and the sample product ID.
 */
USBD_DEVICE_DEFINE(sample_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_SAMPLE_USBD_VID, CONFIG_SAMPLE_USBD_PID);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, CONFIG_SAMPLE_USBD_MANUFACTURER);

#if defined(CONFIG_PLATFORM_TEENSY41)
#if defined(CONFIG_TEENSY41_BOARD_B)
USBD_DESC_PRODUCT_DEFINE(teensyb_product, "SOF Teensy 4.1 Audio B");
#elif defined(CONFIG_TEENSY41_BOARD_A)
USBD_DESC_PRODUCT_DEFINE(teensya_product, "SOF Teensy 4.1 Audio A");
#else
USBD_DESC_PRODUCT_DEFINE(default_product, "SOF Teensy 4.1 Audio");
#endif
#elif defined(CONFIG_PLATFORM_NORDIC)
USBD_DESC_PRODUCT_DEFINE(default_product, "Nordic nRF54LM20 SOF UAC2");
#else
USBD_DESC_PRODUCT_DEFINE(spider_product, "SOF ESP32P4 USB Spider");
USBD_DESC_PRODUCT_DEFINE(aphid_product, "SOF ESP32P4 USB Aphid");
USBD_DESC_PRODUCT_DEFINE(pallas_product, "SOF ESP32P4 USB Pallas");
USBD_DESC_PRODUCT_DEFINE(ceres_product, "SOF ESP32P4 USB Ceres");
USBD_DESC_PRODUCT_DEFINE(default_product, "SOF ESP32P4 USB");
#endif

USBD_DESC_STRING_DEFINE(clock_master_str, "Clock Master", USBD_DUT_STRING_INTERFACE);

IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn)));

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

static const uint8_t attributes = (IS_ENABLED(CONFIG_SAMPLE_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

USBD_CONFIGURATION_DEFINE(sample_fs_config,
			  attributes,
			  CONFIG_SAMPLE_USBD_MAX_POWER, &fs_cfg_desc);

USBD_CONFIGURATION_DEFINE(sample_hs_config,
			  attributes,
			  CONFIG_SAMPLE_USBD_MAX_POWER, &hs_cfg_desc);

enum usbd_speed sample_usbd_get_speed(void)
{
	return usbd_bus_speed(&sample_usbd);
}

#if defined(CONFIG_PLATFORM_TEENSY41)
#include <fsl_common.h>
#include <fsl_sai.h>
#include <fsl_edma.h>
#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/lib/dai-zephyr.h>

extern uint32_t g_rx_pkt_cnt;
extern uint32_t g_tx_pkt_cnt;
extern struct pipeline *sof_static_pipeline_get(uint32_t pipeline_id);
extern struct comp_dev *sof_static_comp_get(uint32_t comp_id);

struct teensy_diag_data {
	uint32_t tcsr;
	uint32_t rcsr;
	uint32_t tcr2;
	uint32_t rcr2;
	uint32_t tcr3;
	uint32_t rcr3;
	uint32_t tfr0;
	uint32_t rfr0;
	uint32_t rx_pkt_cnt;
	uint32_t tx_pkt_cnt;
	uint32_t pipe1_status;
	uint32_t pipe2_status;
	uint32_t dai5_state;
	uint32_t dai6_state;
	uint32_t dma_tx_csr;
	uint32_t dma_rx_csr;
	uint32_t dma_tx_saddr;
	uint32_t dma_rx_daddr;
	uint32_t dma_tx_citer;
	uint32_t dma_rx_citer;
	uint32_t c1_state;
	uint32_t c2_state;
	uint32_t c9_state;
	uint32_t c10_state;
	uint32_t tcr4;
	uint32_t rcr4;
	uint32_t tcr5;
	uint32_t rcr5;
	uint32_t pin8_mux;
	uint32_t pin8_daisy;
	uint32_t pin7_mux;
	uint32_t pin8_pad;
	uint32_t pin7_pad;
	uint32_t gpio1_psr;
	uint32_t gpio2_psr;
	uint32_t interface_type; /* 1 = SAI1 I2S, 2 = S/PDIF */
	uint32_t g_dbg[8];
};

#define TEENSY_INTERFACE_SAI1_I2S 1
#define TEENSY_INTERFACE_SPDIF    2

extern uint32_t g_diag_debug[8];

static struct net_buf *teensy_diag_to_host(const struct usbd_context *const ctx,
					   const struct usb_setup_packet *const setup)
{
	struct net_buf *buf;
	struct teensy_diag_data diag;
	uint16_t len;

	memset(&diag, 0, sizeof(diag));
#if defined(CONFIG_TEENSY41_INTERFACE_SPDIF)
	diag.interface_type = TEENSY_INTERFACE_SPDIF;
	diag.tcsr = SPDIF->SCR;
	diag.rcsr = SPDIF->SRPC;
	diag.tcr2 = SPDIF->STC;
	diag.rcr2 = SPDIF->SRFM;
	diag.tcr3 = SPDIF->SIS;
	diag.rcr3 = SPDIF->SIE;
	diag.tfr0 = 0;
	diag.rfr0 = 0;
	diag.dma_tx_csr = DMA0->TCD[2].CSR;
	diag.dma_rx_csr = DMA0->TCD[3].CSR;
	diag.dma_tx_saddr = DMA0->TCD[2].SADDR;
	diag.dma_rx_daddr = DMA0->TCD[3].DADDR;
	diag.dma_tx_citer = DMA0->TCD[2].CITER_ELINKNO;
	diag.dma_rx_citer = DMA0->TCD[3].CITER_ELINKNO;
#else
	diag.interface_type = TEENSY_INTERFACE_SAI1_I2S;
	diag.tcsr = SAI1->TCSR;
	diag.rcsr = SAI1->RCSR;
	diag.tcr2 = SAI1->TCR2;
	diag.rcr2 = SAI1->RCR2;
	diag.tcr3 = SAI1->TCR3;
	diag.rcr3 = SAI1->RCR3;
	diag.tfr0 = (SAI1->TCR3 & I2S_TCR3_TCE(4)) ? SAI1->TFR[2] : SAI1->TFR[0];
	diag.rfr0 = SAI1->RFR[0];
	diag.dma_tx_csr = DMA0->TCD[0].CSR;
	diag.dma_rx_csr = DMA0->TCD[1].CSR;
	diag.dma_tx_saddr = DMA0->TCD[0].SADDR;
	diag.dma_rx_daddr = DMA0->TCD[1].DADDR;
	diag.dma_tx_citer = DMA0->TCD[0].CITER_ELINKNO;
	diag.dma_rx_citer = DMA0->TCD[1].CITER_ELINKNO;
#endif

	diag.rx_pkt_cnt = g_rx_pkt_cnt;
	diag.tx_pkt_cnt = g_tx_pkt_cnt;

	struct pipeline *p1 = sof_static_pipeline_get(1);
	struct pipeline *p2 = sof_static_pipeline_get(2);
	if (p1) diag.pipe1_status = p1->status;
	if (p2) diag.pipe2_status = p2->status;

	struct comp_dev *c1 = sof_static_comp_get(1);
	struct comp_dev *c2 = sof_static_comp_get(2);
	struct comp_dev *c5 = sof_static_comp_get(5);
	struct comp_dev *c6 = sof_static_comp_get(6);
	struct comp_dev *c9 = sof_static_comp_get(9);
	struct comp_dev *c10 = sof_static_comp_get(10);
	if (c1) diag.c1_state = c1->state;
	if (c2) diag.c2_state = c2->state;
	if (c5) diag.dai5_state = c5->state;
	if (c6) diag.dai6_state = c6->state;
	if (c9) diag.c9_state = c9->state;
	if (c10) diag.c10_state = c10->state;

	diag.tcr4 = SAI1->TCR4;
	diag.rcr4 = SAI1->RCR4;
	diag.tcr5 = SAI1->TCR5;
	diag.rcr5 = SAI1->RCR5;
	diag.pin8_mux = *(volatile uint32_t *)0x401F817C;
	diag.pin8_daisy = *(volatile uint32_t *)0x401F8594;
	diag.pin7_mux = *(volatile uint32_t *)0x401F8180;
	diag.pin8_pad = *(volatile uint32_t *)0x401F836C;
	diag.pin7_pad = *(volatile uint32_t *)0x401F8370;
	diag.gpio1_psr = *(volatile uint32_t *)0x401B8008;
	diag.gpio2_psr = *(volatile uint32_t *)0x401BC008;
	memcpy(diag.g_dbg, g_diag_debug, sizeof(diag.g_dbg));

	len = MIN(setup->wLength, sizeof(diag));
	buf = usbd_ep_ctrl_data_in_alloc(ctx, len);
	if (!buf) return NULL;
	net_buf_add_mem(buf, &diag, len);
	return buf;
}

static int teensy_reboot_to_dev(const struct usbd_context *const ctx,
				const struct usb_setup_packet *const setup,
				const struct net_buf *const buf)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(setup);
	ARG_UNUSED(buf);
	__asm__ volatile("bkpt #251");
	return 0;
}

static struct net_buf *teensy_peek_to_host(const struct usbd_context *const ctx,
					   const struct usb_setup_packet *const setup)
{
	uint32_t addr = (setup->wIndex << 16) | setup->wValue;
	uint16_t len = MIN(setup->wLength, 64);
	if (!len) return NULL;
	struct net_buf *buf = usbd_ep_ctrl_data_in_alloc(ctx, len);
	if (!buf) return NULL;
	net_buf_add_mem(buf, (const void *)addr, len);
	return buf;
}

static int teensy_poke_to_dev(const struct usbd_context *const ctx,
			      const struct usb_setup_packet *const setup,
			      const struct net_buf *const buf)
{
	ARG_UNUSED(ctx);
	uint32_t addr = (setup->wIndex << 16) | setup->wValue;
	if (buf && buf->len > 0) {
		if (setup->wLength == 1) {
			*(volatile uint8_t *)addr = buf->data[0];
		} else if (setup->wLength == 2) {
			uint16_t val;
			memcpy(&val, buf->data, 2);
			*(volatile uint16_t *)addr = val;
		} else if (setup->wLength == 4) {
			uint32_t val;
			memcpy(&val, buf->data, 4);
			*(volatile uint32_t *)addr = val;
		}
	}
	return 0;
}

USBD_VREQUEST_DEFINE(teensy_reboot_vreq, 0xFB, NULL, teensy_reboot_to_dev);
USBD_VREQUEST_DEFINE(teensy_diag_vreq, 0xFC, teensy_diag_to_host, NULL);
USBD_VREQUEST_DEFINE(teensy_peek_poke_vreq, 0xFD, teensy_peek_to_host, teensy_poke_to_dev);
#endif

static void sample_fix_code_triple(struct usbd_context *uds_ctx,
				   const enum usbd_speed speed)
{
	if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_MIDI2_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_VIDEO_CLASS)) {
		usbd_device_set_code_triple(uds_ctx, speed,
					    USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
	}
}

struct usbd_context *sample_usbd_setup_device(usbd_msg_cb_t msg_cb)
{
	int err;
#if defined(CONFIG_PLATFORM_TEENSY41)
	struct usbd_desc_node *product_desc;
#if defined(CONFIG_TEENSY41_BOARD_B)
	product_desc = &teensyb_product;
	LOG_INF("Board Identity: TEENSY 4.1 BOARD B (Slave / Rx)");
#elif defined(CONFIG_TEENSY41_BOARD_A)
	product_desc = &teensya_product;
	LOG_INF("Board Identity: TEENSY 4.1 BOARD A (Master / Tx)");
#else
	product_desc = &default_product;
	LOG_INF("Board Identity: Generic Teensy 4.1 Audio");
#endif
#elif defined(CONFIG_PLATFORM_NORDIC)
	struct usbd_desc_node *product_desc = &default_product;
	LOG_INF("Board Identity: Nordic nRF54LM20 SOF UAC2 Audio");
#elif defined(CONFIG_PLATFORM_ESP32P4)
	uint8_t mac[6] = {0};
	extern int esp_efuse_mac_get_default(uint8_t *mac);
	esp_efuse_mac_get_default(mac);

	printk("[USB INIT] Chip MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	struct usbd_desc_node *product_desc = &default_product;
	if (mac[5] == 0xC7 || mac[5] == 0xc7) {
		LOG_INF("Board Identity: SPIDER PDM DUT (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &spider_product;
	} else if (mac[5] == 0x15) {
		LOG_INF("Board Identity: APHID I2S DUT (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &aphid_product;
	} else if (mac[5] == 0x17) {
		LOG_INF("Board Identity: PALLAS TX MASTER (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &pallas_product;
	} else if (mac[5] == 0x6C || mac[5] == 0x6c) {
		LOG_INF("Board Identity: CERES RX SLAVE (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &ceres_product;
	} else {
		LOG_INF("Board Identity: Generic ESP32-P4 (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &default_product;
	}
#else
	struct usbd_desc_node *product_desc = &default_product;
#endif

	err = usbd_add_descriptor(&sample_usbd, &sample_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&sample_usbd, product_desc);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		return NULL;
	}

	IF_ENABLED(CONFIG_HWINFO, (
		err = usbd_add_descriptor(&sample_usbd, &sample_sn);
		if (err) {
			LOG_ERR("Failed to initialize SN descriptor (%d)", err);
			return NULL;
		}
	))

	err = usbd_add_descriptor(&sample_usbd, &clock_master_str);
	if (err) {
		LOG_ERR("Failed to initialize Clock Master string descriptor (%d)", err);
		return NULL;
	}
	LOG_INF("Registered Clock Master string descriptor at index %u", clock_master_str.str.idx);

	if (USBD_SUPPORTS_HIGH_SPEED &&
	    usbd_caps_speed(&sample_usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&sample_usbd, USBD_SPEED_HS,
					     &sample_hs_config);
		if (err) {
			LOG_ERR("Failed to add High-Speed configuration");
			return NULL;
		}

		err = usbd_register_all_classes(&sample_usbd, USBD_SPEED_HS, 1,
						blocklist);
		if (err) {
			LOG_ERR("Failed to register classes");
			return NULL;
		}

		sample_fix_code_triple(&sample_usbd, USBD_SPEED_HS);
	}

	err = usbd_add_configuration(&sample_usbd, USBD_SPEED_FS,
				     &sample_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration");
		return NULL;
	}

	err = usbd_register_all_classes(&sample_usbd, USBD_SPEED_FS, 1, blocklist);
	if (err) {
		LOG_ERR("Failed to register classes");
		return NULL;
	}

	sample_fix_code_triple(&sample_usbd, USBD_SPEED_FS);
	usbd_self_powered(&sample_usbd, attributes & USB_SCD_SELF_POWERED);

	if (msg_cb != NULL) {
		err = usbd_msg_register_cb(&sample_usbd, msg_cb);
		if (err) {
			LOG_ERR("Failed to register message callback");
			return NULL;
		}
	}

#if defined(CONFIG_PLATFORM_TEENSY41)
	err = usbd_device_register_vreq(&sample_usbd, &teensy_reboot_vreq);
	if (err) {
		LOG_ERR("Failed to register vendor reboot request (%d)", err);
	}
	err = usbd_device_register_vreq(&sample_usbd, &teensy_diag_vreq);
	if (err) {
		LOG_ERR("Failed to register vendor diag request (%d)", err);
	}
	err = usbd_device_register_vreq(&sample_usbd, &teensy_peek_poke_vreq);
	if (err) {
		LOG_ERR("Failed to register vendor peek/poke request (%d)", err);
	}
#endif

	return &sample_usbd;
}

struct usbd_context *sample_usbd_init_device(usbd_msg_cb_t msg_cb)
{
	int err;

	if (sample_usbd_setup_device(msg_cb) == NULL) {
		return NULL;
	}

	err = usbd_init(&sample_usbd);
	if (err) {
		LOG_ERR("Failed to initialize device support");
		return NULL;
	}

	return &sample_usbd;
}
