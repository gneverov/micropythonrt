// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/platform.h"
#include "mbedtls/x509_crt.h"

#include "extmod/modos_newlib.h"
#include "extmod/ssl/modssl.h"
#include "extmod/ssl/sslcontext.h"
#include "extmod/ssl/sslsocket.h"
#include "py/extras.h"
#include "py/parseargs.h"
#include "py/runtime.h"


static socket_tls_context_t *mp_sslcontext_get(mp_obj_t self_in) {
    mp_obj_sslcontext_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->context) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed sslcontext"));
    }
    return self->context;
}

static mp_obj_t mp_sslcontext_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    int endpoint = mp_obj_get_int(args[0]);
    if ((endpoint != MBEDTLS_SSL_IS_CLIENT) && (endpoint != MBEDTLS_SSL_IS_SERVER)) {
        mp_raise_ValueError(NULL);
    }

    // Allocate the SSL context
    mp_obj_sslcontext_t *self = mp_obj_malloc_with_finaliser(mp_obj_sslcontext_t, &mp_type_sslcontext);
    self->context = socket_tls_context_alloc(endpoint);
    if (!self->context) {
        mp_raise_OSError(errno);
    }
    self->protocol = endpoint;
    self->check_hostname = !endpoint;
    return MP_OBJ_FROM_PTR(self);
}

static void mp_sslcontext_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    switch (attr) {
        case MP_QSTR_check_hostname: {
            mp_obj_sslcontext_t *self = MP_OBJ_TO_PTR(self_in);
            socket_tls_context_t *context = mp_sslcontext_get(self_in);
            if (dest[0] != MP_OBJ_SENTINEL) {
                dest[0] = mp_obj_new_bool(self->check_hostname);
            } else if(dest[1] != MP_OBJ_NULL) {
                self->check_hostname = mp_obj_is_true(dest[1]);
                if (self->check_hostname && (context->conf.private_authmode == MBEDTLS_SSL_VERIFY_NONE)) {
                    mbedtls_ssl_conf_authmode(&context->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
                }
                dest[0] = MP_OBJ_NULL;
            }
            break;

        }
        case MP_QSTR_protocol: {
            mp_obj_sslcontext_t *self = MP_OBJ_TO_PTR(self_in);
            if (dest[0] != MP_OBJ_SENTINEL) {
                dest[0] = MP_OBJ_NEW_SMALL_INT(self->protocol & 1);
            }
            break;
        }
        case MP_QSTR_verify_mode: {
            mp_obj_sslcontext_t *self = MP_OBJ_TO_PTR(self_in);
            socket_tls_context_t *context = mp_sslcontext_get(self_in);
            if (dest[0] != MP_OBJ_SENTINEL) {
                dest[0] = MP_OBJ_NEW_SMALL_INT(context->conf.private_authmode);
            } else if(dest[1] != MP_OBJ_NULL) {
                mp_int_t value = mp_obj_get_int(dest[1]);
                if (
                    (value < MBEDTLS_SSL_VERIFY_NONE) || 
                    (value > MBEDTLS_SSL_VERIFY_REQUIRED) || 
                    (self->check_hostname && (value == MBEDTLS_SSL_VERIFY_NONE))) {
                    mp_raise_ValueError(NULL);
                }
                mbedtls_ssl_conf_authmode(&context->conf, mp_obj_get_int(dest[1]));
                dest[0] = MP_OBJ_NULL;
            }
            break;
        }
        default: {
            dest[1] = MP_OBJ_SENTINEL;
            break;
        }
    }
}

static mp_obj_t mp_sslcontext_del(mp_obj_t self_in) {
    mp_obj_sslcontext_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->context) {
        socket_tls_context_free(self->context);
        self->context = NULL;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslcontext_del_obj, mp_sslcontext_del);

