// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <arpa/inet.h>
#include <math.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "morelib/socket.h"

#include "extmod/modos_newlib.h"
#include "extmod/io/modio.h"
#include "extmod/socket/socket.h"
#include "py/extras.h"
#include "py/parseargs.h"
#include "py/runtime.h"
#include "py/stream.h"


#define MP_SOCKET_GLOBAL_DEFAULT_TIMEOUT MP_OBJ_NEW_IMMEDIATE_OBJ(MP_QSTR_socket)

mp_float_t mp_socket_defaulttimeout;

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_gaierror,
    MP_QSTR_gaierror,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new,
    print, mp_obj_exception_print,
    attr, mp_obj_exception_attr,    
    parent, &mp_type_OSError
    );

mp_obj_t mp_socket_sockaddr_format(const struct sockaddr *address, socklen_t address_len) {
    if ((address->sa_family == AF_UNSPEC) || !address_len) {
        return mp_const_none;
    }
    switch (address->sa_family) {
        case AF_UNIX: {
            assert(address_len >= sizeof(struct sockaddr_un));
            const struct sockaddr_un *sa = (const struct sockaddr_un *)address;
            return mp_obj_new_str_from_cstr(sa->sun_path);
        }
        #if LWIP_IPV4
        case AF_INET: {
            assert(address_len >= sizeof(struct sockaddr_in));
            const struct sockaddr_in *sa = (const struct sockaddr_in *)address;
            vstr_t host;
            vstr_init(&host, INET_ADDRSTRLEN);
            if (!inet_ntop(AF_INET, &sa->sin_addr, host.buf, INET_ADDRSTRLEN)) {
                break;
            }
            host.len = strlen(host.buf);
            mp_obj_t items[2] = {
                mp_obj_new_str_from_vstr(&host),
                MP_OBJ_NEW_SMALL_INT(ntohs(sa->sin_port)),
            };
            return mp_obj_new_tuple(2, items);
        }
        #endif
        #if LWIP_IPV6
        case AF_INET6: {
            assert(address_len >= sizeof(struct sockaddr_in6));
            const struct sockaddr_in6 *sa = (const struct sockaddr_in6 *)address;
            vstr_t host;
            vstr_init(&host, INET6_ADDRSTRLEN);
            if (!inet_ntop(AF_INET6, &sa->sin6_addr, host.buf, INET6_ADDRSTRLEN)) {
                break;
            }
            host.len = strlen(host.buf);
            mp_obj_t items[4] = {
                mp_obj_new_str_from_vstr(&host),
                MP_OBJ_NEW_SMALL_INT(ntohs(sa->sin6_port)),
                MP_OBJ_NEW_SMALL_INT(sa->sin6_flowinfo),
                MP_OBJ_NEW_SMALL_INT(sa->sin6_scope_id),
            };
            return mp_obj_new_tuple(4, items);
        }
        #endif        
        default: {
            errno = EAFNOSUPPORT;
            break;
        }
    }
    mp_raise_OSError(errno);
}

struct mp_socket_addrinfo_node {
    nlr_jump_callback_node_t nlr;
    struct addrinfo *ai;
};

static void mp_socket_addrinfo_cb(void *ctx) {
    struct mp_socket_addrinfo_node *node = ctx;
    freeaddrinfo(node->ai);
}

static void mp_socket_check_gai_ret(int ret) {
    if (ret == EAI_SYSTEM) {
        mp_raise_OSError(errno);
    }
    else if (ret < 0) {
        const char *strerror = gai_strerror(ret);
        mp_obj_t args[] = {
            MP_OBJ_NEW_SMALL_INT(ret),
            mp_obj_new_str(strerror, strlen(strerror)),
        };
        mp_obj_t exc = mp_obj_exception_make_new(&mp_type_gaierror, 2, 0, args);
        nlr_raise(exc);
    }    
}

