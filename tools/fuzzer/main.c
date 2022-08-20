// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include "fuzzer.h"
#include <ipc/topology.h>
#include <ipc/stream.h>
#include <ipc/control.h>
#include "qemu-bridge.h"
#include <ipc/trace.h>
#include <tplg_parser/topology.h>

int enable_fuzzer;

int ipc_reply_recd;
pthread_cond_t ipc_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t ipc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/* tplg message types */
uint32_t tplg_cmd_types[] = {SOF_IPC_TPLG_COMP_NEW,
			     SOF_IPC_TPLG_COMP_FREE,
			     SOF_IPC_TPLG_COMP_CONNECT,
			     SOF_IPC_TPLG_PIPE_NEW,
			     SOF_IPC_TPLG_PIPE_FREE,
			     SOF_IPC_TPLG_PIPE_CONNECT,
			     SOF_IPC_TPLG_PIPE_COMPLETE,
			     SOF_IPC_TPLG_BUFFER_NEW,
			     SOF_IPC_TPLG_BUFFER_FREE};

/* PM message types */
uint32_t pm_cmd_types[] = {SOF_IPC_PM_CTX_SAVE,
			   SOF_IPC_PM_CTX_RESTORE,
			   SOF_IPC_PM_CTX_SIZE,
			   SOF_IPC_PM_CLK_SET,
			   SOF_IPC_PM_CLK_GET,
			   SOF_IPC_PM_CLK_REQ,
			   SOF_IPC_PM_CORE_ENABLE};

uint32_t comp_cmd_types[] = {SOF_IPC_COMP_SET_VALUE,
			     SOF_IPC_COMP_GET_VALUE,
			     SOF_IPC_COMP_SET_DATA,
			     SOF_IPC_COMP_GET_DATA};

uint32_t dai_cmd_types[] = {SOF_IPC_DAI_CONFIG, SOF_IPC_DAI_LOOPBACK};

uint32_t stream_cmd_types[] = {SOF_IPC_STREAM_PCM_PARAMS,
			       SOF_IPC_STREAM_PCM_PARAMS_REPLY,
			       SOF_IPC_STREAM_PCM_FREE,
			       SOF_IPC_STREAM_TRIG_START,
			       SOF_IPC_STREAM_TRIG_STOP,
			       SOF_IPC_STREAM_TRIG_PAUSE,
			       SOF_IPC_STREAM_TRIG_RELEASE,
			       SOF_IPC_STREAM_TRIG_DRAIN,
			       SOF_IPC_STREAM_TRIG_XRUN,
			       SOF_IPC_STREAM_POSITION,
			       SOF_IPC_STREAM_VORBIS_PARAMS,
			       SOF_IPC_STREAM_VORBIS_FREE};

uint32_t trace_cmd_types[] = {SOF_IPC_TRACE_DMA_PARAMS,
			      SOF_IPC_TRACE_DMA_POSITION};

/* list of supported target platforms */
static struct fuzz_platform *platform[] = {
		&byt_platform,
		&cht_platform,
		&bsw_platform,
		&hsw_platform,
		&bdw_platform,
		&imx8_platform,
};

static void usage(char *name)
{
	int i;

	fprintf(stdout, "Usage %s -p platform <option(s)>\n", name);
	fprintf(stdout, "		-t topology file\n");
	fprintf(stdout, "		-p platform name\n");
	fprintf(stdout, "		supported platforms: ");
	for (i = 0; i < ARRAY_SIZE(platform); i++)
		fprintf(stdout, "%s ", platform[i]->name);
	fprintf(stdout, "\n");
	fprintf(stdout, "Qemu must be started before the fuzzer is run.\n");

	exit(0);
}

static void ipc_dump(struct ipc_msg *msg)
{
	fprintf(stdout, "ipc: header 0x%x size %d reply %d\n",
		msg->header, msg->msg_size, msg->reply_size);
}