static mp_obj_t mp_sslcontext_cert_store_stats(mp_obj_t self_in) {
    socket_tls_context_t *context = mp_sslcontext_get(self_in);
    mp_int_t num_certs = 0;
    mp_int_t num_ca_certs = 0;
#ifdef MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK
    const struct socket_tls_ca_cert *ca_cert = context->ca_chain;
    while (ca_cert) {
        mbedtls_x509_crt cert;
        mbedtls_x509_crt_init(&cert);
        int ret = mbedtls_x509_crt_parse_der_nocopy(&cert, ca_cert->buf, ca_cert->len);
        if (ret == 0) {
            num_certs++;
            num_ca_certs += cert.private_ca_istrue ? 1 : 0;
        }
        mbedtls_x509_crt_free(&cert);
        ca_cert = ca_cert->next;
    }
#else
    const mbedtls_x509_crt *cert = context->ca_chain;
    while (cert && cert->version) {
        num_certs++;
        num_ca_certs += cert.ca_istrue ? 1 : 0;
        cert = cert->next;
    }
#endif

    mp_obj_t dict = mp_obj_new_dict(3);
    mp_obj_dict_store(dict, mp_obj_new_str_from_cstr("x509"), mp_obj_new_int(num_certs));
    mp_obj_dict_store(dict, mp_obj_new_str_from_cstr("x509_ca"), mp_obj_new_int(num_ca_certs));
    mp_obj_dict_store(dict, mp_obj_new_str_from_cstr("crl"), MP_OBJ_NEW_SMALL_INT(0));
    return dict;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslcontext_cert_store_stats_obj, mp_sslcontext_cert_store_stats);

static mp_obj_t mp_sslcontext_load_cert_chain(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    socket_tls_context_t *context = mp_sslcontext_get(args[0]);
    const qstr kws[] = { MP_QSTR_certfile, MP_QSTR_keyfile, MP_QSTR_password, 0 };
    const char *certfile;
    const char *keyfile = NULL;
    const char *password = NULL;
    parse_args_and_kw_map(n_args - 1, args + 1, kw_args, "s|zz", kws, &certfile, &keyfile, &password);

    int ret = socket_tls_load_cert_chain(context, certfile, keyfile ? keyfile : certfile, password);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_sslcontext_load_cert_chain_obj, 1, mp_sslcontext_load_cert_chain);

static mp_obj_t mp_sslcontext_get_ca_certs(size_t n_args, const mp_obj_t *args) {
    socket_tls_context_t *context = mp_sslcontext_get(args[0]);
    bool binary_form = (n_args > 1) && mp_obj_is_true(args[1]);
    mp_obj_t list = mp_obj_new_list(0, NULL);
#ifdef MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK
    const struct socket_tls_ca_cert *ca_cert = context->ca_chain;
    while (ca_cert) {
        mbedtls_x509_crt cert;
        mbedtls_x509_crt_init(&cert);
        int ret = mbedtls_x509_crt_parse_der_nocopy(&cert, ca_cert->buf, ca_cert->len);
        if (ret == 0) {
            mp_obj_t cert_obj = mp_ssl_make_cert(&cert, binary_form);
            mp_obj_list_append(list, cert_obj);
        }
        mbedtls_x509_crt_free(&cert);
        ca_cert = ca_cert->next;
    }
#else
    const mbedtls_x509_crt *cert = context->ca_chain;
    while (cert && cert->version) {
        mp_obj_t cert_obj = mp_ssl_make_cert(cert, binary_form);
        mp_obj_list_append(list, cert_obj);
        cert = cert->next;
    }
#endif
    return list;    
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_sslcontext_get_ca_certs_obj, 1, 2, mp_sslcontext_get_ca_certs);

