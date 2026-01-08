// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "mbedtls/oid.h"

#include "extmod/ssl/modssl.h"


// static mp_obj_t mp_ssl_make_name(const mbedtls_x509_name *name) {
//     mp_obj_t list = mp_obj_new_list(0, NULL);
//     while (name) {
//         const char *short_name = NULL;
//         mbedtls_oid_get_attr_short_name(&name->oid, &short_name);

//         mp_obj_t items[2] = {
//             mp_obj_new_str_from_cstr(short_name),
//             mp_obj_new_str((char *)name->val.p, name->val.len),
//         };
//         mp_obj_list_append(list, mp_obj_new_tuple(2, items));
//         name = name->next;
//     }

//     size_t len;
//     mp_obj_t *items;
//     mp_obj_list_get(list, &len, &items);
//     return mp_obj_new_tuple(len, items);
// }

// static mp_obj_t mp_ssl_make_time(const mbedtls_x509_time *time) {
//     struct tm tm = {
//         .tm_sec = time->sec,
//         .tm_min = time->min,
//         .tm_hour = time->hour,
//         .tm_mday = time->day,
//         .tm_mon = time->mon - 1,
//         .tm_year = time->year - 1900,
//     };
//     vstr_t vstr;
//     vstr_init(&vstr, 32);
//     vstr.len = strftime(vstr.buf, vstr.alloc, "%b %e %H:%M:%S %Y %Z", &tm);
//     return mp_obj_new_str_from_vstr(&vstr);
// }

__attribute__((visibility("hidden")))
mp_obj_t mp_ssl_make_cert(const mbedtls_x509_crt *cert, bool binary_form) {
    if (binary_form) {
        return mp_obj_new_bytes(cert->raw.p, cert->raw.len);
    }

    vstr_t vstr;
    vstr_init(&vstr, 1024);
    int ret = mbedtls_x509_crt_info(vstr.buf, vstr.alloc, "", cert);
    mp_ssl_check_ret(ret);
    vstr.len = ret;
    return mp_obj_new_str_from_vstr(&vstr);
    

    // mp_obj_dict_t *dict = mp_obj_new_dict(0);
    
    // mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_subject), mp_ssl_make_name(&cert->subject));

    // mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_issuer), mp_ssl_make_name(&cert->issuer));

    // mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_version), mp_obj_new_int(cert->version));

    // vstr_t vstr;
    // vstr_init(&vstr, 64);
    // vstr.len = mbedtls_x509_serial_gets(vstr.buf, vstr.alloc, &cert->serial);
    // mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_serial), mp_obj_new_str_from_vstr(&vstr));
    
    // mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_notBefore), mp_ssl_make_time(&cert->valid_from));

    // mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_notAfter), mp_ssl_make_time(&cert->valid_to));

    // return dict;   
}
