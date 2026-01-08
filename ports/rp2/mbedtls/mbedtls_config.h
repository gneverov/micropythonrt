/**
 * \file config-suite-b.h
 *
 * \brief Minimal configuration for TLS NSA Suite B Profile (RFC 6460)
 */
/*
*  Copyright The Mbed TLS Contributors
*  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
*/
/*
* Minimal configuration for TLS NSA Suite B Profile (RFC 6460)
*
* Distinguishing features:
* - no RSA or classic DH, fully based on ECC
* - optimized for low RAM usage
*
* Possible improvements:
* - if 128-bit security is enough, disable secp384r1 and SHA-512
* - use embedded certs in DER format and disable PEM_PARSE_C and BASE64_C
*
* See README.txt for usage instructions.
*/

#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* System support */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_DEPRECATED_REMOVED

/* Mbed TLS feature support */
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_REMOVE_ARC4_CIPHERSUITES
#define MBEDTLS_REMOVE_3DES_CIPHERSUITES
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_ERROR_STRERROR_DUMMY
#define MBEDTLS_FS_IO
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_PKCS1_V15
// #define MBEDTLS_SSL_ALPN
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
// #define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
// #define MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH
#define MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK

/* Mbed TLS modules */
#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_GCM_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
// #define MBEDTLS_SSL_CACHE_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_SSL_TLS_C
// #define MBEDTLS_THREADING_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
// #define MBEDTLS_X509_CRL_PARSE_C

/* General configuration options */

/* Module configuration options */
/* MPI / BIGNUM options */
// #define MBEDTLS_MPI_MAX_SIZE    512 // 48 bytes for a 384-bit elliptic curve
/* ECP options */
#define MBEDTLS_ECP_WINDOW_SIZE        2
#define MBEDTLS_ECP_FIXED_POINT_OPTIM  0
/* Entropy options */
#define MBEDTLS_ENTROPY_MAX_SOURCES 1

// #define MBEDTLS_SSL_OUT_CONTENT_LEN             4096

/* For test certificates */
//  #define MBEDTLS_BASE64_C
//  #define MBEDTLS_CERTS_C
//  #define MBEDTLS_PEM_PARSE_C

/* Save ROM and a few bytes of RAM by specifying our own ciphersuite list */
#if 0
#define MBEDTLS_SSL_CIPHERSUITES                        \
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,    \
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
#endif

/*
* Save RAM at the expense of interoperability: do this only if you control
* both ends of the connection!  (See comments in "mbedtls/ssl.h".)
* The minimum size here depends on the certificate chain used as well as the
* typical size of records.
*/
//  #define MBEDTLS_SSL_MAX_CONTENT_LEN             1024

/* These defines are present so that the config modifying scripts can enable
* them during tests/scripts/test-ref-configs.pl */
// #define MBEDTLS_USE_PSA_CRYPTO
// #define MBEDTLS_PSA_CRYPTO_C

/* With USE_PSA_CRYPTO, some PK operations also need PK_WRITE */
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#define MBEDTLS_PK_WRITE_C
#endif

/* Error messages and TLS debugging traces
* (huge code size increase, needed for tests/ssl-opt.sh) */
#ifndef NDEBUG
#define MBEDTLS_DEBUG_C
#define MBEDTLS_ERROR_C
#endif

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */
