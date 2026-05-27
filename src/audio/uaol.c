/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#include <zephyr/drivers/uaol.h>
#include <rtos/string.h>
#include <sof/tlv.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/uaol.h>

LOG_MODULE_REGISTER(uaol, CONFIG_SOF_LOG_LEVEL);

struct ipc4_uaol_link_capabilities {
	uint32_t input_streams_supported          : 4;
	uint32_t output_streams_supported         : 4;
	uint32_t bidirectional_streams_supported  : 5;
	uint32_t rsvd                             : 19;
	uint32_t max_tx_fifo_size;
	uint32_t max_rx_fifo_size;
} __packed __aligned(4);

struct ipc4_uaol_capabilities {
	uint32_t link_count;
	struct ipc4_uaol_link_capabilities link_caps[];
} __packed __aligned(4);

#define DEV_AND_COMMA(node) DEVICE_DT_GET(node),
static const struct device *uaol_devs[] = {
	DT_FOREACH_STATUS_OKAY(intel_adsp_uaol, DEV_AND_COMMA)
};

#if !CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY
__cold void tlv_value_set_uaol_caps(struct sof_tlv *tuple, uint32_t type)
{
	const size_t dev_count = ARRAY_SIZE(uaol_devs);
	struct uaol_capabilities dev_cap;
	struct ipc4_uaol_capabilities *caps = (struct ipc4_uaol_capabilities *)tuple->value;
	size_t caps_size = offsetof(struct ipc4_uaol_capabilities, link_caps[dev_count]);
	size_t i;
	int ret;

	assert_can_be_cold();

	memset(caps, 0, caps_size);

	caps->link_count = dev_count;
	for (i = 0; i < dev_count; i++) {
		ret = uaol_get_capabilities(uaol_devs[i], &dev_cap);
		if (ret)
			continue;

		caps->link_caps[i].input_streams_supported = dev_cap.input_streams;
		caps->link_caps[i].output_streams_supported = dev_cap.output_streams;
		caps->link_caps[i].bidirectional_streams_supported = dev_cap.bidirectional_streams;
		caps->link_caps[i].max_tx_fifo_size = dev_cap.max_tx_fifo_size;
		caps->link_caps[i].max_rx_fifo_size = dev_cap.max_rx_fifo_size;
	}

	tlv_value_set(tuple, type, caps_size, caps);
}
#endif /* !CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY */

__cold int uaol_stream_id_to_hda_link_stream_id(int uaol_stream_id)
{
	size_t dev_count = ARRAY_SIZE(uaol_devs);
	size_t i;

	assert_can_be_cold();

	for (i = 0; i < dev_count; i++) {
		int hda_link_stream_id = uaol_get_mapped_hda_link_stream_id(uaol_devs[i],
									    uaol_stream_id);
		if (hda_link_stream_id >= 0)
			return hda_link_stream_id;
	}

	return -1;
}
