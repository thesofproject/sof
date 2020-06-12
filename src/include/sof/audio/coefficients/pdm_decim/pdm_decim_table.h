/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/* PDM decimation FIR filters */

#ifndef __SOF_AUDIO_COEFFICIENTS_PDM_DECIM_PDM_DECIM_TABLE_H__
#define __SOF_AUDIO_COEFFICIENTS_PDM_DECIM_PDM_DECIM_TABLE_H__

#include "pdm_decim_fir.h"

#include <stddef.h>

#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_2
#include "pdm_decim_int32_02_4288_5100_010_095.h"
#include "pdm_decim_int32_02_4375_5100_010_095.h"
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_3
#include "pdm_decim_int32_03_3850_5100_010_095.h"
#include "pdm_decim_int32_03_4375_5100_010_095.h"
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_4
#include "pdm_decim_int32_04_4375_5100_010_095.h"
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_5
#include "pdm_decim_int32_05_4331_5100_010_095.h"
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_6
#include "pdm_decim_int32_06_4156_5100_010_095.h"
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_8
#include "pdm_decim_int32_08_4156_5380_010_090.h"
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_12
#include "pdm_decim_int32_12_4156_6018_010_090.h"
#endif

/* Note: Higher spec filter must be before lower spec filter
 * if there are multiple filters for a decimation factor. The naming
 * scheme of coefficients set is:
 * <type>_<decim factor>_<rel passband>_<rel stopband>_<ripple>_<attenuation>
 */
struct pdm_decim *fir_list[] = {
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_2
	&pdm_decim_int32_02_4375_5100_010_095,
	&pdm_decim_int32_02_4288_5100_010_095,
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_3
	&pdm_decim_int32_03_4375_5100_010_095,
	&pdm_decim_int32_03_3850_5100_010_095,
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_4
	&pdm_decim_int32_04_4375_5100_010_095,
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_5
	&pdm_decim_int32_05_4331_5100_010_095,
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_6
	&pdm_decim_int32_06_4156_5100_010_095,
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_8
	&pdm_decim_int32_08_4156_5380_010_090,
#endif
#if CONFIG_INTEL_DMIC_FIR_DECIMATE_BY_12
	&pdm_decim_int32_12_4156_6018_010_090,
#endif
	NULL, /* This marks the end of coefficients */
};

#endif /* __SOF_AUDIO_COEFFICIENTS_PDM_DECIM_PDM_DECIM_TABLE_H__ */
