/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_PLUGIN_COMMON_H__
#define __SOF_PLUGIN_COMMON_H__

#include <stdint.h>
#include <mqueue.h>
#include <semaphore.h>
#include <alsa/asoundlib.h>

/* temporary - current MAXLEN is not define in UAPI header - fix pending */
#ifndef SNDRV_CTL_ELEM_ID_NAME_MAXLEN
#define SNDRV_CTL_ELEM_ID_NAME_MAXLEN	44
#endif

#include <alsa/sound/uapi/asoc.h>

#define IPC3_MAX_MSG_SIZE	384
#define NAME_SIZE	256

#define MAX_CTLS	256

#define MS_TO_US(_msus)	(_msus * 1000)
#define MS_TO_NS(_msns) (MS_TO_US(_msns * 1000))

#define MS_TO_US(_msus)	(_msus * 1000)
#define MS_TO_NS(_msns) (MS_TO_US(_msns * 1000))

#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

#define SHM_SIZE	(4096 * 64)	/* get from topology - and set for context */

#define NUM_EP_CONFIGS		8

/*
 * Run with valgrind
 * valgrind --trace-children=yes aplay -v -Dsof:blah.tplg,1,hw:1,2  -f dat /dev/zero
 */
//#define VALGRIND
#ifdef VALGRIND
#define DEBUG_TV_SECS	10
#define DEBUG_RETRIES	1000
#else
#define DEBUG_RETRIES	10
#define DEBUG_TV_SECS	0
#endif

#define SOF_MAGIC	"sofpipe"

enum plugin_state {
	SOF_PLUGIN_STATE_INIT	= 0,
	SOF_PLUGIN_STATE_READY	= 1,
	SOF_PLUGIN_STATE_DEAD	= 2,
	SOF_PLUGIN_STATE_STREAM_RUNNING	= 3,
	SOF_PLUGIN_STATE_STREAM_ERROR	= 5,
};

struct plug_shm_ctl {
	unsigned int comp_id;
	unsigned int type;
	union {
		struct snd_soc_tplg_mixer_control mixer_ctl;
		struct snd_soc_tplg_enum_control enum_ctl;
		struct snd_soc_tplg_bytes_control bytes_ctl;
	};
};

/*
 *	config.48k2c {
 *		rate 48000
 *		channels 2
 *		period_time 0
 *		period_frames 6000
 *		buffer_time 0
 *		buffer_frames 24000
 *     }
 */
struct plug_config {
	char name[44];
	unsigned long buffer_frames;
	unsigned long buffer_time;
	unsigned long period_frames;
	unsigned long period_time;
	int rate;
	int channels;
	unsigned long format;
};

/*
 * :[pcm:card:dev:config[pcm:card:dev:config]...]
 */
struct plug_cmdline_item {
	int pcm;
	char card_name[44];
	char dev_name[44];
	char config_name[44];
};

/*
 * Endpoint pipeline configuration
 */
struct endpoint_hw_config {
	int pipeline;
	char card_name[44];
	char dev_name[44];
	char config_name[44];
	unsigned long buffer_frames;
	unsigned long buffer_time;
	unsigned long period_frames;
	unsigned long period_time;
	int rate;
	int channels;
	unsigned long format;
};

struct plug_shm_endpoint {
	char magic[8];			/* SOF_MAGIC */
	uint64_t state;
	uint32_t pipeline_id;
	uint32_t comp_id;
	uint32_t idx;
	unsigned long rpos;	/* current position in ring buffer */
	unsigned long rwrap;
	unsigned long wpos;	/* current position in ring buffer */
	unsigned long wwrap;
	unsigned long buffer_size;		/* buffer size */
	unsigned long wtotal;		/* total frames copied */
	unsigned long rtotal;		/* total frames copied */
	int frame_size;
	char data[0];		// TODO: align this on SIMD/cache
};

struct plug_shm_glb_state {
	char magic[8];			/* SOF_MAGIC */
	uint64_t size;			/* size of this structure in bytes */
	uint64_t state;			/* enum plugin_state */
	struct endpoint_hw_config ep_config[NUM_EP_CONFIGS];
	int num_ep_configs;
	uint64_t num_ctls;		/* number of ctls */
	struct plug_shm_ctl ctl[];
};

struct plug_shm_desc {
	/* SHM for stream context sync */
	int fd;
	int size;
	char name[NAME_SIZE];
	void *addr;
};

struct plug_mq_desc {
	/* IPC message queue */
	mqd_t mq;
	struct mq_attr attr;
	char queue_name[NAME_SIZE];
};

