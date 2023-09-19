// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>

// TODO remove parsing and read ctls from sof-pipe SHM glb context
#include <ipc/control.h>

#include <alsa/asoundlib.h>
#include <alsa/control_external.h>

#include "plugin.h"
#include "common.h"

typedef struct snd_sof_ctl {
	struct plug_shm_glb_state *glb;
	snd_ctl_ext_t ext;
	struct plug_mq_desc ipc;
	struct plug_shm_desc shm_ctx;
	int subscribed;
	int updated[MAX_CTLS];

} snd_sof_ctl_t;

#define CTL_GET_TPLG_HDR(_ctl, _key) \
		(&_ctl->glb->ctl[_key].mixer_ctl.hdr)

#define CTL_GET_TPLG_MIXER(_ctl, _key) \
		(&_ctl->glb->ctl[_key].mixer_ctl)

#define CTL_GET_TPLG_ENUM(_ctl, _key) \
		(&_ctl->glb->ctl[_key].enum_ctl)

#define CTL_GET_TPLG_BYTES(_ctl, _key) \
		(&_ctl->glb->ctl[_key].bytes_ctl)

/* number of ctls */
static int plug_ctl_elem_count(snd_ctl_ext_t *ext)
{
	snd_sof_ctl_t *ctl = ext->private_data;

	/* TODO: get count of elems from topology */
	return ctl->glb->num_ctls;
}

static int plug_ctl_elem_list(snd_ctl_ext_t *ext, unsigned int offset, snd_ctl_elem_id_t *id)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_ctl_hdr *hdr;

	if (offset >=  ctl->glb->num_ctls)
		return -EINVAL;

	hdr = CTL_GET_TPLG_HDR(ctl, offset);

	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_id_set_name(id, hdr->name);

	return 0;
}

static snd_ctl_ext_key_t plug_ctl_find_elem(snd_ctl_ext_t *ext, const snd_ctl_elem_id_t *id)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	unsigned int numid;

	numid = snd_ctl_elem_id_get_numid(id);

	if (numid > ctl->glb->num_ctls)
		return SND_CTL_EXT_KEY_NOT_FOUND;

	return numid - 1;
}

static int plug_ctl_get_attribute(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key,
				  int *type, unsigned int *acc, unsigned int *count)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_ctl_hdr *hdr = CTL_GET_TPLG_HDR(ctl, key);
	struct snd_soc_tplg_mixer_control *mixer_ctl;
	struct snd_soc_tplg_enum_control *enum_ctl;
	struct snd_soc_tplg_bytes_control *bytes_ctl;
	int err = 0;

	switch (hdr->type) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		mixer_ctl = (struct snd_soc_tplg_mixer_control *)hdr;

		/* check for type - boolean should be binary values */
		if (mixer_ctl->max == 1 && mixer_ctl->min == 0)
			*type = SND_CTL_ELEM_TYPE_BOOLEAN;
		else
			*type = SND_CTL_ELEM_TYPE_INTEGER;
		*count = 2;//mixer_ctl->num_channels; ///// WRONG is 0 !!!

		//printf("mixer %d %d\n", __LINE__, mixer_ctl->num_channels);
		break;
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
		enum_ctl = (struct snd_soc_tplg_enum_control *)hdr;
		*type = SND_CTL_ELEM_TYPE_ENUMERATED;
		*count = enum_ctl->num_channels;
		break;
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_CTL_STROBE:
		// TODO: ??
		break;
	case SND_SOC_TPLG_CTL_BYTES:
		printf("%s %d\n", __func__, __LINE__);
		bytes_ctl = (struct snd_soc_tplg_bytes_control *)hdr;
		*type = SND_CTL_ELEM_TYPE_BYTES;
		*count = bytes_ctl->size; // Not sure if size is correct
		break;
	}

	*acc = hdr->access;

	/* access needs the callback to decode the data */
	if ((hdr->access & SND_CTL_EXT_ACCESS_TLV_READ) ||
	    (hdr->access & SND_CTL_EXT_ACCESS_TLV_WRITE))
		*acc |= SND_CTL_EXT_ACCESS_TLV_CALLBACK;
	return err;
}

/*
 * Integer ops
 */
