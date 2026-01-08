// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "mbedtls/error.h"
#include "mbedtls/version.h"

#include "extmod/ssl/modssl.h"
#include "extmod/ssl/sslcontext.h"
#include "extmod/ssl/sslsocket.h"
#include "py/extras.h"
#include "py/objstr.h"
#include "py/parseargs.h"
#include "py/runtime.h"


__attribute__((visibility("hidden")))
void mp_ssl_check_ret(int ret) {
    if (ret >= 0) {
        return;
    }
    vstr_t strerror;
    vstr_init(&strerror, 256);
    mbedtls_strerror(ret, vstr_str(&strerror), 256);
    vstr_add_str(&strerror, vstr_str(&strerror));
    mp_obj_t args[] = {
        mp_obj_new_int(-ret),
        mp_obj_new_str_from_vstr(&strerror),
    };
    const mp_obj_type_t *type;
    switch (ret) {
        // case MBEDTLS_ERR_SSL_CONN_EOF:
        //     type = &mp_type_ssl_eof_error;
        //     break;
        case MBEDTLS_ERR_SSL_WANT_READ:
            type = &mp_type_ssl_read_error;
            break;
        case MBEDTLS_ERR_SSL_WANT_WRITE:
            type = &mp_type_ssl_write_error;
            break;        
        case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
            type = &mp_type_certificate_error;
            break;
        default:
            type = &mp_type_sslerror;
            break;
    }
    mp_obj_t exc = mp_obj_exception_make_new(type, 2, 0, args);
    nlr_raise(exc);
}

static mp_obj_t mp_ssl_create_default_context(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_purpose, MP_QSTR_cafile, MP_QSTR_capath, MP_QSTR_cadata, 0 };
    mp_obj_t purpose = MP_OBJ_NEW_SMALL_INT(MBEDTLS_SSL_IS_CLIENT);
    mp_obj_t cafile = mp_const_none;
    mp_obj_t capath = mp_const_none;
    mp_obj_t cadata = mp_const_none;
    parse_args_and_kw_map(n_args, args, kw_args, "|OOOO", kws, &purpose, &cafile, &capath, &cadata);

    mp_obj_t make_new_args[1] = { purpose };
    mp_obj_t ssl_ctx = mp_obj_make_new(&mp_type_sslcontext, 1, 0, make_new_args);
 
    if ((cafile != mp_const_none) || (capath != mp_const_none) || (cadata != mp_const_none)) {
        mp_obj_t load_args[5] = { MP_OBJ_NULL, MP_OBJ_NULL, cafile, capath, cadata };
        mp_load_method(ssl_ctx, MP_QSTR_load_verify_locations, load_args);
        mp_call_method_n_kw(3, 0, load_args);
    }
    else {
        mp_obj_t load_args[3] = { MP_OBJ_NULL, MP_OBJ_NULL, purpose };
        mp_load_method(ssl_ctx, MP_QSTR_load_default_certs, load_args);
        mp_call_method_n_kw(1, 0, load_args);
    }

    return ssl_ctx;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_ssl_create_default_context_obj, 0, mp_ssl_create_default_context);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_sslerror,
    MP_QSTR_SSLError,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new,
    print, mp_obj_exception_print,
    attr, mp_obj_exception_attr,    
    parent, &mp_type_OSError
    );

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_ssl_read_error,
    MP_QSTR_SSLWantReadError,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new,
    print, mp_obj_exception_print,
    attr, mp_obj_exception_attr,    
    parent, &mp_type_sslerror
    );

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_ssl_write_error,
    MP_QSTR_SSLWantWriteError,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new,
    print, mp_obj_exception_print,
    attr, mp_obj_exception_attr,    
    parent, &mp_type_sslerror
    );

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_certificate_error,
    MP_QSTR_SSLCertVerificationError,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new,
    print, mp_obj_exception_print,
    attr, mp_obj_exception_attr,    
    parent, &mp_type_sslerror
    );

static const mp_rom_map_elem_t mp_ssl_purpose_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_SERVER_AUTH),     MP_ROM_INT(MBEDTLS_SSL_IS_CLIENT) },
    { MP_ROM_QSTR(MP_QSTR_CLIENT_AUTH),     MP_ROM_INT(MBEDTLS_SSL_IS_SERVER) },
};
static MP_DEFINE_CONST_DICT(mp_ssl_purpose_locals_dict, mp_ssl_purpose_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_ssl_purpose,
    MP_QSTR_Purpose,
    MP_TYPE_FLAG_NONE,
    locals_dict, &mp_ssl_purpose_locals_dict
    );

static const MP_DEFINE_STR_OBJ(mp_ssl_openssl_version, MBEDTLS_VERSION_STRING_FULL);

static const mp_obj_tuple_t mp_ssl_openssl_version_info = {
    { &mp_type_tuple }, 
    5, 
    { 
        MP_ROM_INT(MBEDTLS_VERSION_MAJOR), 
        MP_ROM_INT(MBEDTLS_VERSION_MINOR), 
        MP_ROM_INT(MBEDTLS_VERSION_PATCH), 
        MP_ROM_INT(0), 
        MP_ROM_INT(0),
    },
};

