// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//

#include <sof/math/sqrt.h>
#include <rtos/symbol.h>
#include <stdint.h>

/* Lookup table for square root for initial value in iteration,
 * created with Octave commands:
 *
 *   arg=((1:64) * 2^25) / 2^30; lut = int32(sqrt(arg) * 2^30);
 *   fmt=['static const int32_t sqrt_int32_lut[] = {' repmat(' %d,',1, numel(lut)-1) ' %d};\n'];
 *   fprintf(fmt, lut)
 */
static const int32_t sqrt_int32_lut[] = {
	189812531,  268435456,  328764948,  379625062,  424433723,  464943848,  502196753,
	536870912,  569437594,  600239927,  629536947,  657529896,  684378814,  710213460,
	735140772,  759250125,  782617115,  805306368,  827373642,  848867446,  869830292,
	890299688,  910308921,  929887697,  949062656,  967857801,  986294844,  1004393507,
	1022171763, 1039646051, 1056831447, 1073741824, 1090389977, 1106787739, 1122946079,
	1138875187, 1154584553, 1170083026, 1185378878, 1200479854, 1215393219, 1230125796,
	1244684005, 1259073893, 1273301169, 1287371222, 1301289153, 1315059792, 1328687719,
	1342177280, 1355532607, 1368757628, 1381856086, 1394831545, 1407687407, 1420426919,
	1433053185, 1445569171, 1457977717, 1470281545, 1482483261, 1494585366, 1506590260,
	1518500250
};

/* sofm_sqrt_int32() - Calculate 32-bit fractional square root function. */
int32_t sofm_sqrt_int32(int32_t n)
{
	uint64_t n_shifted;
	uint32_t x;
	int shift;

	if (n < 1)
		return 0;

	/* Scale input argument with 2^n, where n is even.
	 * Scale calculated sqrt() with 2^(-n/2).
	 */
	shift = (__builtin_clz(n) - 1) & ~1; /* Make even by clearing LSB */
	n = n << shift;
	shift >>= 1; /* Divide by 2 for sqrt shift compensation */

	/* For Q2.30 divide */
	n_shifted = (uint64_t)n << 30;

	/* Get initial guess from LUT, idx = 0 .. 63 */
	x = sqrt_int32_lut[n >> 25];

	/* Iterate x(n+1) = 1/2 * (x(n) + N / x(n))
	 * N is argument for square root
	 * x(n) is initial guess. Do two iterations.
	 */
	x = (uint32_t)(((n_shifted / x + x) + 1) >> 1);
	x = (uint32_t)(((n_shifted / x + x) + 1) >> 1);

	return (int32_t)(x >> shift);
}
EXPORT_SYMBOL(sofm_sqrt_int32);
