/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef _TESTBENCH_TOPOLOGY_IPC4_H
#define _TESTBENCH_TOPOLOGY_IPC4_H

#include <module/ipc4/base-config.h>
#include "testbench/common_test.h"

#define TB_FAKE_IPC	0

int tb_parse_ipc4_comp_tokens(struct testbench_prm *tp, struct ipc4_base_module_cfg *base_cfg);

void tb_setup_widget_ipc_msg(struct tplg_comp_info *comp_info);

int tb_set_up_widget_ipc(struct testbench_prm *tb, struct tplg_comp_info *comp_info);

int tb_set_up_route(struct testbench_prm *tb, struct tplg_route_info *route_info);

int tb_set_up_pipeline(struct testbench_prm *tb, struct tplg_pipeline_info *pipe_info);

void tb_pipeline_update_resource_usage(struct testbench_prm *tb,
					      struct tplg_comp_info *comp_info);

int tb_is_single_format(struct sof_ipc4_pin_format *fmts, int num_formats);

int tb_match_audio_format(struct testbench_prm *tb, struct tplg_comp_info *comp_info,
				 struct tb_config *config);

int tb_set_up_widget_base_config(struct testbench_prm *tb, struct tplg_comp_info *comp_info);

int tb_pipelines_set_state(struct testbench_prm *tb, int state, int dir);

int tb_delete_pipeline(struct testbench_prm *tb, struct tplg_pipeline_info *pipe_info);

int tb_free_route(struct testbench_prm *tb, struct tplg_route_info *route_info);

int tb_set_running_state(struct testbench_prm *tb);

int tb_set_reset_state(struct testbench_prm *tb);

#endif /* _TESTBENCH_TOPOLOGY_IPC4_H */
