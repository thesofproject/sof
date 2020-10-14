// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google Inc. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>

#include <sof/sof.h>
#include <sof/lib/dai.h>

const struct dai_type_info dti[] = {
};

const struct dai_info lib_dai = {
	.dai_type_array = dti,
	.num_dai_types = ARRAY_SIZE(dti)
};

int dai_init(struct sof *sof)
{
	sof->dai_info = &lib_dai;

	return 0;
}
