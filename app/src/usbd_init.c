/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/logging/log.h>
#include <sample_usbd.h>

LOG_MODULE_REGISTER(usbd_sample_config, LOG_LEVEL_INF);

static const char *const blocklist[] = {
	"dfu_dfu",
	NULL,
};

USBD_DEVICE_DEFINE(sample_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_SAMPLE_USBD_VID, CONFIG_SAMPLE_USBD_PID);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, CONFIG_SAMPLE_USBD_MANUFACTURER);

#if defined(CONFIG_PLATFORM_TEENSY41) && defined(CONFIG_TEENSY41_BOARD_B)
USBD_DESC_PRODUCT_DEFINE(sample_product, "SOF Teensy 4.1 Audio B");
#elif defined(CONFIG_PLATFORM_TEENSY41) && defined(CONFIG_TEENSY41_BOARD_A)
USBD_DESC_PRODUCT_DEFINE(sample_product, "SOF Teensy 4.1 Audio A");
#else
USBD_DESC_PRODUCT_DEFINE(sample_product, CONFIG_SAMPLE_USBD_PRODUCT);
#endif

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

#if defined(CONFIG_BOARD_TEENSY41) || defined(CONFIG_BOARD_TEENSY40)
static int teensy_reboot_to_dev(const struct usbd_context *const ctx,
				const struct usb_setup_packet *const setup,
				const struct net_buf *const buf)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(setup);
	ARG_UNUSED(buf);

	/* Trigger PJRC MKL02 bootloader companion to enter HalfKay HID mode */
	__asm__ volatile("bkpt #251");
	return 0;
}

USBD_VREQUEST_DEFINE(teensy_reboot_vreq, 0xFB, NULL, teensy_reboot_to_dev);
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

	err = usbd_add_descriptor(&sample_usbd, &sample_product);
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

#if defined(CONFIG_BOARD_TEENSY41) || defined(CONFIG_BOARD_TEENSY40)
	err = usbd_device_register_vreq(&sample_usbd, &teensy_reboot_vreq);
	if (err) {
		LOG_ERR("Failed to register HalfKay reboot request (%d)", err);
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
