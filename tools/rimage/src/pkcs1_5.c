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
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <rimage/rimage.h>
#include <rimage/css.h>
#include <rimage/manifest.h>
#include <rimage/misc_utils.h>
#include <rimage/file_utils.h>
#include <rimage/hash.h>

#define DEBUG_PKCS	0

enum manver {
	V15 = 0,
	V18 = 1,
	V25 = 2,
	VACE15 = 3
};

static int rimage_read_key(EVP_PKEY **privkey, struct image *image)
{
	char path[256];
	FILE *fp;

	/* requires private key */
	if (!image->key_name) {
		fprintf(stderr, "error: no private key set \n");
		return -EINVAL;
	}

	/* create new key */
	*privkey = EVP_PKEY_new();
	if (!(*privkey))
		return -ENOMEM;

	/* load in RSA private key from PEM file */
	memset(path, 0, sizeof(path));
	strncpy(path, image->key_name, sizeof(path) - 1);

	fprintf(stdout, " %s: read key '%s'\n", __func__, path);
	fp = fopen(path, "rb");
	if (!fp)
		return file_error("unable to open file for reading", path);
	
	PEM_read_PrivateKey(fp, privkey, NULL, NULL);
	fclose(fp);

	return 0;
}

/*
 * Here we have different implementations of following functionality
 * (based on different openssl versions):
 *
 * rimage_check_key
 *
 * rimage_set_modexp
 *
 * rimage_sign
 *
 * rimage_verify
 *
 * rimage_get_key_size
 *
*/

