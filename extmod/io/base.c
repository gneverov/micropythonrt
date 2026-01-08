// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <memory.h>
#include <stdio.h>

#include "extmod/io/modio.h"
#include "extmod/modos_newlib.h"
#include "py/objstr.h"
#include "py/runtime.h"


static const mp_obj_type_t mp_type_io_native;

static mp_obj_t mp_io_native_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    static const mp_obj_base_t self = { &mp_type_io_native };
    return MP_OBJ_FROM_PTR(&self);
}

enum none_mode {
    NONE_ZERO,          // None means return zero
    NONE_NONBLOCK,      // None means return non-blocking stream error
    NONE_ERROR,         // None means raise TypeError
};

static mp_obj_t mp_io_stream_call_obj(size_t n_args, size_t n_kw, const mp_obj_t *args, int *errcode) {
    nlr_buf_t nlr;
    mp_obj_t ret_obj;
    if (nlr_push(&nlr) == 0) {
        ret_obj = mp_call_method_n_kw(n_args, n_kw, args);
        nlr_pop();
    } else if (mp_obj_is_os_error(nlr.ret_val, errcode)) {
        ret_obj = MP_OBJ_NULL;
    } else {
        nlr_raise(nlr.ret_val);
    }
    return ret_obj;
}

static mp_uint_t mp_io_stream_call(size_t n_args, size_t n_kw, const mp_obj_t *args, int *errcode, enum none_mode none) {
    mp_obj_t ret_obj = mp_io_stream_call_obj(n_args, n_kw, args, errcode);
    
    if (ret_obj == MP_OBJ_NULL) {
        return MP_STREAM_ERROR;
    }

    if (ret_obj == mp_const_none) {
        if (none == NONE_ZERO) {
            return 0;
        }
        if (none == NONE_NONBLOCK) {
            *errcode = MP_EAGAIN;
            return MP_STREAM_ERROR;
        }
    }
    return mp_obj_get_int(ret_obj);
}

static mp_uint_t mp_io_stream_read(mp_obj_t obj, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_t args[3];
    mp_load_method(obj, MP_QSTR_read, args);
    args[2] = mp_obj_new_int(size);
    mp_obj_t ret_obj = mp_io_stream_call_obj(1, 0, args, errcode);

    if (ret_obj == MP_OBJ_NULL) {
        return MP_STREAM_ERROR;
    }

    if (ret_obj == mp_const_none) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;        
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(ret_obj, &bufinfo, MP_BUFFER_READ);
    assert(bufinfo.len <= size);
    memcpy(buf, bufinfo.buf, bufinfo.len);
    return bufinfo.len;
}

static mp_uint_t mp_io_stream_write(mp_obj_t obj, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_t args[3];
    mp_load_method(obj, MP_QSTR_write, args);
    args[2] = mp_obj_new_bytes(buf, size);
    return mp_io_stream_call(1, 0, args, errcode, NONE_NONBLOCK);
}

__attribute__((visibility("hidden")))
mp_uint_t mp_io_stream_ioctl(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode) {
    switch (request) {
        case MP_STREAM_FLUSH: {
            mp_obj_t args[2];
            mp_load_method(obj, MP_QSTR_flush, args);
            return mp_io_stream_call(0, 0, args, errcode, NONE_ZERO);
        }
        case MP_STREAM_SEEK: {
            struct mp_stream_seek_t *s = (void *)arg;
            mp_obj_t args[4];
            mp_load_method(obj, MP_QSTR_seek, args);
            args[2] = mp_obj_new_int(s->offset);
            args[3] = MP_OBJ_NEW_SMALL_INT(s->whence);
            return mp_io_stream_call(2, 0, args, errcode, NONE_ERROR);
        }
        case MP_STREAM_CLOSE: {
            mp_obj_t args[2];
            mp_load_method(obj, MP_QSTR_close, args);
            return mp_io_stream_call(0, 0, args, errcode, NONE_ZERO);
        }
        case MP_STREAM_GET_FILENO: {
            mp_obj_t args[2];
            mp_load_method(obj, MP_QSTR_fileno, args);
            return mp_io_stream_call(0, 0, args, errcode, NONE_ERROR);
        }
        default: {
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
        }
    }
}

__attribute__((visibility("hidden")))
const mp_stream_p_t mp_io_stream_p = {
    .read = mp_io_stream_read,
    .write = mp_io_stream_write,
    .ioctl = mp_io_stream_ioctl,
};

static mp_obj_t mp_io_base_enter(mp_obj_t self_in) {
    mp_obj_t closed = mp_load_attr(self_in, MP_QSTR_closed);
    if (mp_obj_is_true(closed)) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed file"));
    }    
    return self_in;
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_base_enter_obj, mp_io_base_enter);

static mp_obj_t mp_io_base_exit(size_t n_args, const mp_obj_t *args) {
    mp_obj_t new_args[2];
    mp_load_method(args[0], MP_QSTR_close, new_args);
    return mp_call_method_n_kw(0, 0, new_args);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_base_exit_obj, 1, 4, mp_io_base_exit);

static mp_obj_t mp_io_base_close(mp_obj_t self_in) {
    mp_store_attr(self_in, MP_QSTR_closed, mp_const_true);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_base_close_obj, mp_io_base_close);

