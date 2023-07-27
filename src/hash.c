// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Adrian Warecki <adrian.warecki@intel.com>
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include <rimage/rimage.h>
#include <rimage/manifest.h>
#include <rimage/hash.h>

#define DEBUG_HASH 0

#if OPENSSL_VERSION_NUMBER < 0x10100000L
void EVP_MD_CTX_free(EVP_MD_CTX *ctx);
EVP_MD_CTX *EVP_MD_CTX_new(void);

static void *OPENSSL_zalloc(size_t num)
{
	void *ret = OPENSSL_malloc(num);

	if (ret)
		memset(ret, 0, num);
	return ret;
}

EVP_MD_CTX *EVP_MD_CTX_new(void)
{
	return OPENSSL_zalloc(sizeof(EVP_MD_CTX));
}

void EVP_MD_CTX_free(EVP_MD_CTX *ctx)
{
	EVP_MD_CTX_cleanup(ctx);
	OPENSSL_free(ctx);
}
#endif

static int hash_error(struct hash_context *context, int errcode, const char *msg)
{
	EVP_MD_CTX_free(context->context);
	context->context = NULL;
	context->state = HS_ERROR;
	context->error = -errcode;
	fprintf(stderr, "hash: %s\n", msg);
	return context->error;
}

int hash_init(struct hash_context *context, const EVP_MD *algo)
{
	assert(context);
	assert(algo);

	context->error = 0;
	context->digest_length = 0;
	context->algo = algo;

	context->context = EVP_MD_CTX_new();
	if (!context->context)
		return hash_error(context, ENOMEM, "Unable to allocate hash context.");

	if (!EVP_DigestInit_ex(context->context, context->algo, NULL)) {
		EVP_MD_CTX_free(context->context);
		return hash_error(context, ENOTRECOVERABLE, "Unable to initialize hash context.");
	}

	context->state = HS_UPDATE;
	return 0;
}

int hash_sha256_init(struct hash_context *context)
{
	return hash_init(context, EVP_sha256());
}

int hash_sha384_init(struct hash_context *context)
{
	return hash_init(context, EVP_sha384());
}

int hash_update(struct hash_context *context, const void *data, size_t size)
{
	assert(context);
	assert(data);

	if (context->error)
		return context->error;

	assert(context->state == HS_UPDATE);

	if (!EVP_DigestUpdate(context->context, data, size))
		return hash_error(context, EINVAL, "Unable to update hash context.");

	return 0;
}

int hash_finalize(struct hash_context *context)
{
	assert(context);

	if (context->error)
		return context->error;

	assert(context->state == HS_UPDATE);

	if (!EVP_DigestFinal_ex(context->context, context->digest, &context->digest_length))
		return hash_error(context, EINVAL, "Unable to finalize hash context.");

	context->state = HS_DONE;

#if DEBUG_HASH
	fprintf(stdout, "Hash result is: ");
	hash_print(context);
#endif

	EVP_MD_CTX_free(context->context);
	context->context = NULL;
	return 0;
}

int hash_get_digest(struct hash_context *context, void *output, size_t output_len)
{
	assert(context);
	assert(output);

	if (context->error)
		return context->error;

	assert(context->state == HS_DONE);

	if (context->digest_length > output_len)
		return -ENOBUFS;

	memcpy(output, context->digest, context->digest_length);
	return context->digest_length;
}

void hash_print(struct hash_context *context)
{
	unsigned int i;

	assert(context);
	assert(context->state == HS_DONE);
	assert(context->digest_length);

	for (i = 0; i < context->digest_length; i++)
		fprintf(stdout, "%02x", context->digest[i]);
	fprintf(stdout, "\n");
}

int hash_single(const void *data, size_t size, const EVP_MD *algo, void *output, size_t output_len)
{
	int algo_out_size;

	assert(algo);
	assert(data);
	assert(output);

	//algo_out_size = EVP_MD_get_size(algo);
	algo_out_size = EVP_MD_size(algo);
	if (algo_out_size <= 0)
		return -EINVAL;

	if (output_len > algo_out_size)
		return -ENOBUFS;

	if (!EVP_Digest(data, size, output, NULL, algo, NULL)) {
		fprintf(stderr, "Unable to compute hash.");
		return -ENOTRECOVERABLE;
	}

	return 0;
}

int hash_sha256(const void *data, size_t size, void *output, size_t output_len)
{
	return hash_single(data, size, EVP_sha256(), output, output_len);
}

int hash_sha384(const void *data, size_t size, void *output, size_t output_len)
{
	return hash_single(data, size, EVP_sha384(), output, output_len);
}
