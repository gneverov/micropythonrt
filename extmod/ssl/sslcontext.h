// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "morelib/lwip/tls.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#include "py/obj.h"

typedef struct socket_tls_context socket_tls_context_t;

typedef struct {
    mp_obj_base_t base;
    socket_tls_context_t *context;
    int protocol:1;
    int check_hostname:1;
} mp_obj_sslcontext_t;

extern const mp_obj_type_t mp_type_sslcontext;