static mp_obj_t mp_io_base_none(mp_obj_t self_in) {
    mp_io_base_enter(self_in);
    return mp_const_none;
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_base_none_obj, mp_io_base_none);

static mp_obj_t mp_io_base_false(mp_obj_t self_in) {
    mp_io_base_enter(self_in);
    return mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_base_false_obj, mp_io_base_false);

__attribute__((visibility("hidden")))
mp_obj_t mp_io_base_iternext(mp_obj_t self_in) {
    mp_obj_t args[2];
    mp_load_method(self_in, MP_QSTR_readline, args);
    mp_obj_t ret = mp_call_method_n_kw(0, 0, args);
    return mp_obj_is_true(ret) ? ret : MP_OBJ_STOP_ITERATION;
}

static mp_obj_t mp_io_base_readline(size_t n_args, const mp_obj_t *args) {
    size_t size = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;

    mp_obj_t read_args[3];
    mp_load_method(args[0], MP_QSTR_read, read_args);
    read_args[2] = MP_OBJ_NEW_SMALL_INT(1);

    vstr_t out_buffer;
    vstr_init(&out_buffer, MIN(size, MP_OS_DEFAULT_BUFFER_SIZE));
    while (vstr_len(&out_buffer) < size) {
        mp_obj_t ret = mp_call_method_n_kw(1, 0, read_args);
        if (ret == mp_const_none) {
            break;
        }
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(ret, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len == 0) {
            break;
        }
        assert(bufinfo.len == 1);
        byte b = *(byte *)bufinfo.buf;
        vstr_add_byte(&out_buffer, b);
        if (b == '\n') {
            break;
        }        
    }
    return mp_obj_new_bytes_from_vstr(&out_buffer);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_base_readline_obj, 1, 2, mp_io_base_readline);

static mp_obj_t mp_io_base_readlines(size_t n_args, const mp_obj_t *args) {
    mp_int_t hint = -1;
    if ((n_args > 1) && (args[1] != mp_const_none)) {
        hint = mp_obj_get_int(args[1]);
    }
    mp_obj_t list = mp_obj_new_list(0, NULL);
    mp_obj_t readline_args[2];
    mp_load_method(args[0], MP_QSTR_readline, readline_args);
    while (hint) {
        mp_obj_t line = mp_call_method_n_kw(0, 0, readline_args);
        if (!mp_obj_is_true(line)) {
            break;
        }
        mp_obj_list_append(list, line);
        if (mp_obj_is_str_or_bytes(line)) {
            size_t len;
            mp_obj_str_get_data(line, &len);
            hint -= len;
        }
    }
    return list;
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_base_readlines_obj, 1, 2, mp_io_base_readlines);

static mp_obj_t mp_io_base_tell(mp_obj_t self_in) {
    mp_obj_t args[4];
    mp_load_method(self_in, MP_QSTR_seek, args);
    args[2] = MP_OBJ_NEW_SMALL_INT(0);
    args[3] = MP_OBJ_NEW_SMALL_INT(SEEK_CUR);
    return mp_call_method_n_kw(2, 0, args);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_base_tell_obj, mp_io_base_tell);

static mp_obj_t mp_io_base_writelines(mp_obj_t self_in, mp_obj_t lines_in) {
    mp_obj_t args[3];
    mp_load_method(self_in, MP_QSTR_write, args);
    args[2] = mp_iternext(lines_in);
    while (args[2] != MP_OBJ_STOP_ITERATION) {
        mp_call_method_n_kw(1, 0, args);
        args[2] = mp_iternext(lines_in);
    }
    return mp_const_none;
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_2(mp_io_base_writelines_obj, mp_io_base_writelines);

static const mp_rom_map_elem_t mp_io_base_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___enter__),   MP_ROM_PTR(&mp_io_base_enter_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),    MP_ROM_PTR(&mp_io_base_exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_closed),      MP_ROM_FALSE },

    { MP_ROM_QSTR(MP_QSTR_close),       MP_ROM_PTR(&mp_io_base_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),       MP_ROM_PTR(&mp_io_base_none_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),      MP_ROM_PTR(&mp_io_base_false_obj) },
    { MP_ROM_QSTR(MP_QSTR_readable),    MP_ROM_PTR(&mp_io_base_false_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline),    MP_ROM_PTR(&mp_io_base_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines),   MP_ROM_PTR(&mp_io_base_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_seekable),    MP_ROM_PTR(&mp_io_base_false_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell),        MP_ROM_PTR(&mp_io_base_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR_writable),    MP_ROM_PTR(&mp_io_base_false_obj) },
    { MP_ROM_QSTR(MP_QSTR_writelines),  MP_ROM_PTR(&mp_io_base_writelines_obj) },
};
static MP_DEFINE_CONST_DICT(mp_io_base_locals_dict, mp_io_base_locals_dict_table);

__attribute__((visibility("hidden")))
MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_io_base,
    MP_QSTR_IOBase,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT | MP_TYPE_FLAG_TRUE_SELF,
    make_new, mp_io_native_make_new,
    // print, mp_io_buffer_print,
    iter, &mp_io_base_iternext,
    protocol, &mp_io_stream_p,
    locals_dict, &mp_io_base_locals_dict
    );