static int plug_ctl_get_integer_info(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key, long *imin,
				     long *imax, long *istep)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_ctl_hdr *hdr = CTL_GET_TPLG_HDR(ctl, key);
	struct snd_soc_tplg_mixer_control *mixer_ctl =
			CTL_GET_TPLG_MIXER(ctl, key);
	int err = 0;

	switch (hdr->type) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		/* TLV uses the fields differently */
		if ((hdr->access & SND_CTL_EXT_ACCESS_TLV_READ) ||
		    (hdr->access & SND_CTL_EXT_ACCESS_TLV_WRITE)) {
			*istep = mixer_ctl->hdr.tlv.scale.step;
			*imin = (int32_t)mixer_ctl->hdr.tlv.scale.min;
			*imax = mixer_ctl->max;
		} else {
			*istep = 1;
			*imin = mixer_ctl->min;
			*imax = mixer_ctl->max;
		}
		break;
	default:
		SNDERR("invalid ctl type for integer using key %d", key);
		err = -EINVAL;
		break;
	}

	return err;
}

static int plug_ctl_read_integer(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key, long *value)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_mixer_control *mixer_ctl =
		CTL_GET_TPLG_MIXER(ctl, key);
	struct sof_ipc_ctrl_data *ctl_data;
	struct sof_ipc_ctrl_value_chan *chanv;
	int err, i;

	// TODO: set generic max size
	ctl_data = calloc(IPC3_MAX_MSG_SIZE, 1);
	if (!ctl_data)
		return -ENOMEM;
	chanv = ctl_data->chanv;

	/* setup the IPC message */
	ctl_data->comp_id = ctl->glb->ctl[key].comp_id;
	ctl_data->cmd = SOF_CTRL_CMD_VOLUME;
	ctl_data->type = SOF_CTRL_TYPE_VALUE_COMP_GET;
	ctl_data->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_GET_VALUE;
	ctl_data->rhdr.hdr.size = sizeof(ctl_data);
	ctl_data->num_elems = mixer_ctl->num_channels;

	/* send message and wait for reply */
	err = plug_mq_cmd(&ctl->ipc, ctl_data, IPC3_MAX_MSG_SIZE, ctl_data, IPC3_MAX_MSG_SIZE);
	if (err < 0) {
		SNDERR("error: can't read CTL %d message\n",
		       ctl_data->comp_id);
		return err;
	}

	/* did IPC succeed ? */
	if (ctl_data->rhdr.error != 0) {
		SNDERR("error: can't read CTL %d failed: error %d\n",
		       ctl_data->comp_id, ctl_data->rhdr.error);
		return ctl_data->rhdr.error;
	}

	/* get data from IPC */
	for (i = 0; i < mixer_ctl->num_channels; i++)
		value[i] = chanv[i].value;

	free(ctl_data);

	return err;
}

static int plug_ctl_write_integer(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key, long *value)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_mixer_control *mixer_ctl =
			CTL_GET_TPLG_MIXER(ctl, key);
	struct sof_ipc_ctrl_data *ctl_data;
	struct sof_ipc_ctrl_value_chan *chanv;
	int err;
	int i;

	// TODO: set generic max size
	ctl_data = calloc(IPC3_MAX_MSG_SIZE, 1);
	if (!ctl_data)
		return -ENOMEM;
	chanv = ctl_data->chanv;

	/* setup the IPC message */
	ctl_data->comp_id = ctl->glb->ctl[key].comp_id;
	ctl_data->cmd = SOF_CTRL_CMD_VOLUME;
	ctl_data->type = SOF_CTRL_TYPE_VALUE_COMP_SET;
	ctl_data->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_SET_VALUE;
	ctl_data->rhdr.hdr.size = sizeof(ctl_data);
	ctl_data->num_elems = mixer_ctl->num_channels;

	/* set data for IPC */
	for (i = 0; i < mixer_ctl->num_channels; i++)
		chanv[i].value = value[i];

	err = plug_mq_cmd(&ctl->ipc, ctl_data, IPC3_MAX_MSG_SIZE,
			  ctl_data, IPC3_MAX_MSG_SIZE);
	if (err < 0) {
		SNDERR("error: can't write CTL %d message\n", ctl_data->comp_id);
		return err;
	}

	/* did IPC succeed ? */
	if (ctl_data->rhdr.error != 0) {
		SNDERR("error: can't write CTL %d failed: error %d\n",
		       ctl_data->comp_id, ctl_data->rhdr.error);
		return ctl_data->rhdr.error;
	}

	free(ctl_data);

	return err;
}

/*
 * Enum ops
 */
static int plug_ctl_get_enumerated_info(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key,
					unsigned int *items)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_ctl_hdr *hdr = CTL_GET_TPLG_HDR(ctl, key);
	struct snd_soc_tplg_enum_control *enum_ctl =
			(struct snd_soc_tplg_enum_control *)hdr;
	int err = 0;

	switch (hdr->type) {
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
		*items = enum_ctl->items;
		break;
	default:
		SNDERR("invalid ctl type for enum using key %d", key);
		err = -EINVAL;
		break;
	}

	return err;
}

