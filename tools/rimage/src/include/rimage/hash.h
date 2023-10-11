/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifndef __HASH_H__
#define __HASH_H__

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include <stdint.h>

/* Hash context state used to detect invalid use of hash functions */
enum hash_state { HS_INIT, HS_UPDATE, HS_DONE, HS_ERROR };

struct hash_context {
	enum hash_state state;
	EVP_MD_CTX *context;
	const EVP_MD *algo;
	uint8_t digest[EVP_MAX_MD_SIZE];
	unsigned int digest_length;
	int error;
};

/**
 * Initialize hash context with given algorithm
 * @param [out]context structure storing the hash context
 * @param [in]algo hash algorithm
 * @return error code, 0 when success
 */
int hash_init(struct hash_context *context, const EVP_MD *algo);

/**
 * Initialize sha256 hash context
 * @param [out]context structure storing the hash context
 * @return error code, 0 when success
 */
int hash_sha256_init(struct hash_context *context);

/**
 * Initialize sha384 hash context
 * @param [out]context structure storing the hash context
 * @return error code, 0 when success
 */
int hash_sha384_init(struct hash_context *context);

/**
 * Add data to hash
 * @param [in]context structure storing the hash context
 * @param [in]data data to be added
 * @param [in]size length of data in bytes
 * @return error code, 0 when success
 */
int hash_update(struct hash_context *context, const void *data, size_t size);

/**
 * Completes the hash calculation. No more data can be added!
 * @param [in]context structure storing the hash context
 * @return error code, 0 when success
 */
int hash_finalize(struct hash_context *context);

/**
 * Read out computed digest. Must finalize first.
 * @param [in]context structure storing the hash context
 * @param [out]output pointer to array where place hash value
 * @param [in]output_len size of the output buffer
 * @return copied digest length, < 0 if error
 */
int hash_get_digest(struct hash_context *context, void *output, size_t output_len);

/**
 * Print digest value
 * @param [in]context structure storing the hash context
 */
void hash_print(struct hash_context *context);

/**
 * Calculates hash of a single memory buffer
 * @param [in]data pointer to the data to be processed
 * @param [in]size length of the data to be processed
 * @param [in]algo hash algorithm
 * @param [out]output pointer to array where place hash value
 * @param [in]output_len size of the output buffer
 * @return error code, 0 when success
 */
int hash_single(const void* data, size_t size, const EVP_MD *algo, void *output, size_t output_len);

/**
 * Calculates sha256 hash of a memory buffer
 * @param [in]data pointer to the data to be processed
 * @param [in]size length of the data to be processed
 * @param [out]output pointer to array where place hash value
 * @param [in]output_len size of the output buffer
 * @return error code, 0 when success
 */
int hash_sha256(const void* data, size_t size, void *output, size_t output_len);

/**
 * Calculates sha384 hash of a memory buffer
 * @param [in]data pointer to the data to be processed
 * @param [in]size length of the data to be processed
 * @param [out]output pointer to array where place hash value
 * @param [in]output_len size of the output buffer
 * @return error code, 0 when success
 */
int hash_sha384(const void* data, size_t size, void *output, size_t output_len);

#endif /* __HASH_H__ */
