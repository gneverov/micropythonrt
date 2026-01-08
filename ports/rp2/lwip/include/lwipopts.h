#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#include <stdint.h>

#define NO_SYS                          0
#define SYS_LIGHTWEIGHT_PROT            1
#define MEM_ALIGNMENT                   4

#define LWIP_CHKSUM_ALGORITHM           3
// The checksum flags are set in eth.c
#define LWIP_CHECKSUM_CTRL_PER_NETIF    1

#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_RAW                        1
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define LWIP_STATS                      LWIP_SNMP
#define LWIP_NETIF_HOSTNAME             0
#define LWIP_NETIF_EXT_STATUS_CALLBACK  1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LOOPBACK             1

#define LWIP_IPV4                       1
#define LWIP_IPV6                       1
#define LWIP_ND6_NUM_DESTINATIONS       4
#define LWIP_ND6_QUEUEING               0
#define LWIP_DHCP                       1
#define LWIP_DHCP_CHECK_LINK_UP         1
#define LWIP_DHCP_GET_NTP_SRV           1
#define LWIP_DHCP_DOES_ACD_CHECK        0 // to speed DHCP up
#define LWIP_DNS                        1
#define LWIP_DNS_SUPPORT_MDNS_QUERIES   0
#define LWIP_MDNS_RESPONDER             0
#define LWIP_IGMP                       1
#define LWIP_TCP_KEEPALIVE              0
#define LWIP_SNMP                       0
#if LWIP_SNMP
#define MIB2_STATS                      1
#endif

#define LWIP_NUM_NETIF_CLIENT_DATA      (1 + LWIP_MDNS_RESPONDER)

#define SO_REUSE                        1
#define TCP_LISTEN_BACKLOG              1

#define IP_OPTIONS_ALLOWED              0
#define IP_DEFAULT_TTL                 64

#define MEM_SIZE (256 << 10)
#define TCP_MSS (1460)
#define TCP_WND (8 * TCP_MSS)
#define TCP_SND_BUF (8 * TCP_MSS)

typedef uint32_t sys_prot_t;

#define MEM_LIBC_MALLOC                 1
#define MEMP_MEM_MALLOC                 1
#define LWIP_ERRNO_STDINCLUDE           1
#define TCPIP_MBOX_SIZE                 8
#define TCPIP_THREAD_STACKSIZE          4096
#define TCPIP_THREAD_PRIO               2

#define LWIP_FREERTOS_SYS_ARCH_PROTECT_USES_MUTEX 1
#define LWIP_FREERTOS_CHECK_CORE_LOCKING 1
void sys_mark_tcpip_thread(void);
#define LWIP_MARK_TCPIP_THREAD()   sys_mark_tcpip_thread()
void sys_check_core_locking(void);
#define LWIP_ASSERT_CORE_LOCKED()  sys_check_core_locking()
void sys_lock_tcpip_core(void);
#define LOCK_TCPIP_CORE()          sys_lock_tcpip_core()
void sys_unlock_tcpip_core(void);
#define UNLOCK_TCPIP_CORE()        sys_unlock_tcpip_core()

#include <sys/time.h>
#define SNTP_SET_SYSTEM_TIME_US(sec, us)    do { \
        struct timeval tv = { (sec), (us) }; \
        settimeofday(&tv, NULL); \
} while (0)
#define SNTP_GET_SYSTEM_TIME(sec, us)       do { \
        struct timeval tv; \
        gettimeofday(&tv, NULL); \
        (sec) = tv.tv_sec; \
        (us) = tv.tv_usec; \
} while (0)
#define SNTP_CHECK_RESPONSE             2
#define SNTP_COMP_ROUNDTRIP             1

#endif // LWIPOPTS_H