static int plug_ctl_get_enumerated_name(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key,
					unsigned int item, char *name, size_t name_max_len)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_ctl_hdr *hdr = CTL_GET_TPLG_HDR(ctl, key);
	struct snd_soc_tplg_enum_control *enum_ctl =
			(struct snd_soc_tplg_enum_control *)hdr;

	printf("%s %d\n", __func__, __LINE__);

	if (item >= enum_ctl->count) {
		SNDERR("invalid item %d for enum using key %d", item, key);
		return -EINVAL;
	}

	strncpy(name, enum_ctl->texts[item], name_max_len);
	return 0;
}

static int plug_ctl_read_enumerated(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key,
				    unsigned int *items)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_enum_control *enum_ctl =
		CTL_GET_TPLG_ENUM(ctl, key);
	struct sof_ipc_ctrl_data *ctl_data;
	struct sof_ipc_ctrl_value_comp *compv;
	int err, i;

	// TODO: set generic max size
	ctl_data = calloc(IPC3_MAX_MSG_SIZE, 1);
	if (!ctl_data)
		return -ENOMEM;
	compv = ctl_data->compv;

	/* setup the IPC message */
	ctl_data->comp_id = ctl->glb->ctl[key].comp_id;
	ctl_data->cmd = SOF_CTRL_CMD_ENUM;
	ctl_data->type = SOF_CTRL_TYPE_VALUE_COMP_GET;
	ctl_data->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_GET_VALUE;
	ctl_data->rhdr.hdr.size = sizeof(ctl_data);

	/* send message and wait for reply */
	err = plug_mq_cmd(&ctl->ipc, ctl_data, IPC3_MAX_MSG_SIZE,
			  ctl_data, IPC3_MAX_MSG_SIZE);
	if (err < 0) {
		SNDERR("error: can't read CTL %d message\n", ctl_data->comp_id);
		return err;
	}

	/* did IPC succeed ? */
	if (ctl_data->rhdr.error != 0) {
		SNDERR("error: can't read CTL %d failed: error %d\n",
		       ctl_data->comp_id, ctl_data->rhdr.error);
		return ctl_data->rhdr.error;
	}

	/* get data from IPC */
	for (i = 0; i < enum_ctl->num_channels; i++)
		items[i] = compv[i].uvalue;

	free(ctl_data);

	return err;
}

static int plug_ctl_write_enumerated(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key,
				     unsigned int *items)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_enum_control *enum_ctl =
		CTL_GET_TPLG_ENUM(ctl, key);
	struct sof_ipc_ctrl_data *ctl_data;
	struct sof_ipc_ctrl_value_comp *compv;
	int err, i;

	// TODO: set generic max size
	ctl_data = calloc(IPC3_MAX_MSG_SIZE, 1);
	if (!ctl_data)
		return -ENOMEM;
	compv = ctl_data->compv;

	printf("%s %d\n", __func__, __LINE__);

	/* setup the IPC message */
	ctl_data->comp_id = ctl->glb->ctl[key].comp_id;
	ctl_data->cmd = SOF_CTRL_CMD_ENUM;
	ctl_data->type = SOF_CTRL_TYPE_VALUE_COMP_SET;
	ctl_data->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_SET_VALUE;
	ctl_data->rhdr.hdr.size = sizeof(ctl_data);

	/* set data for IPC */
	for (i = 0; i < enum_ctl->num_channels; i++)
		compv[i].uvalue = items[i];

	/* send message and wait for reply */
	err = plug_mq_cmd(&ctl->ipc, ctl_data, IPC3_MAX_MSG_SIZE,
			  ctl_data, IPC3_MAX_MSG_SIZE);
	if (err < 0) {
		SNDERR("error: can't read CTL %d message\n", ctl_data->comp_id);
		goto out;
	}

	/* did IPC succeed ? */
	if (ctl_data->rhdr.error != 0) {
		SNDERR("error: can't read CTL %d failed: error %d\n",
		       ctl_data->comp_id, ctl_data->rhdr.error);
		err = ctl_data->rhdr.error;
	}

out:
	free(ctl_data);
	return err;
}

/*
 * Bytes ops - TODO handle large blobs
 */

