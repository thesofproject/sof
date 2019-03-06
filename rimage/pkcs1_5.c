/*
 * Copyright (c) 2017, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 *  Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *          Keyon Jie <yang.jie@linux.intel.com>
 */

#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/sha.h>
#include <openssl/objects.h>
#include <openssl/bn.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "rimage.h"
#include "css.h"
#include "manifest.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
void RSA_get0_key(const RSA *r,
		  const BIGNUM **n, const BIGNUM **e, const BIGNUM **d);

void RSA_get0_key(const RSA *r,
		  const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
{
	if (n)
		*n = r->n;
	if (e)
		*e = r->e;
	if (d)
		*d = r->d;
}
#endif

#define DEBUG_PKCS	0

static void bytes_swap(uint8_t *ptr, uint32_t size)
{
	uint8_t tmp;
	uint32_t index;

	for (index = 0; index < (size / 2); index++) {
		tmp = ptr[index];
		ptr[index] = ptr[size - 1 - index];
		ptr[size - 1 - index] = tmp;
	}
}

/*
 * RSA signature of manifest. The signature is an PKCS
 * #1-v1_5 of the entire manifest structure, including all
 * extensions, and excluding the last 3 fields of the
 * manifest header (Public Key, Exponent and Signature).
*/

int pkcs_v1_5_sign_man_v1_5(struct image *image,
			    struct fw_image_manifest_v1_5 *man,
			    void *ptr1, unsigned int size1)
{
	RSA *priv_rsa = NULL;
	EVP_PKEY *privkey;
	FILE *fp;

	const BIGNUM *n, *e, *d;
	unsigned char digest[SHA256_DIGEST_LENGTH];
	unsigned char mod[MAN_RSA_KEY_MODULUS_LEN];
	unsigned int siglen = MAN_RSA_SIGNATURE_LEN;
	char path[256];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1);
#endif

	/* create new key */
	privkey = EVP_PKEY_new();
	if (!privkey)
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	if (!image->key_name)
		sprintf(path, "%s/otc_private_key.pem", PEM_KEY_PREFIX);
	else
		strncpy(path, image->key_name, sizeof(path) - 1);

	fprintf(stdout, " pkcs: signing with key %s\n", path);
	fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "error: can't open file %s %d\n",
			path, -errno);
		return -errno;
	}
	PEM_read_PrivateKey(fp, &privkey, NULL, NULL);
	fclose(fp);

	/* validate RSA private key */
	priv_rsa = EVP_PKEY_get1_RSA(privkey);
	if (RSA_check_key(priv_rsa)) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest */
	module_sha256_create(image);
	module_sha256_update(image, ptr1, size1);
	module_sha256_complete(image, digest);

	fprintf(stdout, " pkcs: digest for manifest is ");
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
		fprintf(stdout, "%02x", digest[i]);
	fprintf(stdout, "\n");

	/* sign the manifest */
	ret = RSA_sign(NID_sha256, digest, SHA256_DIGEST_LENGTH,
		       (unsigned char *)man->css_header.signature,
		       &siglen, priv_rsa);
	if (ret < 0)
		fprintf(stderr, "error: failed to sign manifest\n");

	/* copy public key modulus and exponent to manifest */
	RSA_get0_key(priv_rsa, &n, &e, &d);
	BN_bn2bin(n, mod);
	BN_bn2bin(e, (unsigned char *)man->css_header.exponent);

	/* modulus is reveresd  */
	for (i = 0; i < MAN_RSA_KEY_MODULUS_LEN; i++)
		man->css_header.modulus[i]
		= mod[MAN_RSA_KEY_MODULUS_LEN - (1 + i)];

	/* signature is reveresd, swap it */
	bytes_swap(man->css_header.signature,
		   sizeof(man->css_header.signature));

	EVP_PKEY_free(privkey);
	return ret;
}

/*
 * RSA signature of manifest. The signature is an PKCS
 * #1-v1_5 of the entire manifest structure, including all
 * extensions, and excluding the last 3 fields of the
 * manifest header (Public Key, Exponent and Signature).
 */

int pkcs_v1_5_sign_man_v1_8(struct image *image,
			    struct fw_image_manifest_v1_8 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2)
{
	RSA *priv_rsa = NULL;
	EVP_PKEY *privkey;
	FILE *fp;
	const BIGNUM *n, *e, *d;
	unsigned char digest[SHA256_DIGEST_LENGTH];
	unsigned char mod[MAN_RSA_KEY_MODULUS_LEN];
	unsigned int siglen = MAN_RSA_SIGNATURE_LEN;
	char path[256];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	/* create new key */
	privkey = EVP_PKEY_new();
	if (!privkey)
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	if (!image->key_name)
		sprintf(path, "%s/otc_private_key.pem", PEM_KEY_PREFIX);
	else
		strcpy(path, image->key_name);

	fprintf(stdout, " pkcs: signing with key %s\n", path);
	fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "error: can't open file %s %d\n",
			path, -errno);
		return -errno;
	}
	PEM_read_PrivateKey(fp, &privkey, NULL, NULL);
	fclose(fp);

	/* validate RSA private key */
	priv_rsa = EVP_PKEY_get1_RSA(privkey);
	if (RSA_check_key(priv_rsa)) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest */
	module_sha256_create(image);
	module_sha256_update(image, ptr1, size1);
	module_sha256_update(image, ptr2, size2);
	module_sha256_complete(image, digest);

	fprintf(stdout, " pkcs: digest for manifest is ");
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
		fprintf(stdout, "%02x", digest[i]);
	fprintf(stdout, "\n");

	/* sign the manifest */
	ret = RSA_sign(NID_sha256, digest, SHA256_DIGEST_LENGTH,
		       (unsigned char *)man->css.signature,
		       &siglen, priv_rsa);
	if (ret < 0)
		fprintf(stderr, "error: failed to sign manifest\n");

	/* copy public key modulus and exponent to manifest */
	RSA_get0_key(priv_rsa, &n, &e, &d);
	BN_bn2bin(n, mod);
	BN_bn2bin(e, (unsigned char *)man->css.exponent);

	/* modulus is reveresd  */
	for (i = 0; i < MAN_RSA_KEY_MODULUS_LEN; i++)
		man->css.modulus[i] = mod[MAN_RSA_KEY_MODULUS_LEN - (1 + i)];

	/* signature is reveresd, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

	EVP_PKEY_free(privkey);
	return ret;
}

int ri_manifest_sign_v1_5(struct image *image)
{
	struct fw_image_manifest_v1_5 *man = image->fw_image;

	pkcs_v1_5_sign_man_v1_5(image, man,
				(void *)man + MAN_CSS_MAN_SIZE_V1_5,
				image->image_end - sizeof(*man));
	return 0;
}

int ri_manifest_sign_v1_8(struct image *image)
{
	struct fw_image_manifest_v1_8 *man = image->fw_image;

	pkcs_v1_5_sign_man_v1_8(image, man, (void *)man + MAN_CSS_HDR_OFFSET,
				sizeof(struct css_header_v1_8) -
				(MAN_RSA_KEY_MODULUS_LEN +
				 MAN_RSA_KEY_EXPONENT_LEN +
				 MAN_RSA_SIGNATURE_LEN),
				(void *)man + MAN_SIG_PKG_OFFSET_V1_8,
				(man->css.size - man->css.header_len)
				 * sizeof(uint32_t));
	return 0;
}
