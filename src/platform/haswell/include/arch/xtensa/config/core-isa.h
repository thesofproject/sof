/* SPDX-License-Identifier: BSD-3-Clause */

#if CONFIG_HASWELL
#include <xtensa/config/core-isa-hsw.h>
#elif CONFIG_BROADWELL
#include <xtensa/config/core-isa-bdw.h>
#else
#error "No ISA configuration selected"
#endif
