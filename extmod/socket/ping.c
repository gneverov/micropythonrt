// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <sys/socket.h>
#include "morelib/lwip/socket.h"
#include "morelib/lwip/ping.h"

#include "extmod/modos_newlib.h"
#include "extmod/socket/socket.h"
#include "extmod/socket/ping.h"
#include "py/runtime.h"


static mp_obj_t ping_ping(mp_obj_t dest_in) {
    mp_obj_t items[] = { dest_in, mp_const_none };
    struct sockaddr_storage sockaddr;
    socklen_t socklen = mp_socket_sockaddr_parse(AF_UNSPEC, mp_obj_new_tuple(2, items), &sockaddr, 0);

    ip_addr_t ipaddr;
    u16_t port;
    socket_sockaddr_to_lwip((struct sockaddr *)&sockaddr, socklen, &ipaddr, &port);

    int ret;
    MP_OS_CALL(ret, ping, &ipaddr);
    mp_os_check_ret(ret);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(ping_ping_obj, ping_ping);