static int plug_ctl_read_bytes(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key,
			       unsigned char *data, size_t max_bytes)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_bytes_control *binary_ctl =
		CTL_GET_TPLG_BYTES(ctl, key);
	struct sof_ipc_ctrl_data *ctl_data;
	int err;

	// TODO: set generic max size
	ctl_data = calloc(IPC3_MAX_MSG_SIZE, 1);
	if (!ctl_data)
		return -ENOMEM;

	/* setup the IPC message */
	ctl_data->comp_id = ctl->glb->ctl[key].comp_id;
	ctl_data->cmd = SOF_CTRL_CMD_BINARY;
	ctl_data->type = SOF_CTRL_TYPE_VALUE_COMP_GET;
	ctl_data->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_GET_VALUE;
	ctl_data->rhdr.hdr.size = sizeof(ctl_data);

	/* send message and wait for reply */
	err = plug_mq_cmd(&ctl->ipc, ctl_data, IPC3_MAX_MSG_SIZE,
			  ctl_data, IPC3_MAX_MSG_SIZE);
	if (err < 0) {
		SNDERR("error: can't read CTL %d message\n", ctl_data->comp_id);
		return err;
	}

	/* did IPC succeed ? */
	if (ctl_data->rhdr.error != 0) {
		SNDERR("error: can't read CTL %d failed: error %d\n",
		       ctl_data->comp_id, ctl_data->rhdr.error);
		return ctl_data->rhdr.error;
	}

	/* get data for IPC */
	memcpy(data, &ctl_data->data, MIN(binary_ctl->max, max_bytes));
	free(ctl_data);

	return err;
}

static int plug_ctl_write_bytes(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key,
				unsigned char *data, size_t max_bytes)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_bytes_control *binary_ctl =
		CTL_GET_TPLG_BYTES(ctl, key);
	struct sof_ipc_ctrl_data *ctl_data;
	int err;

	// TODO: set generic max size
	ctl_data = calloc(IPC3_MAX_MSG_SIZE, 1);
	if (!ctl_data)
		return -ENOMEM;

	printf("%s %d\n", __func__, __LINE__);

	/* setup the IPC message */
	ctl_data->comp_id = ctl->glb->ctl[key].comp_id;
	ctl_data->cmd = SOF_CTRL_CMD_BINARY;
	ctl_data->type = SOF_CTRL_TYPE_VALUE_COMP_SET;
	ctl_data->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_SET_VALUE;
	ctl_data->rhdr.hdr.size = sizeof(ctl_data);

	/* set data for IPC */
	memcpy(&ctl_data->data, data, MIN(binary_ctl->max, max_bytes));

	/* send message and wait for reply */
	err = plug_mq_cmd(&ctl->ipc, ctl_data, IPC3_MAX_MSG_SIZE,
			  ctl_data, IPC3_MAX_MSG_SIZE);
	if (err < 0) {
		SNDERR("error: can't read CTL %d message\n", ctl_data->comp_id);
		return err;
	}

	/* did IPC succeed ? */
	if (ctl_data->rhdr.error != 0) {
		SNDERR("error: can't read CTL %d failed: error %d\n",
		       ctl_data->comp_id, ctl_data->rhdr.error);
		return ctl_data->rhdr.error;
	}

	free(ctl_data);

	return err;
}

/*
 * TLV ops
 *
 * The format of an array of \a tlv argument is:
 *   tlv[0]:   Type. One of SND_CTL_TLVT_XXX.
 *   tlv[1]:   Length. The length of value in units of byte.
 *   tlv[2..]: Value. Depending on the type.
 */
static int plug_tlv_rw(snd_ctl_ext_t *ext, snd_ctl_ext_key_t key, int op_flag,
		       unsigned int numid, unsigned int *tlv, unsigned int tlv_size)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	struct snd_soc_tplg_ctl_hdr *hdr = CTL_GET_TPLG_HDR(ctl, key);

	//TODO: alsamixer showing wrong dB scales
	tlv[0] = hdr->tlv.type;
	tlv[1] = hdr->tlv.size - sizeof(uint32_t) * 2;
	memcpy(&tlv[2], hdr->tlv.data, hdr->tlv.size - sizeof(uint32_t) * 2);

	return 0;
}

static void plug_ctl_subscribe_events(snd_ctl_ext_t *ext, int subscribe)
{
	snd_sof_ctl_t *ctl = ext->private_data;

	ctl->subscribed = !!(subscribe & SND_CTL_EVENT_MASK_VALUE);
}

static int plug_ctl_read_event(snd_ctl_ext_t *ext, snd_ctl_elem_id_t *id,
			       unsigned int *event_mask)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	int numid;
	int err = 0;

	numid = snd_ctl_elem_id_get_numid(id);

	// TODO: we need a notify() or listening thread to take async/volatile ctl
	// notifications from sof-pipe and notify userspace via events of the ctl change.
	if (!ctl->updated[numid - 1] || !ctl->subscribed) {
		err = -EAGAIN;
		goto out;
	}

	*event_mask = SND_CTL_EVENT_MASK_VALUE;
