// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <errno.h>
#include <time.h>

#include "py/obj.h"


#ifndef MP_OS_DEFAULT_BUFFER_SIZE
#define MP_OS_DEFAULT_BUFFER_SIZE 256
#endif

#define MP_OS_CALL(ret, func, ...) for (;;) { \
        MP_THREAD_GIL_EXIT(); \
        ret = func(__VA_ARGS__); \
        MP_THREAD_GIL_ENTER(); \
        if ((ret >= 0) || (errno != EINTR)) { \
            break; \
        } \
        else { \
            mp_handle_pending(true); \
        } \
}

#define MP_OS_CALL_NULL(ret, func, ...) for (;;) { \
        MP_THREAD_GIL_EXIT(); \
        ret = func(__VA_ARGS__); \
        MP_THREAD_GIL_ENTER(); \
        if ((ret != NULL) || (errno != EINTR)) { \
            break; \
        } \
        else { \
            mp_handle_pending(true); \
        } \
}

mp_obj_t mp_os_check_ret_filename(int ret, mp_obj_t filename);

static inline mp_obj_t mp_os_check_ret(int ret) {
    return mp_os_check_ret_filename(ret, MP_OBJ_NULL);
}

bool mp_os_nonblocking_ret(int ret);

int mp_os_get_fd(mp_obj_t obj_in);

MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_open_obj);

MP_DECLARE_CONST_FUN_OBJ_1(mp_os_close_obj);

int mp_os_read_vstr(int fd, vstr_t *vstr, size_t size);

int mp_os_write_str(int fd, const char *str, size_t len);

static inline mp_obj_t mp_time_to_obj(const time_t *t) {
    return mp_obj_new_int_from_ll(*t);
}

static inline mp_obj_t mp_timespec_to_obj(const struct timespec *ts) {
    return mp_obj_new_float(ts->tv_sec + MICROPY_FLOAT_CONST(1e-9) * ts->tv_nsec);
}

static inline mp_obj_t mp_timespec_to_ns(const struct timespec *ts) {
    return mp_obj_new_int_from_ll(1000000000 * ts->tv_sec + ts->tv_nsec);
}

void mp_obj_to_time(mp_obj_t obj, time_t *t);

void mp_obj_to_timespec(mp_obj_t obj, struct timespec *tp);

void mp_ns_to_timespec(mp_obj_t obj, struct timespec *tp);
