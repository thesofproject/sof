/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifdef __SOF_LIB_DAI_H__

#ifndef __PLATFORM_LIB_DAI_H__
#define __PLATFORM_LIB_DAI_H__

/* SUECREEK */

/* SSP */

/*
 * Number of base and extended SSP ports must be defined separately
 * since some HW registers are in two groups, one for base and one
 * for extended.
 */

/** \brief Number of 'base' SSP ports available */
#define DAI_NUM_SSP_BASE	3

/** \brief Number of 'extended' SSP ports available */
#define DAI_NUM_SSP_EXT		0

/** \brief Number of SSP MCLKs available */
#define DAI_NUM_SSP_MCLK	2

/** \brief Number of HD/A Link Outputs */
#define DAI_NUM_HDA_OUT		9

/** \brief Number of HD/A Link Inputs */
#define DAI_NUM_HDA_IN		7

#endif /* __PLATFORM_LIB_DAI_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/dai.h"

#endif /* __SOF_LIB_DAI_H__ */