static void mp_socket_call_getaddrinfo(mp_obj_t host, mp_obj_t port, const struct addrinfo *hints, struct addrinfo **ai) {
    const char *name = NULL;
    if (host != mp_const_none) {
        name = mp_obj_str_get_str(host);
        if (!strcmp(name, "")) {
            // Empty string means NULL for getaddrinfo call
            name = NULL;
        }
    }
    const char *service = NULL;
    char service_val[12];
    if (mp_obj_is_int(port)) {
        unsigned int int_port = mp_obj_get_uint(port);
        if (int_port > USHRT_MAX) {
            mp_raise_type(&mp_type_OverflowError);
        }
        snprintf(service_val, 12, "%u", int_port);
        service = service_val;
    }
    else if (port != mp_const_none) {
        service = mp_obj_str_get_str(port);
    }
    
    int ret;
    MP_OS_CALL(ret, getaddrinfo, name, service, hints, ai);
    mp_socket_check_gai_ret(ret);
}

static mp_obj_t mp_socket_getaddrinfo(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_host, MP_QSTR_port, MP_QSTR_family, MP_QSTR_type, MP_QSTR_proto, MP_QSTR_flags, 0 };
    mp_obj_t host, port;
    int family=AF_UNSPEC, type=0, proto=0, flags=0;
    parse_args_and_kw_map(n_args, args, kw_args, "OO|iiii", kws, &host, &port, &family, &type, &proto, &flags);

    struct addrinfo hints = { flags, family, type, proto, 0 };
    struct mp_socket_addrinfo_node node;
    mp_socket_call_getaddrinfo(host, port, &hints, &node.ai);

    nlr_push_jump_callback(&node.nlr, mp_socket_addrinfo_cb);
    mp_obj_t list = mp_obj_new_list(0, NULL);
    struct addrinfo *ai = node.ai;
    while (ai) {
        const char *cname = ai->ai_canonname;
        mp_obj_t items[] = {
            MP_OBJ_NEW_SMALL_INT(ai->ai_family),
            MP_OBJ_NEW_SMALL_INT(ai->ai_socktype),
            MP_OBJ_NEW_SMALL_INT(ai->ai_protocol),
            cname ? mp_obj_new_str(cname, strlen(cname)) : mp_const_none,
            mp_socket_sockaddr_format(ai->ai_addr, ai->ai_addrlen),
        };
        mp_obj_t tuple = mp_obj_new_tuple(5, items);
        mp_obj_list_append(list, tuple);
        ai = ai->ai_next;
    }
    nlr_pop_jump_callback(true);
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_socket_getaddrinfo_obj, 0, mp_socket_getaddrinfo);

static socklen_t mp_socket_getaddrinfo_as_sockaddr(int af, mp_obj_t host, mp_obj_t port, struct sockaddr_storage *address, int flags) {
    struct addrinfo hints = { flags, af, 0 };
    struct addrinfo *ai;
    mp_socket_call_getaddrinfo(host, port, &hints, &ai);
    socklen_t len = 0;
    if (ai) {
        memcpy(address, ai->ai_addr, MIN(ai->ai_addrlen, sizeof(*address)));
        len = ai->ai_addrlen;
    }
    freeaddrinfo(ai);
    return len;
}

static mp_obj_t mp_socket_if_indextoname(mp_obj_t index_in);

socklen_t mp_socket_sockaddr_parse(int af, mp_obj_t address_in, struct sockaddr_storage *address, int ai_flags) {
    mp_obj_t *items;
    size_t tuple_len = mp_obj_tuple_get_checked(address_in, &items);
    if (tuple_len < 1) {
        mp_raise_ValueError(NULL);
    }
    socklen_t len = mp_socket_getaddrinfo_as_sockaddr(af, items[0], (tuple_len > 1) ? items[1] : mp_const_none, address, ai_flags);

    switch (address->ss_family) {
        #if LWIP_IPV4
        case AF_INET: {
            if (tuple_len != 2) {
                mp_raise_ValueError(NULL);
            }
            break;
        }
        #endif
    
        #if LWIP_IPV6
        case AF_INET6: {
            struct sockaddr_in6 *sa = (struct sockaddr_in6 *)address;
            switch (tuple_len) {
                case 2: {
                    break;
                }
                case 4: {
                    // scope_id is optional, but if provided, must be a valid interface index
                    mp_int_t scope_id = mp_obj_get_int(items[3]);
                    if (scope_id) {
                        mp_socket_if_indextoname(items[3]);
                        sa->sin6_scope_id = scope_id;
                    }
                    
                    // fall through
                }
                case 3: {
                    mp_int_t flowinfo = mp_obj_get_int(items[2]);
                    if (flowinfo < 0 || flowinfo > 0xFFFFF) {
                        mp_raise_type(&mp_type_OverflowError);
                    }
                    else if (flowinfo != 0) {
                        sa->sin6_flowinfo = flowinfo;
                    }
                    break;
                }
                default: {
                    mp_raise_ValueError(NULL);
                }
            }
            break;
        }
        #endif

        default: {
            mp_raise_OSError(EAFNOSUPPORT);
        }
    }
    return len;
}

