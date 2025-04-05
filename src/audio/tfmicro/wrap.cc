
// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#include <cstdint>
#include <assert.h>
#include "speech.h"

/* llext wrapping for C++ */

namespace __cxxabiv1
{
    extern "C" void
    __cxa_pure_virtual (void)
    {
        assert(0);
    }

    extern "C" void
    __cxa_deleted_virtual (void)
    {
        assert(0);
    }
}
