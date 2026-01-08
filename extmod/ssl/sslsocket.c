// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <sys/socket.h>
#include <unistd.h>

#include "mbedtls/error.h"

#include "extmod/io/modio.h"
#include "extmod/modos_newlib.h"
#include "extmod/ssl/modssl.h"
#include "extmod/ssl/sslsocket.h"
#include "py/extras.h"
#include "py/runtime.h"


static mp_obj_sslsocket_t *mp_sslsocket_get(mp_obj_t self_in) {
    mp_obj_sslsocket_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->sock.fd == -1) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed socket"));
    }
    return self;
}

static struct socket_tls *mp_sslsocket_acquire(mp_obj_sslsocket_t *self) {
    struct socket_tls *socket = socket_tls_acquire(self->sock.fd);
    if (!socket) {
        mp_raise_OSError(errno);
    }
    return socket;
}

static mp_obj_t mp_sslsocket_do_handshake(mp_obj_t self_in);

static int mp_sslsocket_call_handshake(mp_obj_sslsocket_t *self) {
    struct socket_tls *socket = mp_sslsocket_acquire(self);
    int ret;
    MP_OS_CALL(ret, socket_tls_handshake, socket);
    socket_tls_release(socket);
    return ret;
}

mp_obj_t mp_sslsocket_make_new(mp_obj_t sock, mp_obj_sslcontext_t *sslcontext, int flags, mp_obj_t server_hostname_in) {
    if ((flags & SOCKET_TLS_FLAG_SERVER_SIDE) && (server_hostname_in != mp_const_none)) {
        mp_raise_ValueError(NULL);
    }
    const char *server_hostname = NULL;
    if (sslcontext->check_hostname) {
        if (server_hostname_in != mp_const_none) {
            server_hostname = mp_obj_str_get_str(server_hostname_in);
        } else {
            mp_raise_ValueError(MP_ERROR_TEXT("check_hostname requires server_hostname"));
        }
    }

    mp_obj_t args[3];
    mp_load_method(sock, MP_QSTR_gettimeout, args);
    args[2] = mp_call_method_n_kw(0, 0, args);
    mp_load_method(sock, MP_QSTR_detach, args);
    int inner_fd = mp_obj_get_int(mp_call_method_n_kw(0, 0, args));

    int fd = socket_tls_wrap(inner_fd, sslcontext->context, flags);
    close(inner_fd);
    mp_os_check_ret(fd);

    mp_obj_sslsocket_t *self = mp_obj_malloc_with_finaliser(mp_obj_sslsocket_t, &mp_type_sslsocket);
    self->sock.fd = fd;
    self->sslcontext = sslcontext;
    self->server_side = flags & SOCKET_TLS_FLAG_SERVER_SIDE;
    self->server_hostname = server_hostname_in;

    mp_load_method(MP_OBJ_FROM_PTR(self), MP_QSTR_settimeout, args);
    mp_call_method_n_kw(1, 0, args);

    struct socket_tls *socket = mp_sslsocket_acquire(self);
    int ret = 0;
    if (sslcontext->check_hostname) {
        ret = socket_tls_check_ret(mbedtls_ssl_set_hostname(&socket->ssl, server_hostname));
        if (ret < 0) {
            goto exit;
        }
    }

    if (flags & SOCKET_TLS_FLAG_DO_HANDSHAKE_ON_CONNECT) {
        ret = mp_sslsocket_call_handshake(self);
        if ((ret < 0) && (errno == ENOTCONN)) {
            ret = 0;
        }        
    }

exit:
    socket_tls_release(socket);
    mp_os_check_ret(ret);
    return MP_OBJ_FROM_PTR(self);
}

static void mp_sslsocket_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    if (dest[0] == MP_OBJ_SENTINEL) {
        return;
    }
    switch (attr) {
        case MP_QSTR_context:
            dest[0] = MP_OBJ_FROM_PTR(mp_sslsocket_get(self_in)->sslcontext);
            break;
        case MP_QSTR_server_side:
            dest[0] = mp_obj_new_bool(mp_sslsocket_get(self_in)->server_side);
            break;
        case MP_QSTR_server_hostname:
            dest[0] = mp_sslsocket_get(self_in)->server_hostname;
            break;
        default:
            mp_super_attr(self_in, &mp_type_sslsocket, attr, dest);
            break;
    }
}

