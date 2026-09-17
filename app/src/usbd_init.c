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
#include <zephyr/usb/usbd_msg.h>
#include <zephyr/logging/log.h>
#include <sample_usbd.h>

#if defined(CONFIG_SOC_SERIES_ESP32S3)
#include <soc/rtc_cntl_reg.h>
#include <esp_system.h>
#include <esp_rom_sys.h>
#include <zephyr/drivers/uart.h>
#endif

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

USBD_DESC_PRODUCT_DEFINE(spider_product, "SOF ESP32P4 USB Spider");
USBD_DESC_PRODUCT_DEFINE(aphid_product, "SOF ESP32P4 USB Aphid");
USBD_DESC_PRODUCT_DEFINE(pallas_product, "SOF ESP32P4 USB Pallas");
USBD_DESC_PRODUCT_DEFINE(ceres_product, "SOF ESP32P4 USB Ceres");
USBD_DESC_PRODUCT_DEFINE(s3_a_product, "SOF ESP32S3 Master");
USBD_DESC_PRODUCT_DEFINE(s3_b_product, "SOF ESP32S3 Slave");
USBD_DESC_PRODUCT_DEFINE(s3_default_product, "SOF ESP32S3 Audio");
USBD_DESC_PRODUCT_DEFINE(default_product, "SOF USB Audio");

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

#if defined(CONFIG_SOC_SERIES_ESP32S3)
static void usbd_internal_msg_cb(struct usbd_context *const ctx,
				 const struct usbd_msg *const msg)
{
	ARG_UNUSED(ctx);
#if defined(CONFIG_USBD_CDC_ACM_CLASS)
	if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING) {
		uint32_t baud = 0;
		if (uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_BAUD_RATE, &baud) == 0) {
			if (baud == 1200) {
				printk("[USB CDC-ACM] 1200 baud touch detected! Rebooting into ROM download mode...\n");
				k_msleep(50);
				REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
				esp_restart();
			}
		}
	}
#endif
}
#endif

struct usbd_context *sample_usbd_setup_device(usbd_msg_cb_t msg_cb)
{
	int err;
	uint8_t mac[6] = {0};
	extern int esp_efuse_mac_get_default(uint8_t *mac);
	esp_efuse_mac_get_default(mac);

	printk("[USB INIT] Chip MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	struct usbd_desc_node *product_desc = &default_product;
#if defined(CONFIG_PLATFORM_ESP32S3)
	if (mac[0] == 0x34 && (mac[5] == 0x6C || mac[5] == 0x6c)) {
		LOG_INF("Board Identity: ESP32-S3 BOARD A MASTER (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &s3_a_product;
	} else if (mac[0] == 0x20 && mac[5] == 0x84) {
		LOG_INF("Board Identity: ESP32-S3 BOARD B SLAVE (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &s3_b_product;
	} else {
		LOG_INF("Board Identity: Generic ESP32-S3 (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &s3_default_product;
	}
#else
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
	} else if (mac[0] == 0xe8 && (mac[5] == 0x6C || mac[5] == 0x6c)) {
		LOG_INF("Board Identity: CERES RX SLAVE (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &ceres_product;
	} else {
		LOG_INF("Board Identity: Generic ESP32-P4 (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
		        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		product_desc = &default_product;
	}
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

#if defined(CONFIG_SOC_SERIES_ESP32S3)
	if (msg_cb == NULL) {
		msg_cb = usbd_internal_msg_cb;
	}
#endif

	if (msg_cb != NULL) {
		err = usbd_msg_register_cb(&sample_usbd, msg_cb);
		if (err) {
			LOG_ERR("Failed to register message callback");
			return NULL;
		}
	}

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
