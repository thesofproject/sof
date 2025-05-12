// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Tomasz Leman <tomasz.m.leman@intel.com>

/**
 * \file
 * \brief Zephyr RTOS CPU implementation file
 * \authors Tomasz Leman <tomasz.m.leman@intel.com>
 */

#include <sof/audio/component.h>
#if CONFIG_INTEL_ADSP_MIC_PRIVACY
#include <sof/audio/mic_privacy_manager.h>
#ifdef CONFIG_ADSP_IMR_CONTEXT_SAVE
static uint32_t mic_disable_status;
#endif /* CONFIG_ADSP_IMR_CONTEXT_SAVE */
#endif /* CONFIG_INTEL_ADSP_MIC_PRIVACY */
#include <sof/init.h>
#include <sof/lib/cpu.h>
#include <sof/lib/pm_runtime.h>
#include <ipc/topology.h>
#include <module/module/base.h>
#include <rtos/alloc.h>

#include "../audio/copier/copier.h"

/* Zephyr includes */
#include <version.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/smp.h>
#include <zephyr/device.h>
#include <zephyr/drivers/mm/mm_drv_intel_adsp_mtl_tlb.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/hibernate.h>

#if CONFIG_MULTICORE && CONFIG_SMP

extern K_KERNEL_STACK_ARRAY_DEFINE(z_interrupt_stacks, CONFIG_MP_MAX_NUM_CPUS,
				   CONFIG_ISR_STACK_SIZE);

static void secondary_init(void *arg)
{
	secondary_core_init(sof_get());
}

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
#include <sof/trace/trace.h>
#include <rtos/wait.h>

LOG_MODULE_DECLARE(zephyr, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx zephyr_tr;

/* address where zephyr PM will save memory during D3 transition */
#ifdef CONFIG_ADSP_IMR_CONTEXT_SAVE
extern void *global_imr_ram_storage;
#endif

#if CONFIG_PM
#ifdef CONFIG_ADSP_IMR_CONTEXT_SAVE
/*
 * SOF explicitly manages DAI power states to meet the audio-specific requirement
 * that all audio pipelines must be paused prior to entering the D3 power state.
 * Zephyr's PM framework is designed to suspend devices based on their runtime
 * usage, which does not align with the audio pipeline lifecycle managed by SOF.
 * During system PM transitions, Zephyr does not automatically handle the suspension
 * of DAIs, as it lacks the context of audio pipeline states. Therefore, SOF
 * implements additional logic to synchronize DAI states with the DSP core during
 * audio pipeline pauses and resumes. This ensures seamless audio performance and
 * data integrity across D3 transitions, which is critical for SOF's operation
 * and currently outside the scope of Zephyr's device-level PM capabilities.
 */

static void suspend_dais(void)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	struct processing_module *mod;
	struct copier_data *cd;
	struct dai_data *dd;

	list_for_item(clist, &ipc_get()->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT || dev_comp_type(icd->cd) != SOF_COMP_DAI)
			continue;

		mod = comp_mod(icd->cd);
		cd = module_get_private_data(mod);
#if CONFIG_INTEL_ADSP_MIC_PRIVACY
		if (cd->mic_priv && mic_privacy_manager_get_policy() == MIC_PRIVACY_FW_MANAGED)
			mic_disable_status = mic_privacy_get_mic_disable_status();
#endif
		dd = cd->dd[0];
		if (dai_remove(dd->dai->dev) < 0) {
			tr_err(&zephyr_tr, "DAI suspend failed, type %d index %d",
			       dd->dai->type, dd->dai->index);
		}
	}
}

static void resume_dais(void)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	struct processing_module *mod;
	struct copier_data *cd;
	struct dai_data *dd;

#if CONFIG_INTEL_ADSP_MIC_PRIVACY
	/* Re-initialize mic privacy manager first to ensure proper state before DAI resume */
	mic_privacy_manager_init();
#endif

	list_for_item(clist, &ipc_get()->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT || dev_comp_type(icd->cd) != SOF_COMP_DAI)
			continue;

		mod = comp_mod(icd->cd);
		cd = module_get_private_data(mod);
		dd = cd->dd[0];
		if (dai_probe(dd->dai->dev) < 0) {
			tr_err(&zephyr_tr, "DAI resume failed, type %d index %d",
			       dd->dai->type, dd->dai->index);
		}

