// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "mbedtls/x509_crt.h"

#include "py/obj.h"


extern const mp_obj_type_t mp_type_sslerror;
extern const mp_obj_type_t mp_type_ssl_read_error;
extern const mp_obj_type_t mp_type_ssl_write_error;
extern const mp_obj_type_t mp_type_certificate_error;

void mp_ssl_check_ret(int err);

mp_obj_t mp_ssl_make_cert(const mbedtls_x509_crt *cert, bool binary_form);
