// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/math/matrix.h>
#include <errno.h>
#include <stdint.h>

int mat_multiply(struct mat_matrix_16b *a, struct mat_matrix_16b *b, struct mat_matrix_16b *c)
{
	int64_t s;
	int16_t *x;
	int16_t *y;
	int16_t *z = c->data;
	int i, j, k;
	int y_inc = b->columns;
	const int shift_minus_one = a->fractions + b->fractions - c->fractions - 1;

	if (a->columns != b->rows || a->rows != c->rows || b->columns != c->columns)
		return -EINVAL;

	/* If all data is Q0 */
	if (shift_minus_one == -1) {
		for (i = 0; i < a->rows; i++) {
			for (j = 0; j < b->columns; j++) {
				s = 0;
				x = a->data + a->columns * i;
				y = b->data + j;
				for (k = 0; k < b->rows; k++) {
					s += (int32_t)(*x) * (*y);
					x++;
					y += y_inc;
				}
				*z = (int16_t)s; /* For Q16.0 */
				z++;
			}
		}

		return 0;
	}

	for (i = 0; i < a->rows; i++) {
		for (j = 0; j < b->columns; j++) {
			s = 0;
			x = a->data + a->columns * i;
			y = b->data + j;
			for (k = 0; k < b->rows; k++) {
				s += (int32_t)(*x) * (*y);
				x++;
				y += y_inc;
			}
			*z = (int16_t)(((s >> shift_minus_one) + 1) >> 1); /*Shift to Qx.y */
			z++;
		}
	}
	return 0;
}

int mat_multiply_elementwise(struct mat_matrix_16b *a, struct mat_matrix_16b *b,
			     struct mat_matrix_16b *c)
{	int64_t p;
	int16_t *x = a->data;
	int16_t *y = b->data;
	int16_t *z = c->data;
	int i;
	const int shift_minus_one = a->fractions + b->fractions - c->fractions - 1;

	if (a->columns != b->columns || b->columns != c->columns ||
	    a->rows != b->rows || b->rows != c->rows) {
		return -EINVAL;
	}

	/* If all data is Q0 */
	if (shift_minus_one == -1) {
		for (i = 0; i < a->rows * a->columns; i++) {
			*z = *x * *y;
			x++;
			y++;
			z++;
		}

		return 0;
	}

	for (i = 0; i < a->rows * a->columns; i++) {
		p = (int32_t)(*x) * *y;
		*z = (int16_t)(((p >> shift_minus_one) + 1) >> 1); /*Shift to Qx.y */
		x++;
		y++;
		z++;
	}

	return 0;
}