#if CONFIG_INTEL_ADSP_MIC_PRIVACY
		if (cd->mic_priv && mic_privacy_manager_get_policy() == MIC_PRIVACY_FW_MANAGED) {
			uint32_t current_mic_status = mic_privacy_get_mic_disable_status();

			if (mic_disable_status != current_mic_status) {
				tr_dbg(&zephyr_tr, "MIC privacy settings cheange after D3");
				struct mic_privacy_settings settings;

				/* Update privacy settings based on new state */
				mic_privacy_fill_settings(&settings, current_mic_status);
				mic_privacy_propagate_settings(&settings);
				/* Ensure we're starting from a clean state with no fade effects */
				if (cd->mic_priv->mic_privacy_state) {
					/* Force immediate mute without fade effect */
					cd->mic_priv->mic_privacy_state = MIC_PRIV_MUTED;
					cd->mic_priv->fade_in_out_bytes = 0;
					cd->mic_priv->mic_priv_gain_params.gain_env = 0;
					cd->mic_priv->mic_priv_gain_params.fade_in_sg_count = 0;
				}
			}
		}
#endif
	}
}
#endif /* CONFIG_ADSP_IMR_CONTEXT_SAVE */

void cpu_notify_state_entry(enum pm_state state)
{
	if (!cpu_is_primary(arch_proc_id()))
		return;

	if (state == PM_STATE_SOFT_OFF) {
#ifdef CONFIG_ADSP_IMR_CONTEXT_SAVE
		size_t storage_buffer_size;

		/* allocate IMR global_imr_ram_storage */
		const struct device *tlb_dev = DEVICE_DT_GET(DT_NODELABEL(tlb));

		__ASSERT_NO_MSG(tlb_dev);
		const struct intel_adsp_tlb_api *tlb_api =
				(struct intel_adsp_tlb_api *)tlb_dev->api;

		/* get HPSRAM storage buffer size */
		storage_buffer_size = tlb_api->get_storage_size();

		/* add space for LPSRAM */
		storage_buffer_size += LP_SRAM_SIZE;

		/* allocate IMR buffer and store it in the global pointer */
		global_imr_ram_storage = rballoc_align(0, SOF_MEM_CAPS_L3,
						       storage_buffer_size,
						       PLATFORM_DCACHE_ALIGN);

		/* If no IMR buffer we can not recover */
		if (!global_imr_ram_storage) {
			tr_err(&zephyr_tr, "failed to allocate global_imr_ram_storage");
			k_panic();
		}

		/* Suspend all DAI components before entering D3 state. */
		suspend_dais();
#endif /* CONFIG_ADSP_IMR_CONTEXT_SAVE */
	}
}
#endif /* CONFIG_PM */

/* notifier called after every power state transition */
void cpu_notify_state_exit(enum pm_state state)
{
	if (state == PM_STATE_SOFT_OFF)	{
#if CONFIG_MULTICORE
		if (!cpu_is_primary(arch_proc_id())) {
			/* Notifying primary core that secondary core successfully exit the D3
			 * state and is back in the Idle thread.
			 */
			return;
		}
#endif

#ifdef CONFIG_ADSP_IMR_CONTEXT_SAVE
		/* Resume all DAI components after exiting D3 state. */
		resume_dais();
		/* free global_imr_ram_storage */
		rfree(global_imr_ram_storage);
		global_imr_ram_storage = NULL;

		/* send FW Ready message */
		platform_boot_complete(0);
#endif
	}
}

int cpu_enable_core(int id)
{
	/* only called from single core, no RMW lock */
	__ASSERT_NO_MSG(cpu_is_primary(arch_proc_id()));
	/*
	 * This is an open-coded version of zephyr/kernel/smp.c
	 * z_smp_start_cpu(). We do this, so we can use a customized
	 * secondary_init() for SOF.
	 */

	if (arch_cpu_active(id))
		return 0;

#if CONFIG_PM
	/* During kernel initialization, the next pm state is set to ACTIVE. By checking this
	 * value, we determine if this is the first core boot, if not, we need to skip idle thread
	 * initialization. By reinitializing the idle thread, we would overwrite the kernel structs
	 * and the idle thread stack.
	 */
	if (pm_state_next_get(id)->state == PM_STATE_ACTIVE) {
		k_smp_cpu_start(id, secondary_init, NULL);
		return 0;
	}

	k_smp_cpu_resume(id, secondary_init, NULL, true, false);
#endif /* CONFIG_PM */

	return 0;
}

