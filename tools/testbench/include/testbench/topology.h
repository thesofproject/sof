/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef __TESTBENCH_TOPOLOGY_H_
#define __TESTBENCH_TOPOLOGY_H_

#include "testbench/common_test.h"

#define MAX_TPLG_OBJECT_SIZE	4096
#define IPC3_MAX_MSG_SIZE	384
#define IPC4_MAX_MSG_SIZE	384

int tb_new_aif_in_out(struct testbench_prm *tb, int dir);

int tb_new_dai_in_out(struct testbench_prm *tb, int dir);

int tb_new_pga(struct testbench_prm *tb);

int tb_new_process(struct testbench_prm *tb);

int tb_free_pipelines(struct testbench_prm *tb, int dir);

void tb_free_topology(struct testbench_prm *tb);

int tb_get_instance_id_from_pipeline_id(struct testbench_prm *tp, int id);

int tb_set_up_pipelines(struct testbench_prm *tb, int dir);

int tb_free_all_pipelines(struct testbench_prm *tb);

#endif /*  __TESTBENCH_TOPOLOGY_H_ */
