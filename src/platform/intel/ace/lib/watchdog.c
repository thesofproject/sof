// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2023 Intel Corporation
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#include <sof/lib/uuid.h>
#include <rtos/idc.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/ipc/msg.h>
#include <ipc4/notification.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>

#if DT_NODE_HAS_STATUS(DT_NODELABEL(adsp_watchdog), okay)
#include <adsp_watchdog.h>
#include <intel_adsp_ipc.h>
#include <intel_adsp_ipc_devtree.h>

LOG_MODULE_REGISTER(wdt, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(wdt);

DECLARE_TR_CTX(wdt_tr, SOF_UUID(wdt_uuid), LOG_LEVEL_INFO);

static const struct device *const watchdog = DEVICE_DT_GET(DT_NODELABEL(adsp_watchdog));
static struct ipc_msg secondary_timeout_ipc;

static void watchdog_primary_core_action_on_timeout(void)
{
	struct ipc4_watchdog_timeout_notification notif;

	/* Send Watchdog Timeout IPC notification */
	ipc4_notification_watchdog_init(&notif, cpu_get_id(), true);
	intel_adsp_ipc_send_message_emergency(INTEL_ADSP_IPC_HOST_DEV,
					      notif.primary.dat, notif.extension.dat);
}

static void watchdog_secondary_core_action_on_timeout(void)
{
	struct idc_msg msg;

	/* Send Watchdog Timeout IDC notification */
	msg.header = IDC_MSG_SECONDARY_CORE_CRASHED | IDC_SCC_CORE(cpu_get_id()) |
		     IDC_SCC_REASON(IDC_SCC_REASON_WATCHDOG);
	msg.extension = 0;
	msg.core = 0;
	msg.size = 0;
	msg.payload = NULL;
	idc_send_msg(&msg, IDC_NON_BLOCKING);
}

/* This function is called by idc handler on primary core */
void watchdog_secondary_core_timeout(int core)
{
	struct ipc4_watchdog_timeout_notification notif;

	/* Send Watchdog Timeout IPC notification */
	ipc4_notification_watchdog_init(&notif, core, true);
	secondary_timeout_ipc.header = notif.primary.dat;
	secondary_timeout_ipc.extension = notif.extension.dat;
	ipc_msg_send(&secondary_timeout_ipc, NULL, true);
}

static void watchdog_timeout(const struct device *dev, int core)
{
	if (core == PLATFORM_PRIMARY_CORE_ID)
		watchdog_primary_core_action_on_timeout();
	else
		watchdog_secondary_core_action_on_timeout();
}

void watchdog_init(void)
{
	int i, ret;
	const struct wdt_timeout_cfg watchdog_config = {
		.window.max = LL_WATCHDOG_TIMEOUT_US / 1000,
		.callback = watchdog_timeout,
	};

	secondary_timeout_ipc.tx_data = NULL;
	secondary_timeout_ipc.tx_size = 0;
	list_init(&secondary_timeout_ipc.list);

	ret = wdt_install_timeout(watchdog, &watchdog_config);
	if (ret) {
		tr_warn(&wdt_tr, "Watchdog install timeout error %d", ret);
		return;
	}

	for (i = 0; i < arch_num_cpus(); i++)
		intel_adsp_watchdog_pause(watchdog, i);

	ret = wdt_setup(watchdog, 0);
	if (ret)
		tr_warn(&wdt_tr, "Watchdog setup error %d", ret);
}

void watchdog_enable(int core)
{
	intel_adsp_watchdog_resume(watchdog, core);
}

void watchdog_disable(int core)
{
	intel_adsp_watchdog_pause(watchdog, core);
}

void watchdog_feed(int core)
{
	wdt_feed(watchdog, core);
}
#else
void watchdog_enable(int core) {}
void watchdog_disable(int core) {}
void watchdog_feed(int core) {}
void watchdog_init(void) {}
void watchdog_secondary_core_timeout(int core) {}
#endif
