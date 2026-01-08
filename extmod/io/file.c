// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "extmod/io/modio.h"
#include "extmod/modos_newlib.h"
#include "py/parseargs.h"
#include "py/runtime.h"


static mp_obj_io_file_t *mp_io_file_get(mp_obj_t self_in) {
    mp_obj_io_file_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->fd == -1) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed file"));
    }
    return self;
}

static mp_obj_t mp_io_file_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_name, MP_QSTR_mode, MP_QSTR_closefd, MP_QSTR_opener, 0 };
    mp_obj_t name;
    mp_obj_t mode = MP_OBJ_NEW_QSTR(MP_QSTR_r);
    int closefd = 1;
    mp_obj_t opener = mp_const_none;
    parse_args_and_kw(n_args, n_kw, args, "O|OpO", kws, &name, &mode, &closefd, &opener);

    const char *mode_str = mp_obj_str_get_str(mode);
    int mode_rw = 0, mode_x = 0;
    while (*mode_str) {
        switch (*mode_str++) {
            case 'r':
                mode_rw = O_RDONLY;
                break;
            case 'w':
                mode_rw = O_WRONLY;
                mode_x = O_CREAT | O_TRUNC;
                break;
            case 'x':
                mode_rw = O_WRONLY;
                mode_x = O_CREAT | O_EXCL;
                break;
            case 'a':
                mode_rw = O_WRONLY;
                mode_x = O_CREAT | O_APPEND;
                break;
            case '+':
                mode_rw = O_RDWR;
                break;
        }
    }

    mp_obj_t fd_obj = name;
    if (!mp_obj_is_int(name)) {
        if (!closefd) {
            mp_raise_ValueError(NULL);
        }
        mp_obj_t args[] = { name, MP_OBJ_NEW_SMALL_INT(mode_x | mode_rw) };
        fd_obj = mp_call_function_n_kw(opener == mp_const_none ? MP_OBJ_FROM_PTR(&mp_os_open_obj) : opener, 2, 0, args);
    }   
    mp_obj_io_file_t *self = mp_obj_malloc_with_finaliser(mp_obj_io_file_t, type);
    self->fd = mp_obj_get_int(fd_obj);
    self->flags = 0;
    self->name = name;
    self->mode = mode;
    self->closefd = closefd;

    int flags = fcntl(self->fd, F_GETFL);
    mp_os_check_ret(flags);
    self->flags = (flags + 1) & O_ACCMODE & (mode_rw + 1);
    if (self->flags != (mode_rw + 1)) {
        mp_raise_OSError(EBADF);
    }
    return MP_OBJ_FROM_PTR(self);
}

static void mp_io_file_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_obj_io_file_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_SENTINEL) {
        return;
    }
    switch (attr) {
        case MP_QSTR_closed:
            dest[0] = mp_obj_new_bool(self->fd < 0);
            break;
        case MP_QSTR_mode:
            mp_io_file_get(self_in);
            dest[0] = self->mode;
            break;
        case MP_QSTR_name:
            mp_io_file_get(self_in);
            dest[0] = self->name;
            break;
        default:
            dest[1] = MP_OBJ_SENTINEL;
            break;
    }
}

static mp_obj_t mp_io_file_close(mp_obj_t self_in) {
    mp_obj_io_file_t *self = MP_OBJ_TO_PTR(self_in);
    if ((self->fd >= 0) && self->closefd) {
        close(self->fd);
    }
    self->fd = -1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_close_obj, mp_io_file_close);

static mp_obj_t mp_io_file_fileno(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    return MP_OBJ_NEW_SMALL_INT(self->fd);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_fileno_obj, mp_io_file_fileno);

static mp_obj_t mp_io_file_isatty(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    return mp_obj_new_bool(isatty(self->fd) > 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_isatty_obj, mp_io_file_isatty);

static mp_obj_t mp_io_file_readable(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    return mp_obj_new_bool(self->flags & FREAD);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_readable_obj, mp_io_file_readable);

static mp_obj_t mp_io_file_readall(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);

    vstr_t vstr;
    vstr_init(&vstr, MP_OS_DEFAULT_BUFFER_SIZE);
    for (;;) {
        int ret = mp_os_read_vstr(self->fd, &vstr, MP_OS_DEFAULT_BUFFER_SIZE);
        if (ret == 0) {
            break;
        }
        else if (ret > 0) {
        }
        else if (errno != EAGAIN) {
            mp_raise_OSError(errno);
        }
        else if (vstr_len(&vstr)) {
            break;
        }
        else {
            return mp_const_none;
        }
    }
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_readall_obj, mp_io_file_readall);