static mp_obj_t mp_sslcontext_get_ciphers(mp_obj_t self_in) {
    socket_tls_context_t *context = mp_sslcontext_get(self_in);
    mp_obj_t list = mp_obj_new_list(0, NULL);
    const int *cipher_id = context->conf.private_ciphersuite_list;
    assert(cipher_id);
    while (*cipher_id) {
        const mbedtls_ssl_ciphersuite_t *cipher = mbedtls_ssl_ciphersuite_from_id(*cipher_id++);
        mp_obj_t dict = mp_obj_new_dict(10);
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_id), MP_OBJ_NEW_SMALL_INT(mbedtls_ssl_ciphersuite_get_id(cipher)));
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_name), mp_obj_new_str_from_cstr(mbedtls_ssl_ciphersuite_get_name(cipher))); 
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_protocol), mp_obj_new_str_from_cstr("TLSv1.2"));

        mp_int_t bits = -1;
        mp_int_t aead = -1;
        qstr symmetric = 0;
        switch (cipher->private_cipher) {
            // case MBEDTLS_CIPHER_NULL:
            //     bits = 0;
            //     aead = 0;
            //     symmetric = MP_QSTR_null;
            //     break;
            case MBEDTLS_CIPHER_AES_128_GCM:
                bits = 128;
                aead = 1;
                symmetric = MP_QSTR_aes_128_gcm;
                break;
            // case MBEDTLS_CIPHER_AES_192_GCM:
            //     bits = 192;
            //     aead = 1;
            //     symmetric = MP_QSTR_aes_192_gcm;
            //     break;
            case MBEDTLS_CIPHER_AES_256_GCM:
                bits = 256;
                aead = 1;
                symmetric = MP_QSTR_aes_256_gcm;
                break;
            default:
                break;
        }
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_strength_bits), (bits >= 0) ? MP_OBJ_NEW_SMALL_INT(bits) : mp_const_none);
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_alg_bits), (bits >= 0) ? MP_OBJ_NEW_SMALL_INT(bits) : mp_const_none);
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_aead), (aead >= 0) ? mp_obj_new_bool(aead) : mp_const_none);
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_symmetric), symmetric ? MP_OBJ_NEW_QSTR(symmetric) : mp_const_none);

        qstr digest = 0;
        switch (cipher->private_mac) {
            case MBEDTLS_MD_SHA256:
                digest = MP_QSTR_sha256;
                break;
            case MBEDTLS_MD_SHA384:
                digest = MP_QSTR_sha384;
                break;
            case MBEDTLS_MD_SHA512:
                digest = MP_QSTR_sha512;
                break;
            default:
                break;
        }
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_digest), digest ? MP_OBJ_NEW_QSTR(digest) : mp_const_none);

        qstr kea = 0;
        qstr auth = 0;
        switch (cipher->private_key_exchange) {
            case MBEDTLS_KEY_EXCHANGE_ECDHE_RSA:
                kea = MP_QSTR_ecdhe;
                auth = MP_QSTR_rsa;
                break;
            case MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA:
                kea = MP_QSTR_ecdhe;
                auth = MP_QSTR_ecdsa;
                break;
            default:
                break;
        }
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_kea), kea ? MP_OBJ_NEW_QSTR(kea) : mp_const_none);
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_auth), auth ? MP_OBJ_NEW_QSTR(auth) : mp_const_none);
        mp_obj_list_append(list, dict);
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslcontext_get_ciphers_obj, mp_sslcontext_get_ciphers);

static mp_obj_t mp_sslcontext_load_default_certs(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_purpose, 0 };
    int endpoint = MBEDTLS_SSL_IS_CLIENT;
    parse_args_and_kw_map(n_args - 1, pos_args + 1, kw_args, "|i", kws, &endpoint);

    if (endpoint == MBEDTLS_SSL_IS_CLIENT) {
        mp_obj_t call_args[2];
        mp_load_method(pos_args[0], MP_QSTR_set_default_verify_paths, call_args);
        mp_call_method_n_kw(0, 0, call_args);
    }
    else if (endpoint != MBEDTLS_SSL_IS_SERVER) {
        mp_raise_ValueError(NULL);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_sslcontext_load_default_certs_obj, 1, mp_sslcontext_load_default_certs);

static mp_obj_t mp_sslcontext_load_verify_locations(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    socket_tls_context_t *context = mp_sslcontext_get(pos_args[0]);
    const qstr kws[] = { MP_QSTR_cafile, MP_QSTR_capath, MP_QSTR_cadata, 0 };
    const char *cafile = NULL;
    const char *capath = NULL;
    parse_args_and_kw_map(n_args - 1, pos_args + 1, kw_args, "|zzz", kws, &cafile, &capath, NULL);
    if (!cafile && !capath) {
        mp_raise_TypeError(NULL);
    }

    int ret = socket_tls_load_verify_locations(context, cafile, capath);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_sslcontext_load_verify_locations_obj, 1, mp_sslcontext_load_verify_locations);

