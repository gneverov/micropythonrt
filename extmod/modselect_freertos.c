// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <stdatomic.h>
#include <sys/select.h>
#include "morelib/poll.h"

#include "FreeRTOS.h"

#include "extmod/modos_newlib.h"
#include "py/runtime.h"
#include "py/obj.h"


typedef struct {
    mp_obj_base_t base;
    struct pollfd *fds;
    nfds_t nfds;
    size_t size;
    atomic_flag flag;
} select_obj_poll_t;

static mp_obj_t select_poll_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    select_obj_poll_t *self = m_new_obj(select_obj_poll_t);
    self->base.type = type;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t select_poll_register(size_t n_args, const mp_obj_t *args) {
    select_obj_poll_t *self = MP_OBJ_TO_PTR(args[0]);
    int fd = mp_os_get_fd(args[1]);
    uint events = (n_args > 2) ? mp_obj_get_uint(args[2]) : POLLIN | POLLPRI | POLLOUT;

    int i = 0;
    for (; i < self->nfds; i++) {
        if (self->fds[i].fd == fd) {
            goto found;
        }
    }
    i = self->nfds++;
    self->fds = m_realloc(self->fds, self->nfds * sizeof(struct pollfd));
    self->fds[i].fd = fd;

found:
    self->fds[i].events = events;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(select_poll_register_obj, 2, 3, select_poll_register);

static mp_obj_t select_poll_unregister(mp_obj_t self_in, mp_obj_t fd_in) {
    select_obj_poll_t *self = MP_OBJ_TO_PTR(self_in);
    int fd = mp_os_get_fd(fd_in);

    int i = 0;
    for (; i < self->nfds; i++) {
        if (self->fds[i].fd == fd) {
            goto found;
        }
    }
    mp_raise_type(&mp_type_KeyError);

found:
    self->nfds--;
    for (; i < self->nfds; i++) {
        self->fds[i] = self->fds[i + 1];
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(select_poll_unregister_obj, select_poll_unregister);

static mp_obj_t select_poll_modify(mp_obj_t self_in, mp_obj_t fd_in, mp_obj_t events_in) {
    select_obj_poll_t *self = MP_OBJ_TO_PTR(self_in);
    int fd = mp_os_get_fd(fd_in);
    uint events = mp_obj_get_uint(events_in);

    for (int i = 0; i < self->nfds; i++) {
        if (self->fds[i].fd == fd) {
            self->fds[i].events = events;
            return mp_const_none;
        }
    }
    mp_raise_OSError(ENOENT);
}
static MP_DEFINE_CONST_FUN_OBJ_3(select_poll_modify_obj, select_poll_modify);

static int select_poll_locked(atomic_flag *flag, struct pollfd fds[], nfds_t nfds, int timeout) {
    if (atomic_flag_test_and_set(flag)) {
        errno = EBUSY;
        return -1;
    }
    int ret = poll(fds, nfds, timeout);
    atomic_flag_clear(flag);
    return ret;
}

static mp_obj_t select_poll_poll(size_t n_args, const mp_obj_t *args) {
    select_obj_poll_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_int_t timeout_ms = -1;
    if (n_args > 1) {
        if (mp_obj_is_float(args[1])) {
            mp_float_t f = mp_obj_get_float(args[1]);
            if (f > (mp_float_t)INT_MAX) {
                mp_raise_type(&mp_type_OverflowError);
            } else if (f >= MICROPY_FLOAT_CONST(0.0)) {
                timeout_ms = f;
            }
        } else if (args[1] != mp_const_none) {
            timeout_ms = mp_obj_get_int(args[1]);
        }
    }

    nfds_t nfds = self->nfds;
    struct pollfd *fds = m_malloc(nfds * sizeof(struct pollfd));
    memcpy(fds, self->fds, nfds * sizeof(struct pollfd));

    int ret;
    MP_OS_CALL(ret, select_poll_locked, &self->flag, fds, nfds, timeout_ms);
    if ((ret < 0) && (errno == EBUSY)) {
        mp_raise_type(&mp_type_RuntimeError);
    }
    mp_os_check_ret(ret);

    mp_obj_t result = mp_obj_new_list(0, NULL);
    for (int i = 0; i < nfds; i++) {
        if (fds[i].revents) {
            mp_obj_t items[] = { MP_OBJ_NEW_SMALL_INT(fds[i].fd), MP_OBJ_NEW_SMALL_INT(fds[i].revents) };
            mp_obj_t tuple = mp_obj_new_tuple(2, items);
            mp_obj_list_append(result, tuple);
        }
    }
    m_free(fds);
    return result;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(select_poll_poll_obj, 1, 2, select_poll_poll);

static const mp_rom_map_elem_t select_poll_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_register),    MP_ROM_PTR(&select_poll_register_obj) },
    { MP_ROM_QSTR(MP_QSTR_unregister),  MP_ROM_PTR(&select_poll_unregister_obj) },
    { MP_ROM_QSTR(MP_QSTR_modify),      MP_ROM_PTR(&select_poll_modify_obj) },
    { MP_ROM_QSTR(MP_QSTR_poll),        MP_ROM_PTR(&select_poll_poll_obj) },
};
static MP_DEFINE_CONST_DICT(select_poll_locals_dict, select_poll_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    select_type_poll,
    MP_QSTR_poll,
    MP_TYPE_FLAG_NONE,
    make_new, &select_poll_make_new,
    locals_dict, &select_poll_locals_dict
    );

#if MICROPY_PY_SELECT_SELECT
// Helper function to convert Python list to fd_set
static int py_list_to_fd_set(mp_obj_t list_obj, fd_set *fdset, int *max_fd) {
    FD_ZERO(fdset);
    size_t len;
    mp_obj_t *items;
    mp_obj_get_array(list_obj, &len, &items);

    for (size_t i = 0; i < len; i++) {
        int fd = mp_os_get_fd(items[i]);
        if (fd < 0 || fd >= FD_SETSIZE) {
            mp_raise_ValueError(MP_ERROR_TEXT("file descriptor out of range"));
        }
        FD_SET(fd, fdset);
        if (fd > *max_fd) {
            *max_fd = fd;
        }
    }
    return len;
}

// Helper function to convert fd_set back to Python list
static mp_obj_t fd_set_to_py_list(fd_set *fdset, mp_obj_t orig_list) {
    mp_obj_t result_list = mp_obj_new_list(0, NULL);
    size_t len;
    mp_obj_t *items;
    mp_obj_get_array(orig_list, &len, &items);

    for (size_t i = 0; i < len; i++) {
        int fd = mp_os_get_fd(items[i]);
        if (fd >= 0 && fd < FD_SETSIZE && FD_ISSET(fd, fdset)) {
            mp_obj_list_append(result_list, items[i]);
        }
    }
    return result_list;
}

static mp_obj_t mod_select_select(size_t n_args, const mp_obj_t *args) {
    mp_obj_t rlist = args[0];
    mp_obj_t wlist = args[1];
    mp_obj_t xlist = args[2];
    mp_obj_t timeout_obj = (n_args > 3) ? args[3] : mp_const_none;

    fd_set readfds, writefds, errorfds;
    int max_fd = -1;

    // Convert Python lists to fd_sets
    py_list_to_fd_set(rlist, &readfds, &max_fd);
    py_list_to_fd_set(wlist, &writefds, &max_fd);
    py_list_to_fd_set(xlist, &errorfds, &max_fd);

    // Handle timeout
    struct timeval tv;
    if (timeout_obj != mp_const_none) {
        mp_float_t timeout_val = mp_obj_get_float(timeout_obj);
        if (timeout_val < 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("timeout must be non-negative"));
        }
        tv.tv_sec = (long)timeout_val;
        tv.tv_usec = (long)((timeout_val - tv.tv_sec) * 1000000);
    }

    // Call the underlying select system call
    int ret;
    MP_OS_CALL(ret, select, max_fd + 1, &readfds, &writefds, &errorfds, &tv);
    mp_os_check_ret(ret);

    // Convert results back to Python lists
    mp_obj_t ready_rlist = fd_set_to_py_list(&readfds, rlist);
    mp_obj_t ready_wlist = fd_set_to_py_list(&writefds, wlist);
    mp_obj_t ready_xlist = fd_set_to_py_list(&errorfds, xlist);

    // Return tuple of (ready_read, ready_write, ready_error)
    mp_obj_t result_items[] = {ready_rlist, ready_wlist, ready_xlist};
    return mp_obj_new_tuple(3, result_items);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_select_select_obj, 3, 4, mod_select_select);
#endif

static const mp_rom_map_elem_t select_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_select) },
    #if MICROPY_PY_SELECT_SELECT
    { MP_ROM_QSTR(MP_QSTR_select),      MP_ROM_PTR(&mod_select_select_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_poll),        MP_ROM_PTR(&select_type_poll) },
    { MP_ROM_QSTR(MP_QSTR_POLLIN),      MP_ROM_INT(POLLIN) },
    { MP_ROM_QSTR(MP_QSTR_POLLPRI),     MP_ROM_INT(POLLPRI) },
    { MP_ROM_QSTR(MP_QSTR_POLLOUT),     MP_ROM_INT(POLLOUT) },
    { MP_ROM_QSTR(MP_QSTR_POLLERR),     MP_ROM_INT(POLLERR) },
    { MP_ROM_QSTR(MP_QSTR_POLLHUP),     MP_ROM_INT(POLLHUP) },
    { MP_ROM_QSTR(MP_QSTR_POLLNVAL),    MP_ROM_INT(POLLNVAL) },
};

static MP_DEFINE_CONST_DICT(select_module_globals, select_module_globals_table);

const mp_obj_module_t select_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&select_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_select, select_module);
