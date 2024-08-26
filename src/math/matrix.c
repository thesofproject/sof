// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//	   Shriram Shastry <malladi.sastry@linux.intel.com>
//

#include <sof/math/matrix.h>
#include <errno.h>
#include <stdint.h>

/**
 * Description: Performs matrix multiplication of two fixed-point 16-bit integer matrices,
 *	 storing the result in a third matrix. It accounts for fractional bits for
 *	 fixed-point arithmetic, adjusting the result accordingly.
 *
 * Arguments:
 *   a: pointer to the first input matrix
 *   b: pointer to the second input matrix
 *   c: pointer to the output matrix to store result
 *
 * Return:
 *   0 on successful multiplication.
 *   -EINVAL if input dimensions do not allow for multiplication.
 *   -ERANGE if the shift operation might cause integer overflow.
 */
int mat_multiply(struct mat_matrix_16b *a, struct mat_matrix_16b *b, struct mat_matrix_16b *c)
{
	/* Validate matrix dimensions are compatible for multiplication */
	if (a->columns != b->rows || a->rows != c->rows || b->columns != c->columns)
		return -EINVAL;

	int32_t acc;	/* Accumulator for dot product calculation */
	int16_t *x, *y, *z = c->data; /* Pointers for matrices a, b, and c */
	int i, j, k;	/* Loop counters */
	int y_inc = b->columns;	   /* Column increment for matrix b elements */
	/* Calculate shift amount for adjusting fractional bits in the result */
	const int shift = a->fractions + b->fractions - c->fractions - 1;

	/* Check shift to ensure no integer overflow occurs during shifting */
	if (shift < -1 || shift > 31)
		return -ERANGE;

	/* Special case when shift is -1 (Q0 data) */
	if (shift == -1) {
		/* Matrix multiplication loop */
		for (i = 0; i < a->rows; i++) {
			for (j = 0; j < b->columns; j++) {
				/* Initialize accumulator for each element */
				acc = 0;
				/* Set x at the start of ith row of a */
				x = a->data + a->columns * i;
				/* Set y at the top of jth column of b */
				y = b->data + j;
				/* Dot product loop */
				for (k = 0; k < b->rows; k++) {
					/* Multiply & accumulate */
					acc += (int32_t)(*x++) * (*y);
					 /* Move to next row in the current column of b */
					y += y_inc;
				}
				/* Enhanced pointer arithmetic */
				*z = (int16_t)acc;
				z++; /* Move to the next element in the output matrix */
			}
		}
	} else {
		/* General case for other shift values */
		for (i = 0; i < a->rows; i++) {
			for (j = 0; j < b->columns; j++) {
				/* Initialize accumulator for each element */
				acc = 0;
				/* Set x at the start of ith row of a */
				x = a->data + a->columns * i;
				/* Set y at the top of jth column of b */
				y = b->data + j;
				/* Dot product loop */
				for (k = 0; k < b->rows; k++) {
					/* Multiply & accumulate Enhanced pointer arithmetic */
					acc += (int32_t)(*x++) * (*y);
					/* Move to next row in the current column of b */
					y += y_inc;
				}
				/* Enhanced pointer arithmetic */
				*z = (int16_t)(((acc >> shift) + 1) >> 1); /*Shift to Qx.y */
				z++; /* Move to the next element in the output matrix */
			}
		}
	}
	return 0;
}

/**
 * Description: Performs element-wise multiplication of two matrices with 16-bit integer elements
 *		and stores the result in a third matrix. Checks that all matrices have the same
 *		dimensions and adjusts for fractional bits appropriately. This operation handles
 *		the manipulation of fixed-point precision based on the fractional bits present in
 *		the matrices.
 *
 * Arguments:
 *   a - pointer to the first input matrix
 *   b - pointer to the second input matrix
 *   c - pointer to the output matrix where the result will be stored
 *
 * Returns:
 *   0 on successful multiplication,
 *  -EINVAL if input pointers are NULL or matrix dimensions do not match.
 */
int mat_multiply_elementwise(struct mat_matrix_16b *a, struct mat_matrix_16b *b,
			     struct mat_matrix_16b *c)
{
	/* Validate matrix dimensions and non-null pointers */
	if (!a || !b || !c ||
	    a->columns != b->columns || a->rows != b->rows ||
	    c->columns != a->columns || c->rows != a->rows) {
		return -EINVAL;
	}

	int16_t *x = a->data;
	int16_t *y = b->data;
	int16_t *z = c->data;
	int64_t p;
	int i;
	const int shift_minus_one = a->fractions + b->fractions - c->fractions - 1;

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