static mp_obj_t mp_sslcontext_set_ciphers(mp_obj_t self_in, mp_obj_t ciphers) {
    socket_tls_context_t *context = mp_sslcontext_get(self_in);

    mp_obj_t args[3];
    mp_load_method(ciphers, MP_QSTR_split, args);
    args[2] = MP_OBJ_NEW_QSTR(MP_QSTR__colon_);
    mp_obj_t list = mp_call_method_n_kw(1, 0, args);

    mp_obj_t *items;
    size_t len = mp_obj_list_get_checked(list, &items);

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        const char *name = mp_obj_str_get_str(items[i]);
        if (strcmp(name, "ALL") == 0) {
            free(context->ciphersuites);
            context->ciphersuites = NULL;
            mbedtls_ssl_conf_ciphersuites(&context->conf, mbedtls_ssl_list_ciphersuites());
            return mp_const_none;
        }
        const int id = mbedtls_ssl_get_ciphersuite_id(name);
        if (id == 0) {
            continue;
        }
        if (j == 0) {
            void *tmp = realloc(context->ciphersuites, (len + 1) * sizeof(int));
            if (!tmp) {
                mp_raise_OSError(errno);
            }
            context->ciphersuites = tmp;
        }
        context->ciphersuites[j++] = id;
    }
    if (j == 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_sslerror, MP_ERROR_TEXT("No cipher can be selected")));
    }
    context->ciphersuites[j++] = 0;
    mbedtls_ssl_conf_ciphersuites(&context->conf, context->ciphersuites);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_sslcontext_set_ciphers_obj, mp_sslcontext_set_ciphers);

static mp_obj_t mp_sslcontext_set_default_verify_paths(mp_obj_t self_in) {
    socket_tls_context_t *context = mp_sslcontext_get(self_in);
    const char *cafile = getenv("SSL_CERT_FILE");
    const char *capath = getenv("SSL_CERT_DIR");
    socket_tls_load_verify_locations(context, cafile ? cafile : "cert.pem", capath ? capath : "certs");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslcontext_set_default_verify_paths_obj, mp_sslcontext_set_default_verify_paths);

#ifdef MBEDTLS_SSL_ALPN
static mp_obj_t mp_sslcontext_set_alpn_protocols(mp_obj_t self_in, mp_obj_t protocols_in) {
    socket_tls_context_t *context = mp_sslcontext_get(self_in);
    mp_obj_t *protocols;
    size_t num = mp_obj_list_get_checked(protocols_in, protocols_in);

    size_t total_len = 0;
    for (int i = 0; i < num; i++) {
        size_t len;
        mp_obj_str_get_data(protocols[i], &len);
        total_len += len + 1;
    }

    void *buffer = realloc(context->alpn_protocols, (num + 1) * sizeof(char *) + total_len);
    if (!buffer) {
        mp_raise_OSError(errno);
    }

    char **protos = buffer;
    char *p = buffer + (num + 1) * sizeof(char *);
    for (int i = 0; i < num; i++) {
        protos[i] = p;
        size_t len;
        const char *proto = mp_obj_str_get_data(protocols[i], &len);
        strncpy(p, proto, len);
        p += len;
        *p++ = '\0';
    }
    protos[num] = NULL;
    context->alpn_protocols = protos;

    int ret = mbedtls_ssl_conf_alpn_protocols(&context->conf, context->alpn_protocols);
    mp_ssl_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_sslcontext_set_alpn_protocols_obj, mp_sslcontext_set_alpn_protocols);
#endif

