// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

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

#include <rimage/rimage.h>
#include <rimage/css.h>
#include <rimage/manifest.h>

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

	/* requires private key */
	if (!image->key_name) {
		return -EINVAL;
	}

	/* create new key */
	privkey = EVP_PKEY_new();
	if (!privkey)
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	memset(path, 0, sizeof(path));
	strncpy(path, image->key_name, sizeof(path) - 1);

	fprintf(stdout, " %s: signing with key '%s'\n", __func__, path);
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
	module_sha_update(image, ptr1, size1);
	module_sha_complete(image, digest);

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

	/* require private key */
	if (!image->key_name) {
		return -EINVAL;
	}

	/* create new key */
	privkey = EVP_PKEY_new();
	if (!privkey)
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	memset(path, 0, sizeof(path));
	strncpy(path, image->key_name, sizeof(path) - 1);

	fprintf(stdout, " %s: signing with key '%s'\n", __func__, path);
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
	module_sha_update(image, ptr1, size1);
	module_sha_update(image, ptr2, size2);
	module_sha_complete(image, digest);

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

/*
 * RSA signature of manifest. The signature is an RSA PSS
 * of the entire manifest structure, including all
 * extensions, and excluding the last 3 fields of the
 * manifest header (Public Key, Exponent and Signature).
 */

int pkcs_v1_5_sign_man_v2_5(struct image *image,
			    struct fw_image_manifest_v2_5 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2)
{
	RSA *priv_rsa = NULL;
	EVP_PKEY *privkey;
	FILE *fp;
	const BIGNUM *n, *e, *d;
	unsigned char digest[SHA384_DIGEST_LENGTH];
	unsigned char mod[MAN_RSA_KEY_MODULUS_LEN_2_5];
	unsigned char sig[MAN_RSA_SIGNATURE_LEN_2_5];
	char path[256];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	/* require private key */
	if (!image->key_name) {
		return -EINVAL;
	}

	/* create new PSS key */
	privkey = EVP_PKEY_new();
	if (!privkey)
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	memset(path, 0, sizeof(path));
	strncpy(path, image->key_name, sizeof(path) - 1);

	fprintf(stdout, " %s: signing with key '%s'\n", __func__, path);
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

	/* calculate the digest - SHA384 on CAVS2_5+ */
	module_sha384_create(image);
	module_sha_update(image, ptr1, size1);
	module_sha_update(image, ptr2, size2);
	module_sha_complete(image, digest);

	fprintf(stdout, " pkcs: digest for manifest is ");
	for (i = 0; i < SHA384_DIGEST_LENGTH; i++)
		fprintf(stdout, "%02x", digest[i]);
	fprintf(stdout, "\n");

	/* sign the manifest */
	ret = RSA_padding_add_PKCS1_PSS(priv_rsa, sig,
			digest, image->md, /* salt length */ 32);
	if (ret <= 0) {
		ERR_error_string(ERR_get_error(), path);
		fprintf(stderr, "error: failed to sign manifest %s\n", path);
	}

	/* encrypt the signature using the private key */
	ret = RSA_private_encrypt(RSA_size(priv_rsa), sig,
		     (unsigned char *)man->css.signature, priv_rsa, RSA_NO_PADDING);
	if (ret <= 0) {
		ERR_error_string(ERR_get_error(), path);
		fprintf(stderr, "error: failed to encrypt signature %s\n", path);
	}

	/* copy public key modulus and exponent to manifest */
	RSA_get0_key(priv_rsa, &n, &e, &d);
	BN_bn2bin(n, mod);
	BN_bn2bin(e, (unsigned char *)man->css.exponent);

	/* modulus is reversed  */
	for (i = 0; i < MAN_RSA_KEY_MODULUS_LEN_2_5; i++)
		man->css.modulus[i] = mod[MAN_RSA_KEY_MODULUS_LEN_2_5 - (1 + i)];

	/* signature is reversed, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

	EVP_PKEY_free(privkey);
	return ret;
}

int ri_manifest_sign_v1_5(struct image *image)
{
	struct fw_image_manifest_v1_5 *man = image->fw_image;

	/* excluding the manifest header */
	char *const data1 = (char *)man + sizeof(struct fw_image_manifest_v1_5);
	unsigned const size1 = image->image_end - sizeof(*man);

	return pkcs_v1_5_sign_man_v1_5(image, man, data1, size1);
}

int ri_manifest_sign_v1_8(struct image *image)
{
	struct fw_image_manifest_v1_8 *man = image->fw_image;

	char *const data1 = (char *)man + MAN_CSS_HDR_OFFSET;
	unsigned const size1 =
		sizeof(struct css_header_v1_8) -
		(MAN_RSA_KEY_MODULUS_LEN + MAN_RSA_KEY_EXPONENT_LEN +
		 MAN_RSA_SIGNATURE_LEN);

	char *const data2 = (char *)man + MAN_SIG_PKG_OFFSET_V1_8;
	unsigned const size2 =
		(man->css.size - man->css.header_len) * sizeof(uint32_t);

	return pkcs_v1_5_sign_man_v1_8(image, man, data1, size1, data2, size2);
}