static const mp_rom_map_elem_t mp_module_ssl_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),                    MP_ROM_QSTR(MP_QSTR_ssl) },
    { MP_ROM_QSTR(MP_QSTR_CertificateError),            MP_ROM_PTR(&mp_type_certificate_error) },
    { MP_ROM_QSTR(MP_QSTR_SSLCertVerificationError),    MP_ROM_PTR(&mp_type_certificate_error) },
    { MP_ROM_QSTR(MP_QSTR_SSLContext),                  MP_ROM_PTR(&mp_type_sslcontext) },
    { MP_ROM_QSTR(MP_QSTR_SSLError),                    MP_ROM_PTR(&mp_type_sslerror) },
    { MP_ROM_QSTR(MP_QSTR_SSLSocket),                   MP_ROM_PTR(&mp_type_sslsocket) },
    { MP_ROM_QSTR(MP_QSTR_SSLWantReadError),            MP_ROM_PTR(&mp_type_ssl_read_error) },
    { MP_ROM_QSTR(MP_QSTR_SSLWantWriteError),           MP_ROM_PTR(&mp_type_ssl_write_error) },

    { MP_ROM_QSTR(MP_QSTR_create_default_context),      MP_ROM_PTR(&mp_ssl_create_default_context_obj) },

    { MP_ROM_QSTR(MP_QSTR_Purpose),                     MP_ROM_PTR(&mp_type_ssl_purpose) },
    { MP_ROM_QSTR(MP_QSTR_PROTOCOL_TLS_CLIENT),         MP_ROM_INT(MBEDTLS_SSL_IS_CLIENT) },
    { MP_ROM_QSTR(MP_QSTR_PROTOCOL_TLS_SERVER),         MP_ROM_INT(MBEDTLS_SSL_IS_SERVER) },
    { MP_ROM_QSTR(MP_QSTR_CERT_NONE),                   MP_ROM_INT(MBEDTLS_SSL_VERIFY_NONE) },
    { MP_ROM_QSTR(MP_QSTR_CERT_OPTIONAL),               MP_ROM_INT(MBEDTLS_SSL_VERIFY_OPTIONAL) },
    { MP_ROM_QSTR(MP_QSTR_CERT_REQUIRED),               MP_ROM_INT(MBEDTLS_SSL_VERIFY_REQUIRED) },

    { MP_ROM_QSTR(MP_QSTR_OP_ALL),                      MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_OP_CIPHER_SERVER_PREFERENCE),  MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_OP_NO_COMPRESSION),           MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_OP_NO_TICKET),                MP_ROM_INT(0) },

    #ifdef MBEDTLS_SSL_ALPN
    { MP_ROM_QSTR(MP_QSTR_HAS_ALPN),                    MP_ROM_TRUE },
    #else
    { MP_ROM_QSTR(MP_QSTR_HAS_ALPN),                    MP_ROM_FALSE },
    #endif

    #ifdef MBEDTLS_ECDH_C
    { MP_ROM_QSTR(MP_QSTR_HAS_ECDH),                    MP_ROM_TRUE },
    { MP_ROM_QSTR(MP_QSTR_OP_SINGLE_ECDH_USE),          MP_ROM_INT(0) },
    #else
    { MP_ROM_QSTR(MP_QSTR_HAS_ECDH),                    MP_ROM_FALSE },
    #endif
    
    #ifdef MBEDTLS_SSL_SERVER_NAME_INDICATION
    { MP_ROM_QSTR(MP_QSTR_HAS_SNI),                     MP_ROM_FALSE },
    #else
    { MP_ROM_QSTR(MP_QSTR_HAS_SNI),                     MP_ROM_FALSE },
    #endif

    { MP_ROM_QSTR(MP_QSTR_HAS_NPN),                     MP_ROM_FALSE },

    { MP_ROM_QSTR(MP_QSTR_HAS_TLSv1_2),                 MP_ROM_TRUE },
    { MP_ROM_QSTR(MP_QSTR_HAS_TLSv1_3),                 MP_ROM_FALSE },

    { MP_ROM_QSTR(MP_QSTR_CHANNEL_BINDING_TYPES),       MP_ROM_PTR(&mp_const_empty_tuple_obj) },
    { MP_ROM_QSTR(MP_QSTR_OPENSSL_VERSION),             MP_ROM_PTR(&mp_ssl_openssl_version) },
    { MP_ROM_QSTR(MP_QSTR_OPENSSL_VERSION_INFO),        MP_ROM_PTR(&mp_ssl_openssl_version_info) },
    { MP_ROM_QSTR(MP_QSTR_OPENSSL_VERSION_NUMBER),      MP_ROM_INT(MBEDTLS_VERSION_NUMBER) },
};
static MP_DEFINE_CONST_DICT(mp_module_ssl_globals, mp_module_ssl_globals_table);

const mp_obj_module_t mp_module_ssl = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_ssl_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ssl, mp_module_ssl);