#if OPENSSL_VERSION_NUMBER < 0x30000000L
static int rimage_check_key(EVP_PKEY *privkey)
{
	RSA *priv_rsa = NULL;

	priv_rsa = EVP_PKEY_get1_RSA(privkey);

	return RSA_check_key(priv_rsa);
}
#else
static int rimage_check_key(EVP_PKEY *privkey)
{
	EVP_PKEY_CTX *ctx;
	int ret = 0;

	ctx = EVP_PKEY_CTX_new(privkey, NULL /* no engine */);
	if (!ctx)
		return -EINVAL;

	ret = EVP_PKEY_private_check(ctx);

	EVP_PKEY_CTX_free(ctx);

	return ret;
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static void rimage_set_modexp(EVP_PKEY *privkey, unsigned char *mod, unsigned char *exp)
{
	RSA *priv_rsa = NULL;
	const BIGNUM *n;
	const BIGNUM *e;

	priv_rsa = EVP_PKEY_get1_RSA(privkey);

	n = priv_rsa->n;
	e = priv_rsa->e;

	BN_bn2bin(n, mod);
	BN_bn2bin(e, exp);
}
#elif OPENSSL_VERSION_NUMBER < 0x30000000L
static void rimage_set_modexp(EVP_PKEY *privkey, unsigned char *mod, unsigned char *exp)
{
	const BIGNUM *n;
	const BIGNUM *e;
	const BIGNUM *d;
	RSA *priv_rsa = NULL;

	priv_rsa = EVP_PKEY_get1_RSA(privkey);

	RSA_get0_key(priv_rsa, &n, &e, &d);

	BN_bn2bin(n, mod);
	BN_bn2bin(e, exp);
}
#else
static void rimage_set_modexp(EVP_PKEY *privkey, unsigned char *mod, unsigned char *exp)
{
	BIGNUM *n = NULL;
	BIGNUM *e = NULL;

	EVP_PKEY_get_bn_param(privkey, OSSL_PKEY_PARAM_RSA_N, &n);
	EVP_PKEY_get_bn_param(privkey, OSSL_PKEY_PARAM_RSA_E, &e);

	BN_bn2bin(n, mod);
	BN_bn2bin(e, exp);
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x30000000L
static int rimage_sign(EVP_PKEY *privkey, enum manver ver, struct hash_context *digest,
		       unsigned char *signature)
{
	unsigned char sig[MAN_RSA_SIGNATURE_LEN_2_5];
	unsigned int siglen = MAN_RSA_SIGNATURE_LEN;
	RSA *priv_rsa = NULL;
	int ret;

	priv_rsa = EVP_PKEY_get1_RSA(privkey);

	switch (ver) {
	case V15:
		/* fallthrough */
	case V18:
		ret = RSA_sign(NID_sha256, digest->digest, digest->digest_length,
			       signature, &siglen, priv_rsa);
		break;
	case V25:
		/* fallthrough */
	case VACE15:
		ret = RSA_padding_add_PKCS1_PSS(priv_rsa, sig, digest->digest, digest->algo,
						/* salt length */ 32);
		if (ret > 0)
			ret = RSA_private_encrypt(RSA_size(priv_rsa), sig, signature, priv_rsa,
						  RSA_NO_PADDING);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
#else
static int rimage_sign(EVP_PKEY *privkey, enum manver ver,
		       struct hash_context *digest, unsigned char *signature)
{
	EVP_PKEY_CTX *ctx = NULL;
	size_t siglen = MAN_RSA_SIGNATURE_LEN;
	int ret;

	ctx = EVP_PKEY_CTX_new(privkey, NULL /* no engine */);
	if (!ctx)
		return -ENOMEM;

	ret = EVP_PKEY_sign_init(ctx);
	if (ret <= 0)
		goto out;

	if (ver == V25 || ver == VACE15) {
		ret = EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING);
		if (ret <= 0) {
			fprintf(stderr, "error: failed to set rsa padding\n");
			goto out;
		}

		ret = EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, 32);
		if (ret <= 0) {
			fprintf(stderr, "error: failed to set saltlen\n");
			goto out;
		}

		siglen = MAN_RSA_SIGNATURE_LEN_2_5;
	}

	ret = EVP_PKEY_CTX_set_signature_md(ctx, digest->algo);
	if (ret <= 0) {
		fprintf(stderr, "error: failed to set signature algorithm\n");
		goto out;
	}

	ret = EVP_PKEY_sign(ctx, signature, &siglen, digest->digest, digest->digest_length);
	if (ret <= 0) {
		fprintf(stderr, "error: failed to sign manifest\n");
		goto out;
	}

out:
	EVP_PKEY_CTX_free(ctx);

	return ret;
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x30000000L
static int rimage_verify(EVP_PKEY *privkey, enum manver ver, struct hash_context *digest,
			 unsigned char *signature)
{
	unsigned char sig[MAN_RSA_SIGNATURE_LEN_2_5];
	unsigned int siglen = MAN_RSA_SIGNATURE_LEN;
	RSA *priv_rsa = NULL;
	char err_buf[256];
	int ret;

	priv_rsa = EVP_PKEY_get1_RSA(privkey);

	switch (ver) {
	case V15:
		/* fallthrough */
	case V18:
		ret = RSA_verify(NID_sha256, digest->digest, digest->digest_length, signature,
				 siglen, priv_rsa);

		if (ret <= 0) {
			ERR_error_string(ERR_get_error(), err_buf);
			fprintf(stderr, "error: verify %s\n", err_buf);
		}
		break;
	case V25:
		/* fallthrough */
	case VACE15:
		/* decrypt signature */
		ret = RSA_public_decrypt(RSA_size(priv_rsa), signature, sig, priv_rsa,
					 RSA_NO_PADDING);
		if (ret <= 0) {
			ERR_error_string(ERR_get_error(), err_buf);
			fprintf(stderr, "error: verify decrypt %s\n", err_buf);
			return ret;
		}

		ret = RSA_verify_PKCS1_PSS(priv_rsa, digest->digest, digest->algo, sig, 32);
		if (ret <= 0) {
			ERR_error_string(ERR_get_error(), err_buf);
			fprintf(stderr, "error: verify %s\n", err_buf);
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
#else
static int rimage_verify(EVP_PKEY *privkey, enum manver ver,struct hash_context *digest,
			 unsigned char *signature)
{
	EVP_PKEY_CTX *ctx = NULL;
	size_t siglen = MAN_RSA_SIGNATURE_LEN;
	char err_buf[256];
	int ret;

	ctx = EVP_PKEY_CTX_new(privkey, NULL /* no engine */);
	if (!ctx)
		return -ENOMEM;

	ret = EVP_PKEY_verify_init(ctx);
	if (ret <= 0)
		goto out;

	ret = EVP_PKEY_CTX_set_signature_md(ctx, digest->algo);
	if (ret <= 0) {
		ERR_error_string(ERR_get_error(), err_buf);
		fprintf(stderr, "error: set signature %s\n", err_buf);
		goto out;
	}

	switch (ver) {
	case V15:	/* fallthrough */
	case V18:
		break;

	case V25:	/* fallthrough */
	case VACE15:
		ret = EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING);
		if (ret <= 0)
			goto out;

		ret = EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, 32);
		if (ret <= 0)
			goto out;

		siglen = MAN_RSA_SIGNATURE_LEN_2_5;
		break;
	default:
		return -EINVAL;
	}

	ret = EVP_PKEY_verify(ctx, signature, siglen, digest->digest, digest->digest_length);
	if (ret <= 0) {
		ERR_error_string(ERR_get_error(), err_buf);
		fprintf(stderr, "error: verify %s\n", err_buf);
	}

out:
	EVP_PKEY_CTX_free(ctx);

	return ret;
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x30000000L
static int rimage_get_key_size(EVP_PKEY *privkey)
{
	RSA *priv_rsa = NULL;
	int key_length;

	priv_rsa = EVP_PKEY_get1_RSA(privkey);
	key_length = RSA_size(priv_rsa);

	RSA_free(priv_rsa);

	return key_length;
}
#else
static int rimage_get_key_size(EVP_PKEY *privkey)
{
	return EVP_PKEY_get_size(privkey);
}
#endif

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
	EVP_PKEY *privkey;
	struct hash_context digest;
	unsigned char mod[MAN_RSA_KEY_MODULUS_LEN];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1);
#endif

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	if (rimage_check_key(privkey) > 0) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest */
	hash_sha256_init(&digest);
	hash_update(&digest, ptr1, size1);
	ret = hash_finalize(&digest);
	if (ret)
		goto err;

	fprintf(stdout, " pkcs: digest for manifest is ");
	hash_print(&digest);

	/* sign the manifest */
	ret = rimage_sign(privkey, V15, &digest, (unsigned char *)man->css_header.signature);
	if (ret <= 0) {
		fprintf(stderr, "error: failed to sign manifest\n");
		goto err;
	}

	/* copy public key modulus and exponent to manifest */
	rimage_set_modexp(privkey, mod, (unsigned char *)man->css_header.exponent);

	/* modulus is reveresd  */
	for (i = 0; i < MAN_RSA_KEY_MODULUS_LEN; i++)
		man->css_header.modulus[i]
		= mod[MAN_RSA_KEY_MODULUS_LEN - (1 + i)];

	/* signature is reveresd, swap it */
	bytes_swap(man->css_header.signature,
		   sizeof(man->css_header.signature));

err:
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
	EVP_PKEY *privkey;
	struct hash_context digest;
	unsigned char mod[MAN_RSA_KEY_MODULUS_LEN];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	/* validate RSA private key */
	if (rimage_check_key(privkey) > 0) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest */
	hash_sha256_init(&digest);
	hash_update(&digest, ptr1, size1);
	hash_update(&digest, ptr2, size2);
	ret = hash_finalize(&digest);
	if (ret)
		goto err;

	fprintf(stdout, " pkcs: digest for manifest is ");
	hash_print(&digest);

	/* sign the manifest */
	ret = rimage_sign(privkey, V18, &digest, (unsigned char *)man->css.signature);
	if (ret <= 0) {
		fprintf(stderr, "error: failed to sign manifest\n");
		goto err;
	}

	/* copy public key modulus and exponent to manifest */
	rimage_set_modexp(privkey, mod, (unsigned char *)man->css.exponent);

	/* modulus is reveresd  */
	for (i = 0; i < MAN_RSA_KEY_MODULUS_LEN; i++)
		man->css.modulus[i] = mod[MAN_RSA_KEY_MODULUS_LEN - (1 + i)];

	/* signature is reveresd, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

err:
	EVP_PKEY_free(privkey);
	return ret;
}

int pkcs_v1_5_sign_man_v2_5(struct image *image,
			    struct fw_image_manifest_v2_5 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2)
{
	EVP_PKEY *privkey;
	struct hash_context digest;
	unsigned char mod[MAN_RSA_KEY_MODULUS_LEN_2_5];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	/* validate RSA private key */
	if (rimage_check_key(privkey) > 0) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest - SHA384 on CAVS2_5+ */
	hash_sha384_init(&digest);
	hash_update(&digest, ptr1, size1);
	hash_update(&digest, ptr2, size2);
	ret = hash_finalize(&digest);
	if (ret)
		goto err;

	fprintf(stdout, " pkcs: digest for manifest is ");
	hash_print(&digest);

	/* sign the manifest */
	ret = rimage_sign(privkey, V25, &digest, (unsigned char *)man->css.signature);
	if (ret <= 0) {
		fprintf(stderr, "error: failed to sign manifest\n");
		goto err;
	}

	/* copy public key modulus and exponent to manifest */
	rimage_set_modexp(privkey, mod, (unsigned char *)man->css.exponent);

	/* modulus is reversed  */
	for (i = 0; i < MAN_RSA_KEY_MODULUS_LEN_2_5; i++)
		man->css.modulus[i] = mod[MAN_RSA_KEY_MODULUS_LEN_2_5 - (1 + i)];

	/* signature is reversed, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

err:
	EVP_PKEY_free(privkey);
	return ret;
}

int pkcs_v1_5_sign_man_ace_v1_5(struct image *image,
				struct fw_image_manifest_ace_v1_5 *man,
				void *ptr1, unsigned int size1, void *ptr2,
				unsigned int size2)
{
	EVP_PKEY *privkey;
	struct hash_context digest;
	unsigned char mod[MAN_RSA_KEY_MODULUS_LEN_2_5];
	int ret = -EINVAL, i;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	/* validate RSA private key */
	if (rimage_check_key(privkey) > 0) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest - SHA384 on CAVS2_5+ */
	hash_sha384_init(&digest);
	hash_update(&digest, ptr1, size1);
	hash_update(&digest, ptr2, size2);
	ret = hash_finalize(&digest);
	if (ret)
		goto err;

	fprintf(stdout, " pkcs: digest for manifest is ");
	hash_print(&digest);

	/* sign the manifest */
	ret = rimage_sign(privkey, VACE15, &digest, (unsigned char *)man->css.signature);
	if (ret <= 0) {
		fprintf(stderr, "error: failed to sign manifest\n");
		goto err;
	}

	/* copy public key modulus and exponent to manifest */
	rimage_set_modexp(privkey, mod, (unsigned char *)man->css.exponent);

	/* modulus is reversed  */
	for (i = 0; i < MAN_RSA_KEY_MODULUS_LEN_2_5; i++)
		man->css.modulus[i] = mod[MAN_RSA_KEY_MODULUS_LEN_2_5 - (1 + i)];

	/* signature is reversed, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

err:
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

int ri_manifest_sign_ace_v1_5(struct image *image)
{
	struct fw_image_manifest_ace_v1_5 *man = image->fw_image;

	char *const data1 = (char *)man + MAN_CSS_HDR_OFFSET_2_5;
	unsigned const size1 =
		sizeof(struct css_header_v2_5) -
		(MAN_RSA_KEY_MODULUS_LEN_2_5 + MAN_RSA_KEY_EXPONENT_LEN +
		 MAN_RSA_SIGNATURE_LEN_2_5);

	char *const data2 = (char *)man + MAN_SIG_PKG_OFFSET_V2_5;
	unsigned const size2 =
			(man->css.size - man->css.header_len) * sizeof(uint32_t);

	return pkcs_v1_5_sign_man_ace_v1_5(image, man, data1, size1, data2, size2);
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
	EVP_PKEY *privkey;
	struct hash_context digest;
	int ret = -EINVAL;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1);
#endif

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	/* validate RSA private key */
	if (rimage_check_key(privkey) > 0) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest */
	hash_sha256_init(&digest);
	hash_update(&digest, ptr1, size1);
	ret = hash_finalize(&digest);
	if (ret)
		goto err;

	fprintf(stdout, " pkcs: digest for manifest is ");
	hash_print(&digest);

	/* signature is reversed, swap it */
	bytes_swap(man->css_header.signature,
		   sizeof(man->css_header.signature));

	/* verify */
	ret = rimage_verify(privkey, V15, &digest, (unsigned char *)man->css_header.signature);
	if (ret <= 0)
		fprintf(stderr, "error: failed to verify manifest\n");
	else
		fprintf(stdout, "pkcs: signature is valid !\n");

err:
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
	EVP_PKEY *privkey;
	struct hash_context digest;
	int ret = -EINVAL;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	/* validate RSA private key */
	if (rimage_check_key(privkey) > 0) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest */
	hash_sha256_init(&digest);
	hash_update(&digest, ptr1, size1);
	hash_update(&digest, ptr2, size2);
	ret = hash_finalize(&digest);
	if (ret)
		goto err;

	fprintf(stdout, " pkcs: digest for manifest is ");
	hash_print(&digest);

	/* signature is reveresd, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

	/* verify */
	ret = rimage_verify(privkey, V18, &digest, (unsigned char *)man->css.signature);
	if (ret <= 0)
		fprintf(stderr, "error: failed to verify manifest\n");
	else
		fprintf(stdout, "pkcs: signature is valid !\n");

err:
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
	EVP_PKEY *privkey;
	struct hash_context digest;
	int ret = -EINVAL;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	/* validate RSA private key */
	if (rimage_check_key(privkey) > 0) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest - SHA384 on CAVS2_5+ */
	hash_sha384_init(&digest);
	hash_update(&digest, ptr1, size1);
	hash_update(&digest, ptr2, size2);
	ret = hash_finalize(&digest);
	if (ret)
		goto err;

	fprintf(stdout, " pkcs: digest for manifest is ");
	hash_print(&digest);

	/* signature is reversed, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

	/* verify */
	ret = rimage_verify(privkey, V25, &digest, (unsigned char *)man->css.signature);

	if (ret <= 0)
		fprintf(stderr, "error: failed to verify manifest\n");
	else
		fprintf(stdout, "pkcs: signature is valid !\n");

err:
	EVP_PKEY_free(privkey);
	return ret;
}

int pkcs_v1_5_verify_man_ace_v1_5(struct image *image,
				  struct fw_image_manifest_ace_v1_5 *man,
				  void *ptr1, unsigned int size1, void *ptr2,
				  unsigned int size2)
{
	EVP_PKEY *privkey;
	struct hash_context digest;
	int ret = -EINVAL;

#if DEBUG_PKCS
	fprintf(stdout, "offsets 0x%lx size 0x%x offset 0x%lx size 0x%x\n",
		ptr1 - (void *)man, size1, ptr2 - (void *)man, size2);
#endif

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	/* validate RSA private key */
	if (rimage_check_key(privkey) > 0) {
		fprintf(stdout, " pkcs: RSA private key is valid.\n");
	} else {
		fprintf(stderr, "error: validating RSA private key.\n");
		return -EINVAL;
	}

	/* calculate the digest - SHA384 on CAVS2_5+ */
	hash_sha384_init(&digest);
	hash_update(&digest, ptr1, size1);
	hash_update(&digest, ptr2, size2);
	ret = hash_finalize(&digest);
	if (ret)
		goto err;

	fprintf(stdout, " pkcs: digest for manifest is ");
	hash_print(&digest);

	/* signature is reversed, swap it */
	bytes_swap(man->css.signature, sizeof(man->css.signature));

	/* verify */
	ret = rimage_verify(privkey, VACE15, &digest, (unsigned char *)man->css.signature);
	if (ret <= 0)
		fprintf(stderr, "error: failed to verify manifest\n");
	else
		fprintf(stdout, "pkcs: signature is valid !\n");

err:
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
	EVP_PKEY *privkey;
	int key_len;
	int ret;

	ret = rimage_read_key(&privkey, image);
	if (ret < 0)
		return ret;

	key_len = rimage_get_key_size(privkey);

	EVP_PKEY_free(privkey);

	return key_len;
}
