// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//	   Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/* load buffer DAPM widget */
static int tb_register_buffer(struct testbench_prm *tp)
{
	struct tplg_context *ctx = &tp->tplg;
	struct sof *sof = ctx->sof;
	struct sof_ipc_buffer buffer = {{{0}}};
	int ret;

	ret = tplg_new_buffer(ctx, &buffer, sizeof(buffer),
			      NULL, 0);
	if (ret < 0)
		return ret;

	/* create buffer component */
	if (ipc_buffer_new(sof->ipc, &buffer) < 0) {
		fprintf(stderr, "error: buffer new\n");
		return -EINVAL;
	}

	return 0;
}

/* load fileread component */
static int tb_new_fileread(struct testbench_prm *tp, struct sof_ipc_comp_file *fileread)
{
	struct tplg_context *ctx = &tp->tplg;
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	char uuid[UUID_SIZE];
	int ret;

	/* read vendor tokens */
	while (total_array_size < size) {
		if (!tplg_is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: filewrite array size mismatch for widget size %d\n",
				size);
			return -EINVAL;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&fileread->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse comp tokens %d\n",
				size);
			return -EINVAL;
		}

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse mixer uuid token %d\n", size);
			return -EINVAL;
		}

		total_array_size += array->size;
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* configure fileread */
	fileread->mode = FILE_READ;
	fileread->comp.id = comp_id;

	/* use fileread comp as scheduling comp */
	fileread->size = sizeof(struct ipc_comp_file);
	fileread->comp.core = ctx->core_id;
	fileread->comp.hdr.size = sizeof(struct sof_ipc_comp_file) + UUID_SIZE;
	fileread->comp.type = SOF_COMP_FILEREAD;
	fileread->comp.pipeline_id = ctx->pipeline_id;
	fileread->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	fileread->comp.ext_data_length = UUID_SIZE;
	return 0;
}

/* load filewrite component */
static int tb_new_filewrite(struct testbench_prm *tp, struct sof_ipc_comp_file *filewrite)
{
	struct tplg_context *ctx = &tp->tplg;
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	char uuid[UUID_SIZE];
	int ret;

	/* read vendor tokens */
	while (total_array_size < size) {
		if (!tplg_is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: filewrite array size mismatch\n");
			return -EINVAL;
		}

		ret = sof_parse_tokens(&filewrite->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse filewrite tokens %d\n",
				size);
			return -EINVAL;
		}

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse mixer uuid token %d\n", size);
			return -EINVAL;
		}

		total_array_size += array->size;
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* configure filewrite */
	filewrite->comp.core = ctx->core_id;
	filewrite->comp.id = comp_id;
	filewrite->mode = FILE_WRITE;
	filewrite->size = sizeof(struct ipc_comp_file);
	filewrite->comp.hdr.size = sizeof(struct sof_ipc_comp_file) + UUID_SIZE;
	filewrite->comp.type = SOF_COMP_FILEWRITE;
	filewrite->comp.pipeline_id = ctx->pipeline_id;
	filewrite->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	filewrite->comp.ext_data_length = UUID_SIZE;
	return 0;
}

/* load fileread component */
static int tb_register_fileread(struct testbench_prm *tp, int dir)
{
	struct tplg_context *ctx = &tp->tplg;
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_file *fileread;
	struct sof_uuid *file_uuid;
	int ret;

	fileread = calloc(MAX_TPLG_OBJECT_SIZE, 1);
	if (!fileread)
		return -ENOMEM;

	fileread->config.frame_fmt = tplg_find_format(tp->bits_in);

	ret = tb_new_fileread(tp, &fileread);
	if (ret < 0)
		return ret;

	/* configure fileread */
	if (!tp->input_file[tp->input_file_index]) {
		fprintf(stderr,
			"error: input file [%d] is not defined, add filename to -i f1,f2,...\n",
			tp->input_file_index);
		return -EINVAL;
	}

	fileread.fn = strdup(tp->input_file[tp->input_file_index]);
	if (tp->input_file_index == 0)
		tp->fr_id = ctx->comp_id;

	/* use fileread comp as scheduling comp */
	ctx->sched_id = ctx->comp_id;
	tp->input_file_index++;

	/* Set format from testbench command line*/
	fileread->rate = tp->fs_in;
	fileread->channels = tp->channels_in;
	fileread->frame_fmt = tp->frame_fmt;
	fileread->direction = dir;

	file_uuid = (struct sof_uuid *)((uint8_t *)fileread + sizeof(struct sof_ipc_comp_file));
	file_uuid->a = 0xbfc7488c;
	file_uuid->b = 0x75aa;
	file_uuid->c = 0x4ce8;
	file_uuid->d[0] = 0x9d;
	file_uuid->d[1] = 0xbe;
	file_uuid->d[2] = 0xd8;
	file_uuid->d[3] = 0xda;
	file_uuid->d[4] = 0x08;
	file_uuid->d[5] = 0xa6;
	file_uuid->d[6] = 0x98;
	file_uuid->d[7] = 0xc2;

	/* create fileread component */
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(fileread)) < 0) {
		fprintf(stderr, "error: file read\n");
		free(fileread->fn);
		return -EINVAL;
	}

	free(fileread->fn);
	free(fileread);
	return 0;
}

/* load filewrite component */
static int tb_register_filewrite(struct testbench_prm *tp, int dir)
{
	struct tplg_context *ctx = &tp->tplg;
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_file *filewrite;
	struct sof_uuid *file_uuid;
	int ret;

	filewrite = calloc(MAX_TPLG_OBJECT_SIZE, 1);
	if (!filewrite)
		return -ENOMEM;

	ret = tb_new_filewrite(tp, filewrite);
	if (ret < 0)
		return ret;

	/* configure filewrite (multiple output files are supported.) */
	if (!tp->output_file[tp->output_file_index]) {
		fprintf(stderr,
			"error: output file [%d] is not defined, add filename to -o f1,f2,..\n",
			tp->output_file_index);
		return -EINVAL;
	}
	filewrite->fn = strdup(tp->output_file[tp->output_file_index]);
	if (tp->output_file_index == 0)
		tp->fw_id = ctx->comp_id;
	tp->output_file_index++;

	/* Set format from testbench command line*/
	filewrite->rate = tp->fs_out;
	filewrite->channels = tp->channels_out;
	filewrite->frame_fmt = tp->frame_fmt;
	filewrite->direction = dir;

	file_uuid = (struct sof_uuid *)((uint8_t *)filewrite + sizeof(struct sof_ipc_comp_file));
	file_uuid->a = 0xbfc7488c;
	file_uuid->b = 0x75aa;
	file_uuid->c = 0x4ce8;
	file_uuid->d[0] = 0x9d;
	file_uuid->d[1] = 0xbe;
	file_uuid->d[2] = 0xd8;
	file_uuid->d[3] = 0xda;
	file_uuid->d[4] = 0x08;
	file_uuid->d[5] = 0xa6;
	file_uuid->d[6] = 0x98;
	file_uuid->d[7] = 0xc2;

	/* create filewrite component */
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(filewrite)) < 0) {
		fprintf(stderr, "error: new file write\n");
		free(filewrite->fn);
		return -EINVAL;
	}

	free(filewrite->fn);
	free(filewrite);
	return 0;
}