static mp_obj_t mp_socket_gethostbyname(mp_obj_t hostname) {
    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len = mp_socket_getaddrinfo_as_sockaddr(AF_INET, hostname, mp_const_none, &sockaddr, 0);
    mp_obj_t result = mp_socket_sockaddr_format((struct sockaddr *)&sockaddr, sockaddr_len);
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(result, &len, &items);
    return items[0];
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_gethostbyname_obj, mp_socket_gethostbyname);

static mp_obj_t mp_socket_gethostname(void) {
    vstr_t vstr;
    vstr_init(&vstr, 256);
    int ret = gethostname(vstr.buf, vstr.alloc);
    mp_os_check_ret(ret);
    vstr.len = strnlen(vstr.buf, vstr.alloc);
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_socket_gethostname_obj, mp_socket_gethostname);

static mp_obj_t mp_socket_getnameinfo(mp_obj_t sockaddr_in, mp_obj_t flags_in) {
    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len = mp_socket_sockaddr_parse(AF_UNSPEC, sockaddr_in, &sockaddr, AI_NUMERICHOST);

    int flags = mp_obj_get_int(flags_in);

    vstr_t host;
    vstr_init(&host, 64);
    vstr_t service;
    vstr_init(&service, 12);

    int ret = getnameinfo((struct sockaddr *)&sockaddr, sockaddr_len, host.buf, host.alloc, service.buf, service.alloc, flags);
    mp_socket_check_gai_ret(ret);
    
    host.len = strnlen(host.buf, host.alloc);
    service.len = strnlen(service.buf, service.alloc);

    mp_obj_t items[] = {
        mp_obj_new_str_from_vstr(&host),
        mp_obj_new_str_from_vstr(&service),
    };
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_getnameinfo_obj, mp_socket_getnameinfo);

