// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/socket.h>

#include "lwip/err.h"

#include "py/obj.h"


socklen_t mp_socket_sockaddr_parse(int af, mp_obj_t address_in, struct sockaddr_storage *address, int ai_flags);

mp_obj_t mp_socket_sockaddr_format(const struct sockaddr *address, socklen_t address_len);

typedef struct {
    mp_obj_base_t base;
    int fd;
} mp_obj_socket_t;

extern const mp_obj_type_t mp_type_socket;

extern mp_float_t mp_socket_defaulttimeout;

mp_obj_socket_t *mp_socket_get(mp_obj_t self_in);

void mp_socket_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind);