static mp_obj_t mp_io_file_read(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_file_t *self = mp_io_file_get(args[0]);
    mp_int_t opt_size = n_args > 1 ? mp_obj_get_int(args[1]) : -1;
    if (opt_size < 0) {
        return mp_io_file_readall(args[0]);
    }

    size_t size = opt_size;
    vstr_t vstr;
    vstr_init(&vstr, size);
    int ret = mp_os_read_vstr(self->fd, &vstr, size);
    if (ret >= 0) {
        return mp_obj_new_bytes_from_vstr(&vstr);
    }
    else if (errno != EAGAIN) {
        mp_raise_OSError(errno);
    }
    else {
        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_read_obj, 1, 2, mp_io_file_read);

static mp_obj_t mp_io_file_readinto(mp_obj_t self_in, mp_obj_t b_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(b_in, &bufinfo, MP_BUFFER_WRITE);

    vstr_t vstr;
    vstr_init_fixed_buf(&vstr, bufinfo.len, bufinfo.buf);
    int ret = mp_os_read_vstr(self->fd, &vstr, bufinfo.len);
    if (ret >= 0) {
        return mp_obj_new_int(ret);
    }
    else if (errno != EAGAIN) {
        mp_raise_OSError(errno);
    }
    else {
        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_file_readinto_obj, mp_io_file_readinto);

static mp_obj_t mp_io_file_readline(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_file_t *self = mp_io_file_get(args[0]);
    size_t size = ((n_args > 1) && (args[1] != mp_const_none)) ? mp_obj_get_int(args[1]) : -1;

    vstr_t vstr;
    vstr_init(&vstr, MIN(size, MP_OS_DEFAULT_BUFFER_SIZE));
    while (vstr_len(&vstr) < size) {
        if (vstr.len == vstr.alloc) {
            vstr_hint_size(&vstr, MP_OS_DEFAULT_BUFFER_SIZE);
        }
        int ret = mp_os_read_vstr(self->fd, &vstr, 1);
        if (ret == 0) {
            break;
        } 
        else if (ret > 0) {
            if (vstr_str(&vstr)[vstr_len(&vstr) - 1] == '\n') {
                break;
            }            
        }
        else if (errno != EAGAIN) {
            mp_raise_OSError(errno);
        }
        else if (vstr_len(&vstr)) {
            break;
        }
        else {
            return mp_const_none;
        }
    }
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_readline_obj, 1, 2, mp_io_file_readline);

static mp_obj_t mp_io_file_seek(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_file_t *self = mp_io_file_get(args[0]);
    mp_int_t offset = mp_obj_get_int(args[1]);
    mp_int_t whence = n_args > 2 ? mp_obj_get_int(args[2]) : SEEK_SET;
    int ret = lseek(self->fd, offset, whence);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_seek_obj, 2, 3, mp_io_file_seek);

static mp_obj_t mp_io_file_seekable(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    int ret = lseek(self->fd, 0, SEEK_CUR);
    return mp_obj_new_bool(ret >= 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_seekable_obj, mp_io_file_seekable);

static mp_obj_t mp_io_file_truncate(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_file_t *self = mp_io_file_get(args[0]);
    size_t size;
    if ((n_args <= 1) || (args[1] == mp_const_none)) {
        size = lseek(self->fd, 0, SEEK_CUR);
    } else {
        size = mp_obj_get_int(args[1]);
    }
    int ret = ftruncate(self->fd, size);
    mp_os_check_ret(ret);
    return mp_obj_new_int(size);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_truncate_obj, 1, 2, mp_io_file_truncate);

static mp_obj_t mp_io_file_writable(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    return mp_obj_new_bool(self->flags & FWRITE);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_writable_obj, mp_io_file_writable);

static mp_obj_t mp_io_file_write(mp_obj_t self_in, mp_obj_t b_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    if (mp_obj_is_str(b_in)) {
        mp_raise_TypeError(NULL);
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(b_in, &bufinfo, MP_BUFFER_READ);
    int ret = mp_os_write_str(self->fd, bufinfo.buf, bufinfo.len);
    if (ret >= 0) {
        return mp_obj_new_int(ret);
    }
    else if (errno != EAGAIN) {
        mp_raise_OSError(errno);
    }
    else {
        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_file_write_obj, mp_io_file_write);

static const mp_rom_map_elem_t mp_io_file_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),     MP_ROM_PTR(&mp_io_file_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),   MP_ROM_PTR(&mp_io_base_enter_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),    MP_ROM_PTR(&mp_io_base_exit_obj) },

    { MP_ROM_QSTR(MP_QSTR_close),       MP_ROM_PTR(&mp_io_file_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno),      MP_ROM_PTR(&mp_io_file_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),       MP_ROM_PTR(&mp_io_base_none_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),      MP_ROM_PTR(&mp_io_file_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),        MP_ROM_PTR(&mp_io_file_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readable),    MP_ROM_PTR(&mp_io_file_readable_obj) },    
    { MP_ROM_QSTR(MP_QSTR_readall),     MP_ROM_PTR(&mp_io_file_readall_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),    MP_ROM_PTR(&mp_io_file_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline),    MP_ROM_PTR(&mp_io_file_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines),   MP_ROM_PTR(&mp_io_base_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek),        MP_ROM_PTR(&mp_io_file_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_seekable),    MP_ROM_PTR(&mp_io_file_seekable_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell),        MP_ROM_PTR(&mp_io_base_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR_truncate),    MP_ROM_PTR(&mp_io_file_truncate_obj) },
    { MP_ROM_QSTR(MP_QSTR_writable),    MP_ROM_PTR(&mp_io_file_writable_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),       MP_ROM_PTR(&mp_io_file_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_writelines),  MP_ROM_PTR(&mp_io_base_writelines_obj) },
};
static MP_DEFINE_CONST_DICT(mp_io_file_locals_dict, mp_io_file_locals_dict_table);

static mp_uint_t mp_io_file_stream_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    vstr_t vstr;
    vstr_init_fixed_buf(&vstr, size, buf);
    int ret = mp_os_read_vstr(self->fd, &vstr, size);
    if (ret < 0) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }
    return ret;
}

static mp_uint_t mp_io_file_stream_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    int ret = mp_os_write_str(self->fd, buf, size);
    if (ret < 0) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }
    return ret;
}

static const mp_stream_p_t mp_io_file_stream_p = {
    .read = mp_io_file_stream_read,
    .write = mp_io_file_stream_write,
    .ioctl = mp_io_stream_ioctl,
};

__attribute__((visibility("hidden")))
MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_io_file,
    MP_QSTR_FileIO,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    make_new, mp_io_file_make_new,
    // print, mp_io_file_print,
    attr, mp_io_file_attr,
    iter, &mp_io_base_iternext,
    protocol, &mp_io_file_stream_p,
    locals_dict, &mp_io_file_locals_dict
    );