static mp_obj_t mp_socket_gethostbyaddr(mp_obj_t ipaddress) {
    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len = mp_socket_getaddrinfo_as_sockaddr(AF_UNSPEC, ipaddress, mp_const_none, &sockaddr, 0);

    vstr_t host;
    vstr_init(&host, 64);

    int ret = getnameinfo((struct sockaddr *)&sockaddr, sockaddr_len, host.buf, host.alloc, NULL, 0, 0);
    mp_socket_check_gai_ret(ret);
    
    host.len = strnlen(host.buf, host.alloc);

    mp_obj_t result = mp_socket_sockaddr_format((struct sockaddr *)&sockaddr, sockaddr_len);
    size_t len;
    mp_obj_t *ipaddrlist_items;
    mp_obj_tuple_get(result, &len, &ipaddrlist_items);

    mp_obj_t items[] = {
        mp_obj_new_str_from_vstr(&host), // hostname
        mp_obj_new_list(0, NULL), // aliaslist
        mp_obj_new_list(1, ipaddrlist_items), // ipaddrlist
    };
    return mp_obj_new_tuple(3, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_gethostbyaddr_obj, mp_socket_gethostbyaddr);

static mp_obj_t mp_socket_getfqdn(size_t n_args, const mp_obj_t *args) {
    if (n_args > 0) {
        return args[0];
    } else {
        return mp_socket_gethostname();
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_getfqdn_obj, 0, 1, mp_socket_getfqdn);

static mp_obj_t mp_socket_ntohl(mp_obj_t x_in) {
    uint32_t x = mp_obj_get_uint(x_in);
    return mp_obj_new_int_from_uint(ntohl(x));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_ntohl_obj, mp_socket_ntohl);

static mp_obj_t mp_socket_ntohs(mp_obj_t x_in) {
    uint16_t x = mp_obj_get_uint(x_in);
    return MP_OBJ_NEW_SMALL_INT(ntohs(x));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_ntohs_obj, mp_socket_ntohs);

static mp_obj_t mp_socket_htonl(mp_obj_t x_in) {
    uint32_t x = mp_obj_get_uint(x_in);
    return mp_obj_new_int_from_uint(htonl(x));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_htonl_obj, mp_socket_htonl);

static mp_obj_t mp_socket_htons(mp_obj_t x_in) {
    uint16_t x = mp_obj_get_uint(x_in);
    return MP_OBJ_NEW_SMALL_INT(htons(x));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_htons_obj, mp_socket_htons);

static int mp_socket_af_addr_size(int af) {
    switch (af) {
        case AF_INET:
            return sizeof(struct in_addr);
        case AF_INET6:
            return sizeof(struct in6_addr);
        default:
            mp_raise_OSError(EAFNOSUPPORT);
    }
}

static int mp_socket_af_str_size(int af) {
    switch (af) {
        case AF_INET:
            return INET_ADDRSTRLEN;
        case AF_INET6:
            return INET6_ADDRSTRLEN;
        default:
            mp_raise_OSError(EAFNOSUPPORT);
    }
}

static mp_obj_t mp_socket_inet_pton(mp_obj_t af_in, mp_obj_t ip_string_in) {
    int af = mp_obj_get_int(af_in);
    const char *ip_string = mp_obj_str_get_str(ip_string_in);
    vstr_t vstr;
    vstr_init_len(&vstr, mp_socket_af_addr_size(af));
    int ret = inet_pton(af, ip_string, vstr.buf);
    if (ret <= 0) {
        mp_raise_OSError((ret < 0) ? errno : EINVAL);
    }
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_inet_pton_obj, mp_socket_inet_pton);

static mp_obj_t mp_socket_inet_ntop(mp_obj_t af_in, mp_obj_t packed_ip_in) {
    int af = mp_obj_get_int(af_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(packed_ip_in, &bufinfo, MP_BUFFER_READ);
    if (bufinfo.len != mp_socket_af_addr_size(af)) {
        mp_raise_ValueError(NULL);
    }
    vstr_t vstr;
    vstr_init(&vstr, mp_socket_af_str_size(af));
    const char *s = inet_ntop(af, bufinfo.buf, vstr.buf, vstr.alloc);
    if (!s) {
        mp_raise_OSError(errno);
    }
    vstr.len = strnlen(s, vstr.alloc);
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_inet_ntop_obj, mp_socket_inet_ntop);

static mp_obj_t mp_socket_inet_aton(mp_obj_t ip_string) {
    return mp_socket_inet_pton(MP_OBJ_NEW_SMALL_INT(AF_INET), ip_string);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_inet_aton_obj, mp_socket_inet_aton);

static mp_obj_t mp_socket_inet_ntoa(mp_obj_t packed_ip) {
    return mp_socket_inet_ntop(MP_OBJ_NEW_SMALL_INT(AF_INET), packed_ip);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_inet_ntoa_obj, mp_socket_inet_ntoa);

static mp_obj_t mp_socket_sethostname(mp_obj_t name_in) {
    const char *name = mp_obj_str_get_str(name_in);
    int ret = setenv("HOSTNAME", name, 1);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_sethostname_obj, mp_socket_sethostname);

static mp_obj_t mp_socket_if_nametoindex(mp_obj_t name_in) {
    const char *name = mp_obj_str_get_str(name_in);
    unsigned index = if_nametoindex(name);
    if (index == 0) {
        mp_raise_OSError(ENXIO);
    }
    return MP_OBJ_NEW_SMALL_INT(index);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_if_nametoindex_obj, mp_socket_if_nametoindex);

static mp_obj_t mp_socket_if_indextoname(mp_obj_t index_in) {
    unsigned index = mp_obj_get_uint(index_in);
    vstr_t vstr;
    vstr_init(&vstr, IF_NAMESIZE);
    char *ret = if_indextoname(index, vstr.buf);
    if (!ret) {
        mp_raise_OSError(errno);
    }
    vstr.len = strnlen(vstr.buf, IF_NAMESIZE);
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_if_indextoname_obj, mp_socket_if_indextoname);

struct mp_socket_if_nameindex_node {
    nlr_jump_callback_node_t nlr;
    struct if_nameindex *ifs;
};

static void mp_socket_if_nameindex_cb(void *ctx) {
    struct mp_socket_if_nameindex_node *node = ctx;
    if_freenameindex(node->ifs);
}

static mp_obj_t mp_socket_if_nameindex(void) {
    struct if_nameindex *ifs = if_nameindex();
    if (!ifs) {
        mp_raise_OSError(errno);
    }
    
    struct mp_socket_if_nameindex_node node;
    nlr_push_jump_callback(&node.nlr, mp_socket_if_nameindex_cb);
    node.ifs = ifs;

    mp_obj_t list = mp_obj_new_list(0, NULL);
    for (int i = 0; ifs[i].if_index && strlen(ifs[i].if_name); i++) {
        mp_obj_t items[] = { 
            MP_OBJ_NEW_SMALL_INT(ifs[i].if_index),
            mp_obj_new_str_from_cstr(ifs[i].if_name),
        };
        mp_obj_list_append(list, mp_obj_new_tuple(2, items));
    }
    
    nlr_pop_jump_callback(true);
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_socket_if_nameindex_obj, mp_socket_if_nameindex);

static mp_obj_t mp_socket_create_connection(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_address, MP_QSTR_timeout, MP_QSTR_source_address, 0 };
    mp_obj_t gai_args[2];
    mp_obj_t timeout = MP_SOCKET_GLOBAL_DEFAULT_TIMEOUT;
    mp_obj_t source_address = mp_const_none;
    parse_args_and_kw_map(n_args, args, kw_args, "(OO)|OO", kws, &gai_args[0], &gai_args[1], &timeout, &source_address);

    mp_obj_t list = mp_socket_getaddrinfo(2, gai_args, NULL);
    size_t list_len;
    mp_obj_t *list_items;
    mp_obj_list_get(list, &list_len, &list_items);
    int last_error = 0;
    for (size_t i = 0; i < list_len; i++) {
        size_t tuple_len;
        mp_obj_t *tuple_items;
        mp_obj_tuple_get(list_items[i], &tuple_len, &tuple_items);
        assert(tuple_len == 5);

        mp_make_new_fun_t new_sock = MP_OBJ_TYPE_GET_SLOT(&mp_type_socket, make_new);
        mp_obj_t socket = new_sock(&mp_type_socket, 3, 0, tuple_items);

        mp_obj_t sock_args[3];
        if (timeout != MP_SOCKET_GLOBAL_DEFAULT_TIMEOUT) {
            mp_load_method(socket, MP_QSTR_settimeout, sock_args);
            sock_args[2] = timeout;
            mp_call_method_n_kw(1, 0, sock_args);
        }

        if (source_address != mp_const_none) {
            mp_load_method(socket, MP_QSTR_bind, sock_args);
            sock_args[2] = source_address;
            mp_call_method_n_kw(1, 0, sock_args);
        }

        mp_load_method(socket, MP_QSTR_connect_ex, sock_args);
        sock_args[2] = tuple_items[4];
        mp_obj_t ret = mp_call_method_n_kw(1, 0, sock_args);

        last_error = mp_obj_get_int(ret);
        if (last_error == 0) {
            return socket;
        }

        mp_load_method(socket, MP_QSTR_close, sock_args);
        mp_call_method_n_kw(0, 0, sock_args);
    }
    mp_raise_OSError(last_error);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_socket_create_connection_obj, 1, mp_socket_create_connection);

static mp_obj_t mp_socket_socketpair(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_family, MP_QSTR_type, MP_QSTR_proto, 0 };
    mp_int_t family = AF_UNSPEC;
    mp_int_t sock_type = SOCK_STREAM;
    mp_int_t proto = 0;
    parse_args_and_kw_map(n_args, args, kw_args, "|iii", kws, &family, &sock_type, &proto);

    int ret = -1;
    errno = EAFNOSUPPORT;
    int fds[2];
    const int families[3] = { AF_UNIX, AF_INET, AF_INET6 };
    for (int i = 0; i < 3; i++) {
        if ((family != AF_UNSPEC) && (family != families[i])) {
            continue;
        }
        ret = socketpair(families[i], sock_type, proto, fds);
        if ((ret >= 0) || (errno != EAFNOSUPPORT)) {
            break;
        }
    }
    mp_os_check_ret(ret);

    mp_make_new_fun_t socket = MP_OBJ_TYPE_GET_SLOT(&mp_type_socket, make_new);
    mp_obj_t sock_args[2] = { MP_OBJ_NEW_QSTR(MP_QSTR_fileno), MP_OBJ_NULL };
    mp_obj_t items[2];
    for (int i = 0; i < 2; i++) {
        sock_args[1] = MP_OBJ_NEW_SMALL_INT(fds[i]);
        items[i] = socket(&mp_type_socket, 0, 1, sock_args);
    }

    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_socket_socketpair_obj, 0, mp_socket_socketpair);

static mp_obj_t mp_socket_getdefaulttimeout(void) {
    if (isnan(mp_socket_defaulttimeout)) {
        return mp_const_none;
    }
    else {
        return mp_obj_new_float(mp_socket_defaulttimeout);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_socket_getdefaulttimeout_obj, mp_socket_getdefaulttimeout);

static mp_obj_t mp_socket_setdefaulttimeout(mp_obj_t timeout_in) {
    mp_float_t timeout = NAN;
    if (timeout_in != mp_const_none) {
        timeout = mp_obj_get_float(timeout_in);
        if (timeout < 0) {
            mp_raise_ValueError(NULL);
        }
    }
    mp_socket_defaulttimeout = timeout;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_setdefaulttimeout_obj, mp_socket_setdefaulttimeout);

static mp_obj_t mp_socket_init(void) {
    mp_socket_defaulttimeout = NAN;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_socket_init_obj, mp_socket_init);

static const mp_rom_map_elem_t mp_module_socket_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_socket) },
    { MP_ROM_QSTR(MP_QSTR___init__),        MP_ROM_PTR(&mp_socket_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_error),           MP_ROM_PTR(&mp_type_OSError) },
    { MP_ROM_QSTR(MP_QSTR_gaierror),        MP_ROM_PTR(&mp_type_gaierror) },
    { MP_ROM_QSTR(MP_QSTR_timeout),         MP_ROM_PTR(&mp_type_TimeoutError) },
    { MP_ROM_QSTR(MP_QSTR_socket),          MP_ROM_PTR(&mp_type_socket) },
    { MP_ROM_QSTR(MP_QSTR_SocketType),      MP_ROM_PTR(&mp_type_socket) },

    { MP_ROM_QSTR(MP_QSTR_create_connection),   MP_ROM_PTR(&mp_socket_create_connection_obj) },
    { MP_ROM_QSTR(MP_QSTR_getdefaulttimeout),   MP_ROM_PTR(&mp_socket_getdefaulttimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_setdefaulttimeout),   MP_ROM_PTR(&mp_socket_setdefaulttimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_socketpair),          MP_ROM_PTR(&mp_socket_socketpair_obj) },

    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_os_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_getaddrinfo),     MP_ROM_PTR(&mp_socket_getaddrinfo_obj) },
    { MP_ROM_QSTR(MP_QSTR_getfqdn),         MP_ROM_PTR(&mp_socket_getfqdn_obj) },
    { MP_ROM_QSTR(MP_QSTR_gethostbyaddr),   MP_ROM_PTR(&mp_socket_gethostbyaddr_obj) },
    { MP_ROM_QSTR(MP_QSTR_gethostbyname),   MP_ROM_PTR(&mp_socket_gethostbyname_obj) },
    { MP_ROM_QSTR(MP_QSTR_gethostname),     MP_ROM_PTR(&mp_socket_gethostname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getnameinfo),     MP_ROM_PTR(&mp_socket_getnameinfo_obj) },
    { MP_ROM_QSTR(MP_QSTR_ntohl),           MP_ROM_PTR(&mp_socket_ntohl_obj) },
    { MP_ROM_QSTR(MP_QSTR_ntohs),           MP_ROM_PTR(&mp_socket_ntohs_obj) },
    { MP_ROM_QSTR(MP_QSTR_htonl),           MP_ROM_PTR(&mp_socket_htonl_obj) },
    { MP_ROM_QSTR(MP_QSTR_htons),           MP_ROM_PTR(&mp_socket_htons_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_aton),       MP_ROM_PTR(&mp_socket_inet_aton_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_ntoa),       MP_ROM_PTR(&mp_socket_inet_ntoa_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_pton),       MP_ROM_PTR(&mp_socket_inet_pton_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_ntop),       MP_ROM_PTR(&mp_socket_inet_ntop_obj) },
    { MP_ROM_QSTR(MP_QSTR_sethostname),     MP_ROM_PTR(&mp_socket_sethostname_obj) },
    { MP_ROM_QSTR(MP_QSTR_if_nameindex),    MP_ROM_PTR(&mp_socket_if_nameindex_obj) },
    { MP_ROM_QSTR(MP_QSTR_if_nametoindex),  MP_ROM_PTR(&mp_socket_if_nametoindex_obj) },
    { MP_ROM_QSTR(MP_QSTR_if_indextoname),  MP_ROM_PTR(&mp_socket_if_indextoname_obj) },
    

    { MP_ROM_QSTR(MP_QSTR_AF_INET),         MP_ROM_INT(AF_INET) },
    { MP_ROM_QSTR(MP_QSTR_AF_INET6),        MP_ROM_INT(AF_INET6) },
    { MP_ROM_QSTR(MP_QSTR_AF_UNSPEC),       MP_ROM_INT(AF_UNSPEC) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_STREAM),     MP_ROM_INT(SOCK_STREAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_DGRAM),      MP_ROM_INT(SOCK_DGRAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_RAW),        MP_ROM_INT(SOCK_RAW) },
    { MP_ROM_QSTR(MP_QSTR_SOL_SOCKET),      MP_ROM_INT(SOL_SOCKET) },
    { MP_ROM_QSTR(MP_QSTR_SOMAXCONN),       MP_ROM_INT(SOMAXCONN) },
    { MP_ROM_QSTR(MP_QSTR_has_ipv6),        MP_ROM_INT(LWIP_IPV6) },
    { MP_ROM_QSTR(MP_QSTR_SHUT_RD),         MP_ROM_INT(SHUT_RD) },
    { MP_ROM_QSTR(MP_QSTR_SHUT_WR),         MP_ROM_INT(SHUT_WR) },
    { MP_ROM_QSTR(MP_QSTR_SHUT_RDWR),       MP_ROM_INT(SHUT_RDWR) },

    { MP_ROM_QSTR(MP_QSTR_AI_PASSIVE),      MP_ROM_INT(AI_PASSIVE) },
    { MP_ROM_QSTR(MP_QSTR_AI_CANONNAME),    MP_ROM_INT(AI_CANONNAME) },
    { MP_ROM_QSTR(MP_QSTR_AI_NUMERICHOST),  MP_ROM_INT(AI_NUMERICHOST) },
    { MP_ROM_QSTR(MP_QSTR_AI_NUMERICSERV),  MP_ROM_INT(AI_NUMERICSERV) },
    { MP_ROM_QSTR(MP_QSTR_AI_V4MAPPED),     MP_ROM_INT(AI_V4MAPPED) },
    { MP_ROM_QSTR(MP_QSTR_AI_ALL),          MP_ROM_INT(AI_ALL) },
    { MP_ROM_QSTR(MP_QSTR_AI_ADDRCONFIG),   MP_ROM_INT(AI_ADDRCONFIG) },

    { MP_ROM_QSTR(MP_QSTR_NI_NOFQDN),       MP_ROM_INT(NI_NOFQDN) },
    { MP_ROM_QSTR(MP_QSTR_NI_NUMERICHOST),  MP_ROM_INT(NI_NUMERICHOST) },
    { MP_ROM_QSTR(MP_QSTR_NI_NAMEREQD),     MP_ROM_INT(NI_NAMEREQD) },
    { MP_ROM_QSTR(MP_QSTR_NI_NUMERICSERV),  MP_ROM_INT(NI_NUMERICSERV) },
    { MP_ROM_QSTR(MP_QSTR_NI_NUMERICSCOPE), MP_ROM_INT(NI_NUMERICSCOPE) },
    { MP_ROM_QSTR(MP_QSTR_NI_DGRAM),        MP_ROM_INT(NI_DGRAM) },

    { MP_ROM_QSTR(MP_QSTR_EAI_AGAIN),       MP_ROM_INT(EAI_AGAIN) },
    { MP_ROM_QSTR(MP_QSTR_EAI_BADFLAGS),    MP_ROM_INT(EAI_BADFLAGS) },
    { MP_ROM_QSTR(MP_QSTR_EAI_FAIL),        MP_ROM_INT(EAI_FAIL) },
    { MP_ROM_QSTR(MP_QSTR_EAI_FAMILY),      MP_ROM_INT(EAI_FAMILY) },
    { MP_ROM_QSTR(MP_QSTR_EAI_MEMORY),      MP_ROM_INT(EAI_MEMORY) },
    { MP_ROM_QSTR(MP_QSTR_EAI_NONAME),      MP_ROM_INT(EAI_NONAME) },
    { MP_ROM_QSTR(MP_QSTR_EAI_SERVICE),     MP_ROM_INT(EAI_SERVICE) },
    { MP_ROM_QSTR(MP_QSTR_EAI_SOCKTYPE),    MP_ROM_INT(EAI_SOCKTYPE) },
    { MP_ROM_QSTR(MP_QSTR_EAI_SYSTEM),      MP_ROM_INT(EAI_SYSTEM) },
    { MP_ROM_QSTR(MP_QSTR_EAI_OVERFLOW),    MP_ROM_INT(EAI_OVERFLOW) },

    { MP_ROM_QSTR(MP_QSTR_IPPROTO_IP),      MP_ROM_INT(IPPROTO_IP) },
    { MP_ROM_QSTR(MP_QSTR_IPPROTO_IPV6),    MP_ROM_INT(IPPROTO_IPV6) },
    { MP_ROM_QSTR(MP_QSTR_IPPROTO_TCP),     MP_ROM_INT(IPPROTO_TCP) },
    { MP_ROM_QSTR(MP_QSTR_IPPROTO_UDP),     MP_ROM_INT(IPPROTO_UDP) },

    { MP_ROM_QSTR(MP_QSTR_SO_ACCEPTCONN),   MP_ROM_INT(SO_ACCEPTCONN) },
    { MP_ROM_QSTR(MP_QSTR_SO_DOMAIN),       MP_ROM_INT(SO_DOMAIN) },
    { MP_ROM_QSTR(MP_QSTR_SO_ERROR),        MP_ROM_INT(SO_ERROR) },
    { MP_ROM_QSTR(MP_QSTR_SO_KEEPALIVE),    MP_ROM_INT(SO_KEEPALIVE) },
    { MP_ROM_QSTR(MP_QSTR_SO_PROTOCOL),     MP_ROM_INT(SO_PROTOCOL) },
    { MP_ROM_QSTR(MP_QSTR_SO_REUSEADDR),    MP_ROM_INT(SO_REUSEADDR) },
    { MP_ROM_QSTR(MP_QSTR_SO_TYPE),         MP_ROM_INT(SO_TYPE) },

    { MP_ROM_QSTR(MP_QSTR_TCP_NODELAY),     MP_ROM_INT(TCP_NODELAY) },

    { MP_ROM_QSTR(MP_QSTR__GLOBAL_DEFAULT_TIMEOUT), MP_SOCKET_GLOBAL_DEFAULT_TIMEOUT },
};
static MP_DEFINE_CONST_DICT(mp_module_socket_globals, mp_module_socket_globals_table);

const mp_obj_module_t mp_module_socket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_socket_globals,
};

MP_REGISTER_MODULE(MP_QSTR_socket, mp_module_socket);