static mp_obj_t mp_sslsocket_cipher(mp_obj_t self_in) {
    mp_obj_sslsocket_t *self = mp_sslsocket_get(self_in);
    
    struct socket_tls *socket = mp_sslsocket_acquire(self);
    const char *ciphersuite = mbedtls_ssl_get_ciphersuite(&socket->ssl);
    const char *version = mbedtls_ssl_get_version(&socket->ssl);
    socket_tls_release(socket);

    if (!ciphersuite) {
        return mp_const_none;
    }
    mp_obj_t items[3] = {
        mp_obj_new_str_from_cstr(ciphersuite),
        mp_obj_new_str_from_cstr(version),
        mp_const_none,
    };
    return mp_obj_new_tuple(3, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslsocket_cipher_obj, mp_sslsocket_cipher);

static mp_obj_t mp_sslsocket_do_handshake(mp_obj_t self_in) {
    mp_obj_sslsocket_t *self = mp_sslsocket_get(self_in);
    int ret = mp_sslsocket_call_handshake(self);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslsocket_do_handshake_obj, mp_sslsocket_do_handshake);

static mp_obj_t mp_sslsocket_getpeercert(size_t n_args, const mp_obj_t *args) {
    mp_obj_sslsocket_t *self = mp_sslsocket_get(args[0]);
    bool binary_form = (n_args > 1) && mp_obj_is_true(args[1]);
    struct socket_tls *socket = mp_sslsocket_acquire(self);
    const mbedtls_x509_crt *cert = mbedtls_ssl_get_peer_cert(&socket->ssl);
    socket_tls_release(socket);
    if (!cert) {
        return mp_const_none;
    }

    return mp_ssl_make_cert(cert, binary_form);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_sslsocket_getpeercert_obj, 1, 2, mp_sslsocket_getpeercert);

static mp_obj_t mp_sslsocket_compression(mp_obj_t self_in) {
    mp_sslsocket_get(self_in);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslsocket_compression_obj, mp_sslsocket_compression);

static mp_obj_t mp_sslsocket_get_channel_binding(size_t n_args, const mp_obj_t *args) {
    mp_sslsocket_get(args[0]);
    mp_raise_ValueError(NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_sslsocket_get_channel_binding_obj, 1, 2, mp_sslsocket_get_channel_binding);

static mp_obj_t mp_sslsocket_selected_alpn_protocol(mp_obj_t self_in) {
    mp_obj_sslsocket_t *self = mp_sslsocket_get(self_in);
#ifdef MBEDTLS_SSL_ALPN
    struct socket_tls *socket = mp_sslsocket_acquire(self);
    const char *protocol = mbedtls_ssl_get_alpn_protocol(&socket->ssl);
    socket_tls_release(socket);
    return protocol ? mp_obj_new_str_from_cstr(protocol) : mp_const_none;
#else
    (void)self;
    return mp_const_none;
#endif
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslsocket_selected_alpn_protocol_obj, mp_sslsocket_selected_alpn_protocol);

static mp_obj_t mp_sslsocket_unwrap(mp_obj_t self_in) {
    mp_obj_sslsocket_t *self = mp_sslsocket_get(self_in);
    struct socket_tls *socket = mp_sslsocket_acquire(self);
    int fd = socket_fd(socket->inner);
    socket_tls_release(socket);
    
    mp_obj_t args[2] = { MP_OBJ_NEW_QSTR(MP_QSTR_fileno), MP_OBJ_NEW_SMALL_INT(fd) };
    mp_obj_t ret = mp_obj_make_new(&mp_type_socket, 0, 1, args);

    mp_load_method(self_in, MP_QSTR_close, args);
    mp_call_method_n_kw(0, 0, args);
    return ret;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslsocket_unwrap_obj, mp_sslsocket_unwrap);

static mp_obj_t mp_sslsocket_version(mp_obj_t self_in) {
    mp_obj_t cipher = mp_sslsocket_cipher(self_in);
    if (cipher == mp_const_none) {
        return mp_const_none;
    }
    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(cipher);
    return tuple->items[1];
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sslsocket_version_obj, mp_sslsocket_version);


static const mp_rom_map_elem_t mp_sslsocket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_cipher),          MP_ROM_PTR(&mp_sslsocket_cipher_obj) },
    { MP_ROM_QSTR(MP_QSTR_do_handshake),    MP_ROM_PTR(&mp_sslsocket_do_handshake_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpeercert),     MP_ROM_PTR(&mp_sslsocket_getpeercert_obj) },
    { MP_ROM_QSTR(MP_QSTR_compression),     MP_ROM_PTR(&mp_sslsocket_compression_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_channel_binding), MP_ROM_PTR(&mp_sslsocket_get_channel_binding_obj) },
    { MP_ROM_QSTR(MP_QSTR_selected_alpn_protocol), MP_ROM_PTR(&mp_sslsocket_selected_alpn_protocol_obj) },
    { MP_ROM_QSTR(MP_QSTR_unwrap),          MP_ROM_PTR(&mp_sslsocket_unwrap_obj) },
    { MP_ROM_QSTR(MP_QSTR_version),         MP_ROM_PTR(&mp_sslsocket_version_obj) },
};
static MP_DEFINE_CONST_DICT(mp_sslsocket_locals_dict, mp_sslsocket_locals_dict_table);

extern const mp_stream_p_t mp_socket_stream_p;

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_sslsocket,
    MP_QSTR_SSLSocket,
    MP_TYPE_FLAG_NONE,
    // make_new, mp_sslsocket_make_new,
    attr, mp_sslsocket_attr,
    print, mp_socket_print,
    protocol, &mp_socket_stream_p,
    locals_dict, &mp_sslsocket_locals_dict,
    parent, &mp_type_socket
    );
