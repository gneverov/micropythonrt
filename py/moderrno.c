/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <assert.h>
#include <string.h>

#include "py/obj.h"
#include "py/mperrno.h"

#if MICROPY_PY_ERRNO

// This list can be defined per port in mpconfigport.h to tailor it to a
// specific port's needs.  If it's not defined then we provide a default.
#ifndef MICROPY_PY_ERRNO_LIST
#define MICROPY_PY_ERRNO_LIST \
    X(EPERM) \
    X(ENOENT) \
    X(ESRCH) \
    X(EINTR) \
    X(EIO) \
    X(ENXIO) \
    X(E2BIG) \
    X(ENOEXEC) \
    X(EBADF) \
    X(ECHILD) \
    X(EAGAIN) \
    X(ENOMEM) \
    X(EACCES) \
    X(EFAULT) \
    X(EBUSY) \
    X(EEXIST) \
    X(EXDEV) \
    X(ENODEV) \
    X(ENOTDIR) \
    X(EISDIR) \
    X(EINVAL) \
    X(ENFILE) \
    X(EMFILE) \
    X(ENOTTY) \
    X(ETXTBSY) \
    X(EFBIG) \
    X(ENOSPC) \
    X(ESPIPE) \
    X(EROFS) \
    X(EMLINK) \
    X(EPIPE) \
    X(EDOM) \
    X(ERANGE) \
    X(EWOULDBLOCK) \
    X(ENOTSOCK) \
    X(EDESTADDRREQ) \
    X(EMSGSIZE) \
    X(EPROTOTYPE) \
    X(ENOPROTOOPT) \
    X(EPROTONOSUPPORT) \
    X(ESOCKTNOSUPPORT) \
    X(EOPNOTSUPP) \
    X(EPFNOSUPPORT) \
    X(EAFNOSUPPORT) \
    X(EADDRINUSE) \
    X(EADDRNOTAVAIL) \
    X(ENETDOWN) \
    X(ENETUNREACH) \
    X(ENETRESET) \
    X(ECONNABORTED) \
    X(ECONNRESET) \
    X(ENOBUFS) \
    X(EISCONN) \
    X(ENOTCONN) \
    X(ESHUTDOWN) \
    X(ETOOMANYREFS) \
    X(ETIMEDOUT) \
    X(ECONNREFUSED) \
    X(EHOSTDOWN) \
    X(EHOSTUNREACH) \
    X(EALREADY) \
    X(EINPROGRESS) \
    X(ECANCELED)
#endif

#if MICROPY_PY_ERRNO_ERRORCODE
static const mp_rom_map_elem_t errorcode_table[] = {
    #define X(e) { MP_ROM_INT(MP_##e), MP_ROM_QSTR(MP_QSTR_##e) },
    MICROPY_PY_ERRNO_LIST
#undef X
};

static const mp_obj_dict_t errorcode_dict = {
    .base = {&mp_type_dict},
    .map = {
        .all_keys_are_qstrs = 0, // keys are integers
        .is_fixed = 1,
        .is_ordered = 1,
        .used = MP_ARRAY_SIZE(errorcode_table),
        .alloc = MP_ARRAY_SIZE(errorcode_table),
        .table = (mp_map_elem_t *)(mp_rom_map_elem_t *)errorcode_table,
    },
};
#endif

static const mp_rom_map_elem_t mp_module_errno_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_errno) },
    #if MICROPY_PY_ERRNO_ERRORCODE
    { MP_ROM_QSTR(MP_QSTR_errorcode), MP_ROM_PTR(&errorcode_dict) },
    #endif

    #define X(e) { MP_ROM_QSTR(MP_QSTR_##e), MP_ROM_INT(MP_##e) },
    MICROPY_PY_ERRNO_LIST
#undef X
};

static MP_DEFINE_CONST_DICT(mp_module_errno_globals, mp_module_errno_globals_table);

const mp_obj_module_t mp_module_errno = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_errno_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_errno, mp_module_errno);

mp_obj_t mp_errno_to_str(int errcode) {
    #if MICROPY_FREERTOS
    const char *errstr = strerror(errcode);
    return mp_obj_new_str(errstr, strlen(errstr));
    #elif MICROPY_PY_ERRNO_ERRORCODE
    // We have the errorcode dict so can do a lookup using the hash map
    mp_map_elem_t *elem = mp_map_lookup((mp_map_t *)&errorcode_dict.map, MP_OBJ_NEW_SMALL_INT(errcode), MP_MAP_LOOKUP);
    if (elem == NULL) {
        return MP_OBJ_NEW_QSTR(MP_QSTRnull);
    } else {
        return elem->value;
    }
    #else
    // We don't have the errorcode dict so do a simple search in the modules dict
    for (size_t i = 0; i < MP_ARRAY_SIZE(mp_module_uerrno_globals_table); ++i) {
        if (MP_OBJ_NEW_SMALL_INT(errcode) == mp_module_errno_globals_table[i].value) {
            return mp_module_errno_globals_table[i].key;
        }
    }
    return MP_OBJ_NEW_QSTR(MP_QSTRnull);
    #endif
}

#endif // MICROPY_PY_ERRNO