static void ipc_dump_err(struct ipc_msg *msg)
{
	/* TODO: dump IPC payload */
	fprintf(stderr, "ipc: header 0x%x size %d reply %d\n",
		msg->header, msg->msg_size, msg->reply_size);
}

void *fuzzer_create_io_region(struct fuzz *fuzzer, int id, int idx)
{
	struct fuzz_platform *plat = fuzzer->platform;
	struct fuzzer_reg_space *space;
	char shm_name[32];
	int err;
	void *ptr = NULL;

	space = &plat->reg_region[idx];

	sprintf(shm_name, "%s-io", space->name);
	fprintf(stdout, "registering %s\n", shm_name);
	err = qemu_io_register_shm(shm_name, id, space->desc.size, &ptr);
	if (err < 0)
		fprintf(stderr, "error: can't allocate IO %s:%d SHM %d\n", shm_name,
			id, err);

	return ptr;
}

void *fuzzer_create_memory_region(struct fuzz *fuzzer, int id, int idx)
{
	struct fuzz_platform *plat = fuzzer->platform;
	struct fuzzer_mem_desc *desc;
	char shm_name[32];
	void *ptr = NULL;
	int err;

	desc = &plat->mem_region[idx];

	/* shared via SHM (not shared on real HW) */
	sprintf(shm_name, "%s-mem", desc->name);
	fprintf(stdout, "registering %s\n", shm_name);
	err = qemu_io_register_shm(shm_name, id, desc->size, &ptr);
	if (err < 0)
		fprintf(stderr, "error: can't allocate %s:%d SHM %d\n", shm_name,
			id, err);

	return ptr;
}

/* frees all SHM and message queues */
void fuzzer_free_regions(struct fuzz *fuzzer)
{
	struct fuzz_platform *plat = fuzzer->platform;
	int i;

	for (i = 0; i < plat->num_mem_regions; i++)
		qemu_io_free_shm(i);

	for (i = 0; i < plat->num_reg_regions; i++)
		qemu_io_free_shm(i);

	qemu_io_free();
}

/* called by platform when it receives IPC message */
void fuzzer_ipc_msg_rx(struct fuzz *fuzzer, struct mailbox *mailbox)
{
	struct sof_ipc_comp_reply r;
	struct sof_ipc_cmd_hdr hdr;
	uint32_t cmd;

	/* read mailbox */
	fuzzer_mailbox_read(fuzzer, mailbox, 0, &hdr, sizeof(hdr));
	cmd = hdr.cmd & SOF_GLB_TYPE_MASK;

	/* check message type */
	switch (cmd) {
	case SOF_IPC_GLB_REPLY:
		fprintf(stderr, "error: ipc reply unknown\n");
		break;
	case SOF_IPC_FW_READY:
		fuzzer_fw_ready(fuzzer);
		fuzzer->boot_complete = 1;
		break;
	case SOF_IPC_GLB_COMPOUND:
	case SOF_IPC_GLB_TPLG_MSG:
	case SOF_IPC_GLB_PM_MSG:
	case SOF_IPC_GLB_COMP_MSG:
	case SOF_IPC_GLB_STREAM_MSG:
	case SOF_IPC_GLB_TRACE_MSG:
		fuzzer_mailbox_read(fuzzer, mailbox, 0, &r, sizeof(r));
		break;
	default:
		fprintf(stderr, "error: unknown DSP message 0x%x\n", cmd);
		break;
	}

}

/* called by platform when it receives IPC message reply */
void fuzzer_ipc_msg_reply(struct fuzz *fuzzer, struct mailbox *mailbox)
{
	int ret;

	ret = fuzzer->platform->get_reply(fuzzer, &fuzzer->msg);
	if (ret < 0)
		fprintf(stderr, "error: incorrect DSP reply\n");

	ipc_dump(&fuzzer->msg);

	pthread_mutex_lock(&ipc_mutex);
	ipc_reply_recd = 1;
	pthread_cond_signal(&ipc_cond);
	pthread_mutex_unlock(&ipc_mutex);
}