int ri_manifest_sign_v2_5(struct image *image)
{
	struct fw_image_manifest_v2_5 *man = image->fw_image;

	char *const data1 = (char *)man + MAN_CSS_HDR_OFFSET_2_5;
	unsigned const size1 =
		sizeof(struct css_header_v2_5) -
		(MAN_RSA_KEY_MODULUS_LEN_2_5 + MAN_RSA_KEY_EXPONENT_LEN +
		 MAN_RSA_SIGNATURE_LEN_2_5);

	char *const data2 = (char *)man + MAN_SIG_PKG_OFFSET_V2_5;
	unsigned const size2 =
			(man->css.size - man->css.header_len) * sizeof(uint32_t);

	return pkcs_v1_5_sign_man_v2_5(image, man, data1, size1, data2, size2);
}

/*
 * RSA verify of manifest. The signature is an PKCS
 * #1-v1_5 of the entire manifest structure, including all
 * extensions, and excluding the last 3 fields of the
 * manifest header (Public Key, Exponent and Signature).
 */

int pkcs_v1_5_verify_man_v1_5(struct image *image,
			    struct fw_image_manifest_v1_5 *man,
			    void *ptr1, unsigned int size1)
{
	RSA *priv_rsa = NULL;
	EVP_PKEY *privkey;
	FILE *fp;
	unsigned char digest[SHA256_DIGEST_LENGTH];
	unsigned int siglen = MAN_RSA_SIGNATURE_LEN;
	char path[256];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1);
#endif

	/* requires private key */
	if (!image->key_name) {
		return -EINVAL;
	}

	/* create new key */
	privkey = EVP_PKEY_new();
	if (!privkey)
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	memset(path, 0, sizeof(path));
	strncpy(path, image->key_name, sizeof(path) - 1);

	fprintf(stdout, " pkcs: verify with key %s\n", path);
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
	module_sha_update(image, ptr1, size1);
	module_sha_complete(image, digest);

	fprintf(stdout, " pkcs: digest for manifest is ");
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
		fprintf(stdout, "%02x", digest[i]);
	fprintf(stdout, "\n");

	/* signature is reversed, swap it */
	bytes_swap(man->css_header.signature,
		   sizeof(man->css_header.signature));

	/* sign the manifest */
	ret = RSA_verify(NID_sha256, digest, SHA256_DIGEST_LENGTH,
		       (unsigned char *)man->css_header.signature,
		       siglen, priv_rsa);
	if (ret <= 0) {
		ERR_error_string(ERR_get_error(), path);
		fprintf(stderr, "error: failed to verify manifest %s\n", path);
	} else
		fprintf(stdout, "pkcs: signature is valid !\n");


	EVP_PKEY_free(privkey);
	return ret;
}

/*
 * RSA verify of manifest. The signature is an PKCS
 * #1-v1_5 of the entire manifest structure, including all
 * extensions, and excluding the last 3 fields of the
 * manifest header (Public Key, Exponent and Signature).
 */

int pkcs_v1_5_verify_man_v1_8(struct image *image,
			    struct fw_image_manifest_v1_8 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2)
{
	RSA *priv_rsa = NULL;
	EVP_PKEY *privkey;
	FILE *fp;
	unsigned char digest[SHA256_DIGEST_LENGTH];
	unsigned int siglen = MAN_RSA_SIGNATURE_LEN;
	char path[256];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	/* require private key */
	if (!image->key_name) {
		return -EINVAL;
	}

	/* create new key */
	privkey = EVP_PKEY_new();
	if (!privkey)
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	memset(path, 0, sizeof(path));
	strncpy(path, image->key_name, sizeof(path) - 1);

	fprintf(stdout, " pkcs: verifying with key %s\n", path);
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
	module_sha_update(image, ptr1, size1);
	module_sha_update(image, ptr2, size2);
	module_sha_complete(image, digest);

	fprintf(stdout, " pkcs: digest for manifest is ");
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
		fprintf(stdout, "%02x", digest[i]);
	fprintf(stdout, "\n");

	/* signature is reveresd, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

	/* sign the manifest */
	ret = RSA_verify(NID_sha256, digest, SHA256_DIGEST_LENGTH,
		       (unsigned char *)man->css.signature,
		       siglen, priv_rsa);
	if (ret <= 0) {
		ERR_error_string(ERR_get_error(), path);
		fprintf(stderr, "error: failed to verify manifest %s\n", path);
	} else
		fprintf(stdout, "pkcs: signature is valid !\n");

	EVP_PKEY_free(privkey);
	return ret;
}

/*
 * RSA signature of manifest. The signature is an RSA PSS
 * of the entire manifest structure, including all
 * extensions, and excluding the last 3 fields of the
 * manifest header (Public Key, Exponent and Signature).
 */

