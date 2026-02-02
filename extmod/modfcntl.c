// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <memory.h>
#include <sys/ioctl.h>

#include "extmod/modos_newlib.h"


static uintptr_t mp_fcntl_load_arg(mp_obj_t arg_in, void **tmp) {
    if (mp_obj_is_int(arg_in)) {
        return mp_obj_get_int(arg_in);
    } else {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(arg_in, &bufinfo, MP_BUFFER_READ);
        void *ret = bufinfo.buf;
        *tmp = NULL;
        if ((bufinfo.len < IOCTL_MAX_ARG_SIZE) || !mp_get_buffer(arg_in, &bufinfo, MP_BUFFER_RW)) {
            ret = *tmp = m_malloc(IOCTL_MAX_ARG_SIZE);
            memcpy(ret, bufinfo.buf, bufinfo.len);
        }
        return (uintptr_t)ret;
    }
}

static void mp_fcntl_save_arg(mp_obj_t arg_in, void *tmp) {
    mp_buffer_info_t bufinfo;
    if (tmp && mp_get_buffer(arg_in, &bufinfo, MP_BUFFER_RW)) {
        memcpy(bufinfo.buf, tmp, MIN(bufinfo.len, IOCTL_MAX_ARG_SIZE));
    }
    m_free(tmp);
}

static mp_obj_t mp_fcntl_fcntl(size_t n_args, const mp_obj_t *args) {
    int fd = mp_os_get_fd(args[0]);
    int cmd = mp_obj_get_int(args[1]);
    mp_obj_t arg_in = (n_args > 2) ? args[2] : MP_OBJ_NEW_SMALL_INT(0);
    void *tmp = NULL;
    uintptr_t arg = mp_fcntl_load_arg(arg_in, &tmp);
    int ret = fcntl(fd, cmd, arg);
    mp_fcntl_save_arg(arg_in, tmp);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_fcntl_fcntl_obj, 2, 3, mp_fcntl_fcntl);

static mp_obj_t mp_fcntl_ioctl(size_t n_args, const mp_obj_t *args) {
    int fd = mp_os_get_fd(args[0]);
    unsigned long request = mp_obj_get_uint(args[1]);
    mp_obj_t arg_in = (n_args > 2) ? args[2] : MP_OBJ_NEW_SMALL_INT(0);
    void *tmp;
    uintptr_t arg = mp_fcntl_load_arg(arg_in, &tmp);
    int ret = ioctl(fd, request, arg);
    mp_fcntl_save_arg(arg_in, tmp);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_fcntl_ioctl_obj, 2, 3, mp_fcntl_ioctl);

static const mp_rom_map_elem_t mp_module_fcntl_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_fcntl) },
    { MP_ROM_QSTR(MP_QSTR_fcntl),       MP_ROM_PTR(&mp_fcntl_fcntl_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl),       MP_ROM_PTR(&mp_fcntl_ioctl_obj) },
    { MP_ROM_QSTR(MP_QSTR_F_GETFL),     MP_ROM_INT(F_GETFL) },
    { MP_ROM_QSTR(MP_QSTR_F_SETFL),     MP_ROM_INT(F_SETFL) },
};
static MP_DEFINE_CONST_DICT(mp_module_fcntl_globals, mp_module_fcntl_globals_table);

const mp_obj_module_t mp_module_fcntl = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_fcntl_globals,
};
MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_fcntl, mp_module_fcntl);
