/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef _TESTBENCH_TOPOLOGY_IPC4_H
#define _TESTBENCH_TOPOLOGY_IPC4_H

#include <module/ipc4/base-config.h>
#include "testbench/common_test.h"

#define TB_IPC4_MAX_TPLG_OBJECT_SIZE	4096
#define TB_IPC4_MAX_MSG_SIZE		384

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