struct plug_sem_desc {
	char name[NAME_SIZE];
	sem_t *sem;
};

struct plug_ctl_container {
	struct snd_soc_tplg_ctl_hdr *tplg[MAX_CTLS];
	int updated[MAX_CTLS];
	int count;
};

static inline void *plug_ep_rptr(struct plug_shm_endpoint *ep)
{
	return ep->data + ep->rpos;
}

static inline void *plug_ep_wptr(struct plug_shm_endpoint *ep)
{
	return ep->data + ep->wpos;
}

static inline int plug_ep_wrap_rsize(struct plug_shm_endpoint *ep)
{
	return ep->buffer_size - ep->rpos;
}

static inline int plug_ep_wrap_wsize(struct plug_shm_endpoint *ep)
{
	return ep->buffer_size - ep->wpos;
}

static inline int plug_ep_get_free(struct plug_shm_endpoint *ep)
{
	if (ep->rwrap == ep->wwrap) {
		/* calculate available bytes */
		if (ep->rpos < ep->wpos)
			return ep->buffer_size - (ep->wpos - ep->rpos);
		else
			return ep->buffer_size;
	} else {
		return ep->rpos - ep->wpos;
	}
}

static inline int plug_ep_get_avail(struct plug_shm_endpoint *ep)
{
	if (ep->rwrap == ep->wwrap) {
		/* calculate available bytes */
		if (ep->rpos < ep->wpos)
			return ep->wpos - ep->rpos;
		else
			return 0;
	} else {
		return (ep->buffer_size - ep->rpos) + ep->wpos;
	}
}

static inline void *plug_ep_consume(struct plug_shm_endpoint *ep, unsigned int bytes)
{
	ep->rtotal += bytes;
	ep->rpos += bytes;

	if (ep->rpos >= ep->buffer_size) {
		ep->rpos -= ep->buffer_size;
		ep->rwrap++;
	}

	return ep->data + ep->rpos;
}

static inline void *plug_ep_produce(struct plug_shm_endpoint *ep, unsigned int bytes)
{
	ep->wtotal += bytes;
	ep->wpos += bytes;

	if (ep->wpos >= ep->buffer_size) {
		ep->wpos -= ep->buffer_size;
		ep->wwrap++;
	}

	return ep->data + ep->wpos;
}

/*
 * SHM
 */
int plug_shm_init(struct plug_shm_desc *shm, const char *tplg, const char *type, int index);

int plug_shm_create(struct plug_shm_desc *shm);

int plug_shm_open(struct plug_shm_desc *shm);

void plug_shm_free(struct plug_shm_desc *shm);

/*
 * IPC
 */
int plug_mq_create(struct plug_mq_desc *ipc);

int plug_mq_open(struct plug_mq_desc *ipc);

int plug_mq_init(struct plug_mq_desc *ipc, const char *tplg, const char *type, int index);

void plug_mq_free(struct plug_mq_desc *ipc);

int plug_mq_cmd(struct plug_mq_desc *ipc, void *msg, size_t len, void *reply, size_t rlen);

int plug_mq_cmd_tx_rx(struct plug_mq_desc *ipc_tx, struct plug_mq_desc *ipc_rx,
		      void *msg, size_t len, void *reply, size_t rlen);

/*
 * Locking
 */
int plug_lock_create(struct plug_sem_desc *lock);

void plug_lock_free(struct plug_sem_desc *lock);

int plug_lock_init(struct plug_sem_desc *lock, const char *tplg, const char *type, int index);

int plug_lock_open(struct plug_sem_desc *lock);

/*
 * Timing.
 */
void plug_timespec_add_ms(struct timespec *ts, unsigned long ms);

long plug_timespec_delta_ns(struct timespec *before, struct timespec *after);

/* dump the IPC data - dont print lines of 0s */
static inline void data_dump(void *vdata, size_t bytes)
{
	uint32_t *data = vdata;
	size_t words = bytes >> 2;
	int i;

	for (i = 0; i < words; i++) {
		/* 4 words per line */
		if (i % 4 == 0) {
			/* delete lines with all 0s */
			if (i > 0 && data[i - 3] == 0 && data[i - 2] == 0 &&
			    data[i - 1] == 0 && data[i - 0] == 0)
				printf("\r");
			else
				printf("\n");

			printf("0x%4.4x: 0x%8.8x", i, data[i]);
		} else {
			printf(" 0x%8.8x", data[i]);
		}
	}
	printf("\n");
}

#endif
