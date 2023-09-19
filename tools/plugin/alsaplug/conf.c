/*-*- linux-c -*-*/

/*
 * ALSA <-> SOF Config plugin
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 *
 */

#include <stdio.h>
#include <alsa/asoundlib.h>
#include "plugin.h"

/* Not actually part of the alsa api....  */
extern int
snd_config_hook_load(snd_config_t *root, snd_config_t *config,
		     snd_config_t **dst, snd_config_t *private_data);

/* manifest file could be loaded as a hook */
int sofplug_load_hook(snd_config_t *root, snd_config_t *config,
		      snd_config_t **dst, snd_config_t *private_data)
{
	int ret = 0;

	*dst = NULL;

	/* TODO: load hook when SOF ready */
	return snd_config_hook_load(root, config, dst, private_data);
}

SND_DLSYM_BUILD_VERSION(sofplug_load_hook,
			SND_CONFIG_DLSYM_VERSION_HOOK);