void cpu_disable_core(int id)
{
	/* only called from single core, no RMW lock */
	__ASSERT_NO_MSG(cpu_is_primary(arch_proc_id()));

	if (!arch_cpu_active(id)) {
		tr_warn(&zephyr_tr, "core %d is already disabled", id);
		return;
	}

#if defined(CONFIG_PM)
	/* TODO: before requesting core shut down check if it's not actively used */
	if (!pm_state_force(id, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0})) {
		tr_err(&zephyr_tr, "failed to set PM_STATE_SOFT_OFF on core %d", id);
		return;
	}

	if (cpu_is_primary(id)) {
		cpu_notify_state_entry(PM_STATE_SOFT_OFF);
#if defined(CONFIG_POWEROFF)
		/* Primary core will be turned off by the host. This does not return. */
		sys_poweroff();
#elif defined(CONFIG_HIBERNATE)
		/*
		 * Primary core will be turned off by the host. This function returns during
		 * context restore.
		 */
		sys_hibernate();
		return;
#endif
	}


	/* Broadcasting interrupts to other cores. */
	arch_sched_broadcast_ipi();

	uint64_t timeout = k_cycle_get_64() +
		k_ms_to_cyc_ceil64(CONFIG_SECONDARY_CORE_DISABLING_TIMEOUT);

	/* Waiting for secondary core to enter idle state */
	while (arch_cpu_active(id) && (k_cycle_get_64() < timeout))
		k_busy_wait(1);

	if (arch_cpu_active(id)) {
		tr_err(&zephyr_tr, "core %d did not enter idle state", id);
		return;
	}

	if (soc_adsp_halt_cpu(id) != 0)
		tr_err(&zephyr_tr, "failed to disable core %d", id);
#endif /* CONFIG_PM */
}

int cpu_is_core_enabled(int id)
{
	return arch_cpu_active(id);
}

int cpu_enabled_cores(void)
{
	unsigned int i;
	int mask = 0;

	for (i = 0; i < CONFIG_MP_MAX_NUM_CPUS; i++)
		if (arch_cpu_active(i))
			mask |= BIT(i);

	return mask;
}
#else
static int w_core_enable_mask = 0x1; /*Core 0 is always active*/

int cpu_enable_core(int id)
{
	pm_runtime_get(PM_RUNTIME_DSP, PWRD_BY_TPLG | id);

	/* only called from single core, no RMW lock */
	__ASSERT_NO_MSG(cpu_get_id() == PLATFORM_PRIMARY_CORE_ID);

	w_core_enable_mask |= BIT(id);

	return 0;
}

int cpu_enable_secondary_core(int id)
{
	if (arch_cpu_active(id))
		return 0;

	/* During kernel initialization, the next pm state is set to ACTIVE. By checking this
	 * value, we determine if this is the first core boot, if not, we need to skip idle thread
	 * initialization. By reinitializing the idle thread, we would overwrite the kernel structs
	 * and the idle thread stack.
	 */
	if (pm_state_next_get(id)->state == PM_STATE_ACTIVE) {
		k_smp_cpu_start(id, secondary_init, NULL);
		return 0;
	}

	k_smp_cpu_resume(id, secondary_init, NULL, true, false);

	return 0;
}

void cpu_disable_core(int id)
{
	/* TODO: call Zephyr API */

	/* only called from single core, no RMW lock */
	__ASSERT_NO_MSG(cpu_get_id() == PLATFORM_PRIMARY_CORE_ID);

	w_core_enable_mask &= ~BIT(id);
}

int cpu_is_core_enabled(int id)
{
	return w_core_enable_mask & BIT(id);
}

int cpu_enabled_cores(void)
{
	return w_core_enable_mask;
}
#endif /* CONFIG_ZEPHYR_NATIVE_DRIVERS */

void cpu_power_down_core(uint32_t flags)
{
	/* TODO: use Zephyr version */
}

int cpu_restore_secondary_cores(void)
{
	/* TODO: use Zephyr API */
	return 0;
}

int cpu_secondary_cores_prepare_d0ix(void)
{
	/* TODO: use Zephyr API */
	return 0;
}

#endif /* CONFIG_MULTICORE && CONFIG_SMP */