out:
	return err;
}

static int plug_ctl_poll_revents(snd_ctl_ext_t *ext, struct pollfd *pfd,
				 unsigned int nfds, unsigned short *revents)
{
	snd_sof_ctl_t *ctl = ext->private_data;
	int i;

	*revents = 0;

	for (i = 0; i < ctl->glb->num_ctls; i++) {
		if (ctl->updated[i]) {
			*revents = POLLIN;
			break;
		}
	}

	return 0;
}

static void plug_ctl_close(snd_ctl_ext_t *ext)
{
	snd_sof_ctl_t *ctl = ext->private_data;

	/* TODO: munmap */
	free(ctl);
}

static const snd_ctl_ext_callback_t sof_ext_callback = {
	.elem_count = plug_ctl_elem_count,
	.elem_list = plug_ctl_elem_list,
	.find_elem = plug_ctl_find_elem,
	.get_attribute = plug_ctl_get_attribute,
	.get_integer_info = plug_ctl_get_integer_info,
	.read_integer = plug_ctl_read_integer,
	.write_integer = plug_ctl_write_integer,
	.get_enumerated_info = plug_ctl_get_enumerated_info,
	.get_enumerated_name = plug_ctl_get_enumerated_name,
	.read_enumerated = plug_ctl_read_enumerated,
	.write_enumerated = plug_ctl_write_enumerated,
	.read_bytes = plug_ctl_read_bytes,
	.write_bytes = plug_ctl_write_bytes,
	.subscribe_events = plug_ctl_subscribe_events,
	.read_event = plug_ctl_read_event,
	.poll_revents = plug_ctl_poll_revents,
	.close = plug_ctl_close,
};

SND_CTL_PLUGIN_DEFINE_FUNC(sof)
{
	snd_sof_plug_t *plug;
	int err;
	snd_sof_ctl_t *ctl;

	/* create context */
	plug = calloc(1, sizeof(*plug));
	if (!plug)
		return -ENOMEM;

	ctl = calloc(1, sizeof(*ctl));
	if (!ctl)
		return -ENOMEM;
	plug->module_prv = ctl;

	/* parse the ALSA configuration file for sof plugin */
	err = plug_parse_conf(plug, name, root, conf);
	if (err < 0) {
		SNDERR("failed to parse config: %s", strerror(err));
		goto error;
	}

	/* create message queue for IPC */
	err = plug_mq_init(&ctl->ipc, plug->tplg_file, "ipc", 0);
	if (err < 0)
		goto error;

	/* open message queue for IPC */
	err = plug_mq_open(&ctl->ipc);
	if (err < 0) {
		SNDERR("failed to open IPC message queue: %s", strerror(err));
		SNDERR("The PCM needs to be open for mixers to connect to pipeline");
		goto error;
	}

	/* create a SHM mapping for low latency stream position */
	err = plug_shm_init(&ctl->shm_ctx, plug->tplg_file, "ctx", 0);
	if (err < 0)
		goto error;

	// TODO: make this open/close per operation for shared access
	/* create a SHM mapping for low latency stream position */
	err = plug_shm_open(&ctl->shm_ctx);
	if (err < 0)
		goto error;

	/* get global context for kcontrol lookup */
	ctl->glb = ctl->shm_ctx.addr;

	/* TODO: add some flavour to the names based on the topology */
	ctl->ext.version = SND_CTL_EXT_VERSION;
	ctl->ext.card_idx = 0;
	strncpy(ctl->ext.id, "sof", sizeof(ctl->ext.id) - 1);
	strncpy(ctl->ext.driver, "SOF plugin",
		sizeof(ctl->ext.driver) - 1);
	strncpy(ctl->ext.name, "SOF", sizeof(ctl->ext.name) - 1);
	strncpy(ctl->ext.mixername, "SOF",
		sizeof(ctl->ext.mixername) - 1);

	/* polling on message queue - supported on Linux but not portable */
	ctl->ext.poll_fd = ctl->ipc.mq;

	ctl->ext.callback = &sof_ext_callback;
	ctl->ext.private_data = ctl;
	ctl->ext.tlv.c = plug_tlv_rw;

	err = snd_ctl_ext_create(&ctl->ext, name, mode);
	if (err < 0)
		goto error;

	*handlep = ctl->ext.handle;

	return 0;

error:
	free(ctl);

	return err;
}

SND_CTL_PLUGIN_SYMBOL(sof);
