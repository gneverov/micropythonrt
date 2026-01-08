// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "lwip/apps/sntp.h"

#include "extmod/socket/netif.h"
#include "extmod/socket/sntp.h"
#include "py/runtime.h"


static mp_obj_t sntp_sntp_get(void) {
    LOCK_TCPIP_CORE();
    bool enabled = sntp_enabled();
    ip_addr_t sntp_servers[SNTP_MAX_SERVERS]; 
    if (enabled) {
        for (size_t i = 0; i < SNTP_MAX_SERVERS; i++) {
            sntp_servers[i] = *sntp_getserver(i);
        }
    }
    UNLOCK_TCPIP_CORE();

    if (!enabled) {
        return mp_const_none;
    }

    size_t len = 0;
    mp_obj_t items[SNTP_MAX_SERVERS];
    for (size_t i = 0; i < SNTP_MAX_SERVERS; i++) {
        if (!ip_addr_isany(&sntp_servers[i])) {
            items[len++] = netif_inet_ntoa(&sntp_servers[i]);
        }
    }
    return mp_obj_new_list(len, items);    
}

// Call signatures:
// sntp() - returns list of sntp servers or None if not enabled
// sntp(false_value) - disables sntp
// sntp(non_empty_list) - enables sntp with list of server ip addresses
// sntp(true_value) - enables sntp with servers from dhcp
mp_obj_t sntp_sntp(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return sntp_sntp_get();
    }

    bool enabled = mp_obj_is_true(args[0]);
    size_t len = 0;
    mp_obj_t *items = MP_OBJ_NULL;
    if (mp_obj_is_exact_type(args[0], MP_OBJ_FROM_PTR(&mp_type_list))) {
        mp_obj_list_get(args[0], &len, &items);
    }
    if (len > SNTP_MAX_SERVERS) {
        mp_raise_ValueError(NULL);
    }
    ip_addr_t sntp_servers[SNTP_MAX_SERVERS];
    for (size_t i = 0; i < len; i++) {
        netif_inet_aton(items[i], &sntp_servers[i]);
    }

    LOCK_TCPIP_CORE();
    sntp_stop();
    if (items) {
        for (size_t i = 0; i < SNTP_MAX_SERVERS; i++) {
            sntp_setserver(i, (i < len) ? &sntp_servers[i] : NULL);
        }
    }
    if (enabled) {
        sntp_servermode_dhcp(!items);
        sntp_init();
    }
    UNLOCK_TCPIP_CORE();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sntp_sntp_obj, 0, 1, sntp_sntp);
