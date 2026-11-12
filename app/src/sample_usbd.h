/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SAMPLES_SUBSYS_USB_COMMON_SAMPLE_USBD_H
#define ZEPHYR_SAMPLES_SUBSYS_USB_COMMON_SAMPLE_USBD_H

#include <stdint.h>
#include <zephyr/usb/usbd.h>

struct usbd_context *sample_usbd_init_device(usbd_msg_cb_t msg_cb);
struct usbd_context *sample_usbd_setup_device(usbd_msg_cb_t msg_cb);

#endif /* ZEPHYR_SAMPLES_SUBSYS_USB_COMMON_SAMPLE_USBD_H */
