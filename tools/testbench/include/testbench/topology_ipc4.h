/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef _TESTBENCH_TOPOLOGY_IPC4_H
#define _TESTBENCH_TOPOLOGY_IPC4_H

#include <module/ipc4/base-config.h>
#include "testbench/utils.h"

#define TB_IPC4_MAX_TPLG_OBJECT_SIZE	4096

/* See module_set_large_config() where message fragment is
 * MAILBOX_DSPBOX_SIZE. The add of header size (8) is because
 * testbench and plugin have the set large config header in
 * same memory as the payload.
 */
#define TB_IPC4_MAX_MSG_SIZE	(MAILBOX_DSPBOX_SIZE + sizeof(struct ipc4_module_large_config))

#define TB_MIXIN_MODULE_ID		0x2
#define TB_MIXOUT_MODULE_ID		0x3
#define TB_PGA_MODULE_ID		0x6
#define TB_SRC_MODULE_ID		0x7
#define TB_ASRC_MODULE_ID		0x8
#define TB_PROCESS_MODULE_ID		0x95
#define TB_FILE_OUT_AIF_MODULE_ID	0x9a
#define TB_FILE_IN_AIF_MODULE_ID	0x9b
#define TB_FILE_OUT_DAI_MODULE_ID	0x9c
#define TB_FILE_IN_DAI_MODULE_ID	0x9d

enum tb_pin_type {
	TB_PIN_TYPE_INPUT = 0,
	TB_PIN_TYPE_OUTPUT,
};

int tb_delete_pipeline(struct testbench_prm *tp, struct tplg_pipeline_info *pipe_info);
int tb_free_all_pipelines(struct testbench_prm *tp);
int tb_free_route(struct testbench_prm *tp, struct tplg_route_info *route_info);
int tb_get_instance_id_from_pipeline_id(struct testbench_prm *tp, int id);
int tb_is_single_format(struct sof_ipc4_pin_format *fmts, int num_formats);
int tb_match_audio_format(struct testbench_prm *tp, struct tplg_comp_info *comp_info,
			  struct tb_config *config);
int tb_new_aif_in_out(struct testbench_prm *tp, int dir);
int tb_new_dai_in_out(struct testbench_prm *tp, int dir);
int tb_new_pga(struct testbench_prm *tp);
int tb_new_process(struct testbench_prm *tp);
int tb_pipelines_set_state(struct testbench_prm *tp, int state, int dir);
int tb_send_bytes_data(struct tb_mq_desc *ipc_tx, struct tb_mq_desc *ipc_rx,
		       uint32_t module_id, uint32_t instance_id, struct sof_abi_hdr *abi);
int tb_send_volume_control(struct tb_mq_desc *ipc_tx, struct tb_mq_desc *ipc_rx,
			   struct tb_ctl *ctl, int *control_values, int num_values);
int tb_send_alsa_control(struct tb_mq_desc *ipc_tx, struct tb_mq_desc *ipc_rx, struct tb_ctl *ctl,
			 int *control_values, int num_values, int param_id);
int tb_set_reset_state(struct testbench_prm *tp);
int tb_set_running_state(struct testbench_prm *tp);
int tb_set_up_pipeline(struct testbench_prm *tp, struct tplg_pipeline_info *pipe_info);
int tb_set_up_route(struct testbench_prm *tp, struct tplg_route_info *route_info);
int tb_set_up_widget_base_config(struct testbench_prm *tp,
				 struct tplg_comp_info *comp_info);
int tb_set_up_widget_ipc(struct testbench_prm *tp, struct tplg_comp_info *comp_info);
void tb_free_topology(struct testbench_prm *tp);
void tb_pipeline_update_resource_usage(struct testbench_prm *tp,
				       struct tplg_comp_info *comp_info);

#endif /* _TESTBENCH_TOPOLOGY_IPC4_H */
