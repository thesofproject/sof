// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 AMD. All rights reserved.
//
// Author: Basavaraj Hiregoudar<basavaraj.hiregoudar@amd.com>

#include <sof/common.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <platform/chip_registers.h>

const struct freq_table platform_cpu_freq[] = {
	{600000000, 600000 },
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
			      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

static int acp_reg_read_via_smn(uint32_t reg_offset,
				uint32_t size)
{
	uint32_t reg_value;
	uint32_t delay_cnt = 10000;
	uint32_t smn_client_base_addr = (reg_offset >> 10);
	uint32_t region_start_addr = (smn_client_base_addr << 10);
	uint32_t apertureid = ((reg_offset >> 20) & 0xFFF);
	acp_srbm_cycle_sts_t  acp_srbm_cycle_sts;

	/* Configuring the MP1 Aperture Id in SRB_client_config register */
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_CONFIG), apertureid);
	/* Configuring the base address of MP1 */
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_BASE_ADDR), smn_client_base_addr);
	/* dummy read to the to the address offset 0x3400 */
	/* Master config offset to access register outside of ACP */
	reg_value = (uint32_t)io_reg_read(PU_REGISTER_BASE +
	(ACP_MASTER_REG_ACCESS_ADDRESS + reg_offset - region_start_addr + ACP_FIRST_REG_OFFSET));
	if (reg_value)
		reg_value = 0;
	/* reading status register to check above read request is completed or not */
	acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
							       ACP_SRBM_CYCLE_STS);
	/* Waiting for status to be set to '0' */
	while (delay_cnt > 0) {
		if (!acp_srbm_cycle_sts.bits.srbm_clients_sts) {
			/* when status is '0' from above read the data from data register */
			return (uint32_t) io_reg_read(PU_REGISTER_BASE + ACP_SRBM_CLIENT_RDDATA);
		}
		acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
									ACP_SRBM_CYCLE_STS);
		delay_cnt--;
	}
	return -1;
}

static void acp_reg_write_via_smn(uint32_t reg_offset,
				  uint32_t value, uint32_t size)
{
	uint32_t	delay_cnt = 10000;
	uint32_t        smn_client_base_addr    = (reg_offset >> 10);
	uint32_t        region_start_addr = (smn_client_base_addr << 10);
	uint32_t	apertureid = ((reg_offset >> 20) & 0xFFF);
	acp_srbm_cycle_sts_t      acp_srbm_cycle_sts;

	/* Configuring the MP1 Aperture Id in SRB_client_config register */
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_CONFIG), apertureid);
	/* Configuring the base address of MP1 */
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_BASE_ADDR), smn_client_base_addr);
	/* write to the address offset 0x3400
	 *  Master config offset to access register outside of ACP
	 */
	io_reg_write((PU_REGISTER_BASE +
		(ACP_MASTER_REG_ACCESS_ADDRESS + reg_offset - region_start_addr + ACP_FIRST_REG_OFFSET)),
		value);
	/* reading status register to check above read request is completed or not */
	acp_srbm_cycle_sts =
		(acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE + ACP_SRBM_CYCLE_STS);
	/* Waiting for status to be set to '0' */
	while (delay_cnt > 0) {
		acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
								ACP_SRBM_CYCLE_STS);
		if (!acp_srbm_cycle_sts.bits.srbm_clients_sts)
			return;
		delay_cnt--;
	}
}

static void  get_response_from_smu(void)
{
	uint32_t ret_value;
	uint32_t count = 0;

	do {
		ret_value = acp_reg_read_via_smn(MP1_SMN_C2PMSG_93, sizeof(int));
		if (count > 0x007fffff)
			count = 0;
		count++;
	} while (ret_value == 0);
}


void acp_change_clock_notify(uint32_t	clock_freq)
{
	/* converting Hz to Mhz */
	clock_freq = (clock_freq / 1000000);
	/* Writing response zero to the response register
	 * so that it cab reset to '0' for further response
	 */
	acp_reg_write_via_smn(MP1_SMN_C2PMSG_93,
				0, sizeof(int));
	/* Writing clock frequency value in Mhz in the Argument register */
	acp_reg_write_via_smn(MP1_SMN_C2PMSG_85,
				clock_freq, sizeof(int));
	/* Writing Message to message register */
	/* Writing Aclk change message to message register */
	acp_reg_write_via_smn(MP1_SMN_C2PMSG_69,
				ACP_SMU_MSG_SET_ACLK, sizeof(int));
	/* Waiting for SMU response */
	get_response_from_smu();
}

void platform_clock_init(struct sof *sof)
{
	int i = 0;

	sof->clocks = platform_clocks_info;
	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info) {
			.freqs_num = NUM_CPU_FREQ,
			.freqs = platform_cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = NULL,
		};
		k_spinlock_init(&sof->clocks[i].lock);
	}
}
