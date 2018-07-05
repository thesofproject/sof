/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

 /**
  * \file include/sof/platform.h
  * \brief Platform API definition
  * \author Marcin Maka <marcin.maka@linux.intel.com>
  */

#ifndef __INCLUDE_SOF_PLATFORM_H__
#define __INCLUDE_SOF_PLATFORM_H__

#include <sof/sof.h>

/** \addtogroup platform_api Platform API
 *  Platform API specification.
 *  @{
 */

/*
 * APIs declared here are defined for every platform.
 */

/**
 * \brief Platform specific implementation of the On Boot Complete handler.
 * \param[in] boot_message Boot status code.
 * \return 0 if successful, error code otherwise.
 */
int platform_boot_complete(uint32_t boot_message);

/**
 * \brief Platform initialization entry, called during FW initialization.
 * \param[in] sof Context.
 * \return 0 if successful, error code otherwise.
 */
int platform_init(struct sof *sof);

/**
 * \brief Called by the panic handler.
 * \param[in] p Panic cause, one of SOF_IPC_PANIC_... codes (see ipc.h).
 */
static inline void platform_panic(uint32_t p);

/** @}*/

#endif