/* called by platform when FW crashses */
void fuzzer_ipc_crash(struct fuzz *fuzzer, struct mailbox *mailbox,
		      unsigned int offset)
{
	/* TODO: DSP FW has crashed. dump stack, regs, last IPC, log etc */
	fprintf(stderr, "error: DSP FW crash\n");
	exit(EXIT_FAILURE);
}

int fuzzer_send_msg(struct fuzz *fuzzer)
{
	struct timespec timeout;
	struct timeval tp;
	int ret;

	ipc_dump(&fuzzer->msg);

	/* send msg */
	ret = fuzzer->platform->send_msg(fuzzer, &fuzzer->msg);
	if (ret < 0) {
		fprintf(stderr, "error: message tx failed\n");
		return ret;
	}

	/* wait for ipc reply */
	gettimeofday(&tp, NULL);
	timeout.tv_sec  = tp.tv_sec;
	timeout.tv_nsec = tp.tv_usec * 1000;
	timeout.tv_nsec += 300000000; /* 300ms timeout */

	/* first lock the boot wait mutex */
	pthread_mutex_lock(&ipc_mutex);

	/* reset condition for next IPC message */
	ipc_reply_recd = 0;
	ret = 0;

	/* now wait for mutex unlock and ipc_reply_recd set by IPC reply */
	while (!ipc_reply_recd && !ret)
		ret = pthread_cond_timedwait(&ipc_cond, &ipc_mutex, &timeout);

	if (ret == ETIMEDOUT && !ipc_reply_recd) {
		ret = -EINVAL;
		fprintf(stderr, "error: IPC timeout\n");
		ipc_dump_err(&fuzzer->msg);
		pthread_mutex_unlock(&ipc_mutex);
		exit(0);
	}

	pthread_mutex_unlock(&ipc_mutex);

	/*
	 * sleep for 5 ms before continuing sending the next message.
	 * This helps with the condition signaling. Without this,
	 * the condition seems to always satisfy and
	 * the fuzzer never waits for a response from the DSP.
	 */
	usleep(50000);

	return ret;
}

int main(int argc, char *argv[])
{
	struct fuzz fuzzer;
	int ret;
	char opt;
	char *topology_file = NULL;
	char *platform_name = NULL;
	int i;
	struct tplg_context ctx;

	/* parse arguments */
	while ((opt = getopt(argc, argv, "ht:p:")) != -1) {
		switch (opt) {
		case 't':
			topology_file = optarg;
			break;
		case 'p':
			platform_name = optarg;
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	/* initialise emulated target device */
	if (!platform_name) {
		fprintf(stderr, "error: no target platform specified\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* find platform */
	for (i = 0; i < ARRAY_SIZE(platform); i++) {
		if (!strcmp(platform[i]->name, platform_name))
			break;
	}

	/* no platform found */
	if (i >= ARRAY_SIZE(platform)) {
		fprintf(stderr, "error: platform %s not supported\n",
			platform_name);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* init plaform */
	fprintf(stdout, "initialising platform %s\n", platform[i]->name);
	ret = platform[i]->init(&fuzzer, platform[i]);
	if (ret == ETIMEDOUT) {
		fprintf(stderr, "error: platform %s failed to initialise\n",
			platform_name);
		exit(EXIT_FAILURE);
	}

	fprintf(stdout, "FW boot complete\n");

	/* allocate max ipc size bytes for the msg and reply */
	fuzzer.msg.msg_data = malloc(SOF_IPC_MSG_MAX_SIZE);
	fuzzer.msg.reply_data = malloc(SOF_IPC_MSG_MAX_SIZE);
	ctx.fuzzer = &fuzzer;
	ctx.tplg_file = topology_file;

	/* load topology */
	ret = fuzzer_parse_topology(&ctx);
	if (ret < 0)
		exit(EXIT_FAILURE);

	/* all done - now free platform */
	platform[i]->free(&fuzzer);
	return 0;
}
