// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <string.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/snmp.h"
#include "lwip/apps/snmp_mib2.h"
#include "lwip/apps/snmpv3.h"
#include "lwip/apps/sntp.h"


#if LWIP_MDNS_RESPONDER
static void lwip_helper_netif_cb(struct netif *netif, netif_nsc_reason_t reason, const netif_ext_callback_args_t *args) {
    if (reason & LWIP_NSC_NETIF_ADDED) {
        char hostname[MDNS_LABEL_MAXLEN + 1];
        if ((gethostname(hostname, MDNS_LABEL_MAXLEN) >= 0) && strnlen(hostname, MDNS_LABEL_MAXLEN)) {
            mdns_resp_add_netif(netif, hostname);
        }
    }
    if (reason & LWIP_NSC_NETIF_REMOVED) {
        mdns_resp_remove_netif(netif);
    }
}
#endif

static void lwip_init_cb(void *arg) {
    SemaphoreHandle_t init_sem = arg;
    xSemaphoreGive(init_sem);

    #if LWIP_MDNS_RESPONDER
    mdns_resp_init();
    NETIF_DECLARE_EXT_CALLBACK(netif_callback);
    netif_add_ext_callback(&netif_callback, lwip_helper_netif_cb);
    #endif

    sntp_servermode_dhcp(1);
    sntp_init();


    #if LWIP_SNMP
    s32_t req_nr;
    #if SNMP_LWIP_MIB2
    snmp_mib2_set_syscontact_readonly((const u8_t *)"root", NULL);
    snmp_mib2_set_syslocation_readonly((const u8_t *)"lwIP development PC", NULL);
    snmp_mib2_set_sysdescr((const u8_t *)"lwIP example", NULL);
    #endif /* SNMP_LWIP_MIB2 */

    #if LWIP_SNMP_V3
    snmpv3_dummy_init();
    #endif

    snmp_init();

    snmp_trap_dst_ip_set(0, &netif_default->gw);
    snmp_trap_dst_enable(0, 1);

    snmp_send_inform_generic(SNMP_GENTRAP_COLDSTART, NULL, &req_nr);
    snmp_send_trap_generic(SNMP_GENTRAP_COLDSTART);

    #endif /* LWIP_SNMP */
}

void lwip_helper_init(void) {
    SemaphoreHandle_t init_sem = xSemaphoreCreateBinary();
    tcpip_init(lwip_init_cb, init_sem);
    xSemaphoreTake(init_sem, portMAX_DELAY);
    vSemaphoreDelete(init_sem);
}