#ifdef MBEDTLS_ECDH_C
static mp_obj_t mp_sslcontext_set_ecdh_curve(mp_obj_t self_in, mp_obj_t curve_name) {
    socket_tls_context_t *context = mp_sslcontext_get(self_in);
    const char *name = mp_obj_str_get_str(curve_name);
    const mbedtls_ecp_curve_info *info = mbedtls_ecp_curve_info_from_name(name);
    if (!info) {
        mp_raise_ValueError(NULL);
    }

    context->groups[0] = info->tls_id;
    context->groups[1] = MBEDTLS_SSL_IANA_TLS_GROUP_NONE;
    mbedtls_ssl_conf_groups(&context->conf, context->groups);
    return mp_const_none;    
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_sslcontext_set_ecdh_curve_obj, mp_sslcontext_set_ecdh_curve);
#endif

static mp_obj_t mp_sslcontext_wrap_socket(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_sock, MP_QSTR_server_side, MP_QSTR_do_handshake_on_connect, MP_QSTR_suppress_ragged_eofs, MP_QSTR_server_hostname, MP_QSTR_session, 0 };
    mp_sslcontext_get(pos_args[0]);
    mp_obj_socket_t *sock;
    mp_int_t server_side = 0;
    mp_int_t do_handshake_on_connect = 1;
    mp_int_t suppress_ragged_eofs = 1;
    mp_obj_t server_hostname = mp_const_none;
    mp_obj_t session = mp_const_none;
    parse_args_and_kw_map(n_args - 1, pos_args + 1, kw_args, "O&|pppOO", kws, mp_socket_get, &sock, &server_side, &do_handshake_on_connect, &suppress_ragged_eofs, &server_hostname, &session);

    int flags = 0;
    flags |= server_side ? SOCKET_TLS_FLAG_SERVER_SIDE : 0;
    flags |= do_handshake_on_connect ? SOCKET_TLS_FLAG_DO_HANDSHAKE_ON_CONNECT : 0;
    flags |= suppress_ragged_eofs ? SOCKET_TLS_FLAG_SUPPRESS_RAGGED_EOFS : 0;
    return mp_sslsocket_make_new(sock, MP_OBJ_TO_PTR(pos_args[0]), flags, server_hostname);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_sslcontext_wrap_socket_obj, 1, mp_sslcontext_wrap_socket);

static const mp_rom_map_elem_t mp_sslcontext_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),                     MP_ROM_PTR(&mp_sslcontext_del_obj) },

    { MP_ROM_QSTR(MP_QSTR_cert_store_stats),            MP_ROM_PTR(&mp_sslcontext_cert_store_stats_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_ca_certs),                MP_ROM_PTR(&mp_sslcontext_get_ca_certs_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_ciphers),                 MP_ROM_PTR(&mp_sslcontext_get_ciphers_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_cert_chain),             MP_ROM_PTR(&mp_sslcontext_load_cert_chain_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_default_certs),          MP_ROM_PTR(&mp_sslcontext_load_default_certs_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_verify_locations),       MP_ROM_PTR(&mp_sslcontext_load_verify_locations_obj) },
    #ifdef MBEDTLS_SSL_ALPN
    { MP_ROM_QSTR(MP_QSTR_set_alpn_protocols),          MP_ROM_PTR(&mp_sslcontext_set_alpn_protocols_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_set_ciphers),                 MP_ROM_PTR(&mp_sslcontext_set_ciphers_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_default_verify_paths),    MP_ROM_PTR(&mp_sslcontext_set_default_verify_paths_obj) },
    #ifdef MBEDTLS_ECDH_C
    { MP_ROM_QSTR(MP_QSTR_set_ecdh_curve),              MP_ROM_PTR(&mp_sslcontext_set_ecdh_curve_obj) },
    #endif    
    { MP_ROM_QSTR(MP_QSTR_wrap_socket),                 MP_ROM_PTR(&mp_sslcontext_wrap_socket_obj) },
};
static MP_DEFINE_CONST_DICT(mp_sslcontext_locals_dict, mp_sslcontext_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_sslcontext,
    MP_QSTR_SSLContext,
    MP_TYPE_FLAG_NONE,
    make_new, mp_sslcontext_make_new,
    attr, mp_sslcontext_attr,
    locals_dict, &mp_sslcontext_locals_dict
    );