int pkcs_v1_5_verify_man_v2_5(struct image *image,
			    struct fw_image_manifest_v2_5 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2)
{
	RSA *priv_rsa = NULL;
	EVP_PKEY *privkey;
	FILE *fp;
	unsigned char digest[SHA384_DIGEST_LENGTH];
	unsigned char sig[MAN_RSA_SIGNATURE_LEN_2_5];
	char path[256];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	/* require private key */
	if (!image->key_name) {
		return -EINVAL;
	}

	/* create new PSS key */
	privkey = EVP_PKEY_new();
	if (!privkey)
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	memset(path, 0, sizeof(path));
	strncpy(path, image->key_name, sizeof(path) - 1);

	fprintf(stdout, " pkcs: PSS verify with key %s\n", path);
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

	/* calculate the digest - SHA384 on CAVS2_5+ */
	module_sha384_create(image);
	module_sha_update(image, ptr1, size1);
	module_sha_update(image, ptr2, size2);
	module_sha_complete(image, digest);

	fprintf(stdout, " pkcs: digest for manifest is ");
	for (i = 0; i < SHA384_DIGEST_LENGTH; i++)
		fprintf(stdout, "%02x", digest[i]);
	fprintf(stdout, "\n");

	/* signature is reversed, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

	/* decrypt signature */
	ret = RSA_public_decrypt(RSA_size(priv_rsa), (unsigned char *)man->css.signature,
			     sig, priv_rsa, RSA_NO_PADDING);
	if (ret <= 0) {
		ERR_error_string(ERR_get_error(), path);
		fprintf(stderr, "error: failed to decrypt signature %s\n", path);
	}

	ret = RSA_verify_PKCS1_PSS(priv_rsa, digest, image->md, sig, 32);
	if (ret <= 0) {
		ERR_error_string(ERR_get_error(), path);
		fprintf(stderr, "error: failed to verify manifest %s\n", path);
	} else
		fprintf(stdout, "pkcs: signature is valid !\n");

	EVP_PKEY_free(privkey);
	return ret;
}

int ri_manifest_verify_v1_5(struct image *image)
{
	struct fw_image_manifest_v1_5 *man = image->fw_image;

	char *const data1 = (char *)man + MAN_CSS_MAN_SIZE_V1_5;
	unsigned const size1 = image->image_end - sizeof(*man);

	return pkcs_v1_5_verify_man_v1_5(image, man, data1, size1);
}

int ri_manifest_verify_v1_8(struct image *image)
{
	struct fw_image_manifest_v1_8 *man = image->fw_image;

	char *const data1 = (char *)man + MAN_CSS_HDR_OFFSET;
	unsigned const size1 =
		sizeof(struct css_header_v1_8) -
		(MAN_RSA_KEY_MODULUS_LEN + MAN_RSA_KEY_EXPONENT_LEN +
		 MAN_RSA_SIGNATURE_LEN);

	char *const data2 = (char *)man + MAN_SIG_PKG_OFFSET_V1_8;
	unsigned const size2 =
		(man->css.size - man->css.header_len) * sizeof(uint32_t);

	return pkcs_v1_5_verify_man_v1_8(image, man, data1, size1, data2, size2);
}

int ri_manifest_verify_v2_5(struct image *image)
{
	struct fw_image_manifest_v2_5 *man = image->fw_image;

	char *const data1 = (char *)man + MAN_CSS_HDR_OFFSET_2_5;
	unsigned const size1 =
		sizeof(struct css_header_v2_5) -
		(MAN_RSA_KEY_MODULUS_LEN_2_5 + MAN_RSA_KEY_EXPONENT_LEN +
		 MAN_RSA_SIGNATURE_LEN_2_5);

	char *const data2 = (char *)man + MAN_SIG_PKG_OFFSET_V2_5;
	unsigned const size2 =
			(man->css.size - man->css.header_len) * sizeof(uint32_t);

	return pkcs_v1_5_verify_man_v2_5(image, man, data1, size1, data2, size2);
}

int get_key_size(struct image *image)
{
	RSA *priv_rsa = NULL;
	EVP_PKEY *privkey;
	FILE *fp;
	int key_length;
	char path[256];

	/* require private key */
	if (!image->key_name) {
		return -EINVAL;
	}

	/* load in RSA private key from PEM file */
	memset(path, 0, sizeof(path));
	strncpy(path, image->key_name, sizeof(path) - 1);

	fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "error: can't open file %s %d\n",
			path, -errno);
		return -errno;
	}
	PEM_read_PrivateKey(fp, &privkey, NULL, NULL);
	fclose(fp);

	priv_rsa = EVP_PKEY_get1_RSA(privkey);
	key_length = RSA_size(priv_rsa);

	RSA_free(priv_rsa);
	EVP_PKEY_free(privkey);

	return key_length;
}
