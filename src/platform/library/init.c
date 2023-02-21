// SPDX-License-Identifier: BSD-3-Clause

#include <rtos/sof.h>
#include <stdlib.h>

/* main firmware context */
static struct sof sof;

struct sof *sof_get()
{
	return &sof;
}
