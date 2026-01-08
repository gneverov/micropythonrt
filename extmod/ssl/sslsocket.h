// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "extmod/socket/socket.h"
#include "extmod/ssl/sslcontext.h"
#include "py/obj.h"


typedef struct {
    mp_obj_socket_t sock;
    mp_obj_sslcontext_t *sslcontext;
    int server_side:1;
    int do_handshake:1;
    mp_obj_t server_hostname;
} mp_obj_sslsocket_t;

extern const mp_obj_type_t mp_type_sslsocket;

mp_obj_t mp_sslsocket_make_new(mp_obj_t sock, mp_obj_sslcontext_t *sslcontext, int flags, mp_obj_t server_hostname);
