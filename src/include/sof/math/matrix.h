/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/* Matrix calculation related functions */

#ifndef __SOF_MATH_MATRIX_H__
#define __SOF_MATH_MATRIX_H__

#include <rtos/alloc.h>
#include <ipc/topology.h>
#include <stdint.h>
#include <string.h>

struct mat_matrix_16b {
	int16_t rows;
	int16_t columns;
	int16_t fractions;
	int16_t reserved;
	int16_t data[];
};

static inline void mat_init_16b(struct mat_matrix_16b *mat, int16_t rows, int16_t columns,
				int16_t fractions)
{
	mat->rows = rows;
	mat->columns = columns;
	mat->fractions = fractions;
}

static inline struct mat_matrix_16b *mat_matrix_alloc_16b(int16_t rows, int16_t columns,
							  int16_t fractions)
{
	struct mat_matrix_16b *mat;
	const int mat_size = sizeof(int16_t) * rows * columns + sizeof(struct mat_matrix_16b);

	mat = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, mat_size);
	if (mat)
		mat_init_16b(mat, rows, columns, fractions);

	return mat;
}

static inline void mat_copy_from_linear_16b(struct mat_matrix_16b *mat, const int16_t *lin_data)
{
	size_t bytes = sizeof(int16_t) * mat->rows * mat->columns;

	memcpy_s(mat->data, bytes, lin_data, bytes);
}

static inline void mat_set_all_16b(struct mat_matrix_16b *mat, int16_t val)
{
	const int n = mat->rows * mat->columns;
	int i;

	for (i = 0; i < n; i++)
		mat->data[i] = val;
}

static inline int16_t mat_get_scalar_16b(struct mat_matrix_16b *mat, int row, int col)
{
	return mat->data[col + row * mat->columns];
}

static inline void mat_set_scalar_16b(struct mat_matrix_16b *mat, int row, int col, int16_t val)
{
	mat->data[col + row * mat->columns] = val;
}

static inline int16_t *mat_get_row_vector_16b(struct mat_matrix_16b *mat, int row)
{
	return mat->data + row * mat->columns;
}

int mat_multiply(struct mat_matrix_16b *a, struct mat_matrix_16b *b, struct mat_matrix_16b *c);

int mat_multiply_elementwise(struct mat_matrix_16b *a, struct mat_matrix_16b *b,
			     struct mat_matrix_16b *c);

#endif /* __SOF_MATH_MATRIX_H__ */
