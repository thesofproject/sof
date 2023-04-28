// SPDX-License-Identifier: BSD-3-Clause

#include <stdint.h>
#include <xtensa/hal.h>

int _xt_atomic_compare_exchange_4(int *address, int32_t test_value, int32_t set_value);

int _xt_atomic_compare_exchange_4(int *address, int32_t test_value, int32_t set_value)
{
	return xthal_compare_and_set(address, test_value, set_value);
}
