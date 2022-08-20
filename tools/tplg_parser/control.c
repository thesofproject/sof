// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Topology parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <ipc/topology.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

int tplg_create_single_control(struct snd_soc_tplg_ctl_hdr **ctl, char **priv_data,
			  FILE *file)
{
	struct snd_soc_tplg_ctl_hdr *ctl_hdr;
	struct snd_soc_tplg_mixer_control *mixer_ctl = NULL;
	struct snd_soc_tplg_enum_control *enum_ctl = NULL;
	struct snd_soc_tplg_bytes_control *bytes_ctl = NULL;
	size_t rewind_size;
	size_t read_size;
	size_t hdr_size;
	int ret;

	/* These are set if success */
	*ctl = NULL;
	*priv_data = NULL;

	/* allocate memory */
	hdr_size = sizeof(struct snd_soc_tplg_ctl_hdr);
	ctl_hdr = (struct snd_soc_tplg_ctl_hdr *)malloc(hdr_size);
	if (!ctl_hdr) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* read control header */
	ret = fread(ctl_hdr, hdr_size, 1, file);
	if (ret != 1) {
		ret = -EINVAL;
		goto err;
	}

	/* load control based on type */
	switch (ctl_hdr->ops.info) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_STROBE:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
		/* load mixer type control */
		rewind_size = sizeof(struct snd_soc_tplg_ctl_hdr);
		if (fseek(file, (long)rewind_size * -1, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		read_size = sizeof(struct snd_soc_tplg_mixer_control);
		mixer_ctl = malloc(read_size);
		if (!mixer_ctl) {
			fprintf(stderr, "error: mem alloc\n");
			ret = -errno;
			goto err;
		}

		ret = fread(mixer_ctl, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		/* skip mixer private data */
		if (fseek(file, mixer_ctl->priv.size, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		*ctl = (struct snd_soc_tplg_ctl_hdr *)mixer_ctl;
		break;

	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
		/* load enum type control */
		rewind_size = sizeof(struct snd_soc_tplg_ctl_hdr);
		if (fseek(file, (long)rewind_size * -1, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		read_size = sizeof(struct snd_soc_tplg_enum_control);
		enum_ctl = malloc(read_size);
		if (!enum_ctl) {
			fprintf(stderr, "error: mem alloc\n");
			ret = -errno;
			goto err;
		}

		ret = fread(enum_ctl, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		/* skip enum private data */
		if (fseek(file, enum_ctl->priv.size, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		*ctl = (struct snd_soc_tplg_ctl_hdr *)enum_ctl;
		break;

	case SND_SOC_TPLG_CTL_BYTES:
		/* load bytes type controls */
		rewind_size = sizeof(struct snd_soc_tplg_ctl_hdr);
		if (fseek(file, (long)rewind_size * -1, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		read_size = sizeof(struct snd_soc_tplg_bytes_control);
		bytes_ctl = malloc(read_size);
		if (!bytes_ctl) {
			fprintf(stderr, "error: mem alloc\n");
			ret = -errno;
			goto err;
		}

		ret = fread(bytes_ctl, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		/* Get private data */
		read_size = bytes_ctl->priv.size;
		*priv_data = malloc(read_size);
		if (!(*priv_data)) {
			fprintf(stderr, "error: mem alloc\n");
			ret = -errno;
			goto err;
		}

		ret = fread(*priv_data, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		*ctl = (struct snd_soc_tplg_ctl_hdr *)bytes_ctl;
		break;

	default:
		printf("info: control type not supported\n");
		return -EINVAL;
	}

	free(ctl_hdr);
	return 0;

err:
	/* free all data */
	free(ctl_hdr);
	free(mixer_ctl);
	free(enum_ctl);
	free(bytes_ctl);
	free(*priv_data);
	return ret;
}

/* load dapm widget kcontrols
 * we don't use controls in the fuzzer atm.
 * so just skip to the next dapm widget
 */
int tplg_create_controls(int num_kcontrols, FILE *file, struct snd_soc_tplg_ctl_hdr *rctl,
			 size_t max_ctl_size)
{
	struct snd_soc_tplg_ctl_hdr *ctl_hdr;
	struct snd_soc_tplg_mixer_control *mixer_ctl;
	struct snd_soc_tplg_enum_control *enum_ctl;
	struct snd_soc_tplg_bytes_control *bytes_ctl;
	size_t read_size, size;
	int j, ret = 0;

	/* allocate memory */
	size = sizeof(struct snd_soc_tplg_ctl_hdr);
	ctl_hdr = (struct snd_soc_tplg_ctl_hdr *)calloc(size, 1);
	if (!ctl_hdr) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	size = sizeof(struct snd_soc_tplg_mixer_control);
	mixer_ctl = (struct snd_soc_tplg_mixer_control *)calloc(size, 1);
	if (!mixer_ctl) {
		fprintf(stderr, "error: mem alloc\n");
		free(ctl_hdr);
		return -errno;
	}

	size = sizeof(struct snd_soc_tplg_enum_control);
	enum_ctl = (struct snd_soc_tplg_enum_control *)calloc(size, 1);
	if (!enum_ctl) {
		fprintf(stderr, "error: mem alloc\n");
		free(ctl_hdr);
		free(mixer_ctl);
		return -errno;
	}

	size = sizeof(struct snd_soc_tplg_bytes_control);
	bytes_ctl = (struct snd_soc_tplg_bytes_control *)calloc(size, 1);
	if (!bytes_ctl) {
		fprintf(stderr, "error: mem alloc\n");
		free(ctl_hdr);
		free(mixer_ctl);
		free(enum_ctl);
		return -errno;
	}

	for (j = 0; j < num_kcontrols; j++) {
		/* read control header */
		read_size = sizeof(struct snd_soc_tplg_ctl_hdr);
		ret = fread(ctl_hdr, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		/* load control based on type */
		switch (ctl_hdr->ops.info) {
		case SND_SOC_TPLG_CTL_VOLSW:
		case SND_SOC_TPLG_CTL_STROBE:
		case SND_SOC_TPLG_CTL_VOLSW_SX:
		case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		case SND_SOC_TPLG_CTL_RANGE:
		case SND_SOC_TPLG_DAPM_CTL_VOLSW:
			/* load mixer type control */
			read_size = sizeof(struct snd_soc_tplg_ctl_hdr);
			if (fseek(file, (long)read_size * -1, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}

			read_size = sizeof(struct snd_soc_tplg_mixer_control);
			ret = fread(mixer_ctl, read_size, 1, file);
			if (ret != 1) {
				ret = -EINVAL;
				goto err;
			}

			/* skip mixer private data */
			if (fseek(file, mixer_ctl->priv.size, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}
			break;

		case SND_SOC_TPLG_CTL_ENUM:
		case SND_SOC_TPLG_CTL_ENUM_VALUE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
			/* load enum type control */
			read_size = sizeof(struct snd_soc_tplg_ctl_hdr);
			if (fseek(file, (long)read_size * -1, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}

			read_size = sizeof(struct snd_soc_tplg_enum_control);
			ret = fread(enum_ctl, read_size, 1, file);
			if (ret != 1) {
				ret = -EINVAL;
				goto err;
			}

			/* skip enum private data */
			if (fseek(file, enum_ctl->priv.size, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}
			break;

		case SND_SOC_TPLG_CTL_BYTES:
			/* load bytes type controls */
			read_size = sizeof(struct snd_soc_tplg_ctl_hdr);
			if (fseek(file, (long)read_size * -1, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}

			read_size = sizeof(struct snd_soc_tplg_bytes_control);
			ret = fread(bytes_ctl, read_size, 1, file);
			if (ret != 1) {
				ret = -EINVAL;
				goto err;
			}

			/* skip bytes private data */
			if (fseek(file, bytes_ctl->priv.size, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}
			break;

		default:
			printf("info: control type not supported\n");
			return -EINVAL;
		}
	}

	if (rctl) {
		/* make sure the CTL will fit if we need to copy it for others */
		if (ctl_hdr->size > max_ctl_size) {
			fprintf(stderr, "error: failed control control copy\n");
			ret = -EINVAL;
			goto err;
		}
		memcpy(rctl, ctl_hdr, ctl_hdr->size);
	}
err:
	/* free all data */
	free(mixer_ctl);
	free(enum_ctl);
	free(bytes_ctl);
	free(ctl_hdr);
	return ret;
}
