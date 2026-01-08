// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <stdio.h>
#include <stdio-bufio.h>
#include <unistd.h>

#include "extmod/io/modio.h"
#include "extmod/modos_newlib.h"
#include "py/parseargs.h"
#include "py/runtime.h"

// An extra flags value to mark that a file is seekable. 
// Added to the other F* flags from fcntl.h.
#define FSEEK 0x04


__attribute__((visibility("hidden")))
mp_obj_io_buffer_t *mp_io_buffer_get(mp_obj_t self_in, int flags) {
    mp_obj_io_buffer_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->file) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed file"));
    }
    if ((self->flags & flags) != flags) {
        mp_raise_ValueError(MP_ERROR_TEXT("unsupported operation"));
    }
    return self;
}

static void mp_io_buffer_init(mp_obj_io_buffer_t *self, FILE *file, size_t buffer_size, int flags) {
    switch (buffer_size) {
        case 0:
            setvbuf(file, NULL, _IONBF, 0);
            break;
        case 1:
            setvbuf(file, NULL, _IOLBF, BUFSIZ);
            break;
        default:
            setvbuf(file, NULL, _IOFBF, buffer_size);
            break;
    }

    self->file = file;
    self->flags = flags;
    self->flags |= (lseek(fileno(file), 0, SEEK_CUR) >= 0) ? FSEEK : 0;
}

__attribute__((visibility("hidden")))
mp_obj_t mp_io_buffer_new(const mp_obj_type_t *type, mp_obj_t name, mp_obj_t mode_obj, size_t buffer_size, bool closefd) {
    const char *mode_str = mp_obj_str_get_str(mode_obj);
    char mode[3] = { 0 };
    int flags = 0;
    while (*mode_str) {
        switch (*mode_str++) {
            case 'r':
                mode[0] = 'r';
                flags = FREAD;
                break;
            case 'w':
                mode[0] = 'w';
                flags = FWRITE | FTRUNC;
                break;
            case 'x':
                mode[0] = 'w';
                flags = FWRITE | FEXCL;
                break;                
            case 'a':
                mode[0] = 'a';
                flags = FWRITE | FAPPEND;
                break;
            case '+':
                mode[1] = '+';
                flags |= FREAD | FWRITE;
                break;
        }
    }

    FILE *file;
    if (!mp_obj_is_int(name)) {
        if (!closefd) {
            mp_raise_ValueError(NULL);
        }
        if (flags & FEXCL) {
            int fd = open(mp_obj_str_get_str(name), O_CREAT | O_EXCL);
            mp_os_check_ret(fd);
            close(fd);
        }
        file = fopen(mp_obj_str_get_str(name), mode);
    }
    else {
        int fd = mp_obj_get_int(name);
        int actual_flags = fcntl(fd, F_GETFL);
        mp_os_check_ret(actual_flags);
        if (((actual_flags + 1) & O_ACCMODE & flags) != (flags & O_ACCMODE)) {
            mp_raise_OSError(EBADF);
        }
        file = fdopen(fd, mode);
        if (file && !closefd) {
            // hack to not close the fd when the FILE pointer is closed
            assert(file->flags & __SBUF);
            struct __file_bufio *pf = (struct __file_bufio *) file;
            pf->close_int = NULL;
        }
    }
    if (!file) {
        mp_raise_OSError(errno);
    }
    mp_obj_io_buffer_t *self = mp_obj_malloc_with_finaliser(mp_obj_io_buffer_t, type);
    mp_io_buffer_init(self, file, buffer_size, flags & O_ACCMODE);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mp_io_buffer_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_raw, MP_QSTR_buffer_size, 0 };
    mp_obj_t raw_obj;
    mp_int_t buffer_size = MP_OS_DEFAULT_BUFFER_SIZE;
    parse_args_and_kw(n_args, n_kw, args, "O|i", kws, &raw_obj, &buffer_size);

    int fd = mp_os_get_fd(raw_obj);
    mp_obj_t mode = mp_load_attr(raw_obj, MP_QSTR_mode);
    return mp_io_buffer_new(type, MP_OBJ_NEW_SMALL_INT(fd), mode, buffer_size, true);
}

__attribute__((visibility("hidden")))
void mp_io_buffer_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_obj_io_buffer_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_SENTINEL) {
        return;
    }
    switch (attr) {
        case MP_QSTR_closed:
            dest[0] = mp_obj_new_bool(!self->file);
            break;
        default:
            dest[1] = MP_OBJ_SENTINEL;
            break;
    }
}

static mp_obj_t mp_io_buffer_close(mp_obj_t self_in) {
    mp_obj_io_buffer_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->file) {
        fclose(self->file);
    }
    self->file = NULL;
    return mp_const_none;
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_close_obj, mp_io_buffer_close);

// static mp_obj_t mp_io_buffer_detach(mp_obj_t self_in) {
//     mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, 0);
//     int fd = mp_os_check_ret(fileno(self->file));
//     fclose(self->file);
//     self->file = NULL;
//     return mp_io_file_alloc(fd);
// }
// static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_detach_obj, mp_io_buffer_detach);

static mp_obj_t mp_io_buffer_fileno(mp_obj_t self_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, 0);
    int fd = fileno(self->file);
    return MP_OBJ_NEW_SMALL_INT(fd);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_fileno_obj, mp_io_buffer_fileno);

static mp_obj_t mp_io_buffer_flush(mp_obj_t self_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, 0);
    mp_os_check_ret(fflush(self->file));
    return mp_const_none;
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_flush_obj, mp_io_buffer_flush);

static mp_obj_t mp_io_buffer_isatty(mp_obj_t self_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, 0);
    int fd = fileno(self->file);
    return mp_obj_new_bool(isatty(fd) > 0);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_isatty_obj, mp_io_buffer_isatty);

static size_t mp_io_fread(FILE *file, void *buf, size_t size) {
    int ret;
    MP_OS_CALL(ret, fread, buf, 1, size, file);
    return ret;
}

static mp_obj_t mp_io_buffer_read(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(args[0], FREAD);
    size_t size = n_args > 1 ? mp_obj_get_int(args[1]) : -1;

    vstr_t out_buffer;
    vstr_init(&out_buffer, MIN(size, MP_OS_DEFAULT_BUFFER_SIZE));
    while (out_buffer.len < size) {
        if (out_buffer.len == out_buffer.alloc) {
            vstr_hint_size(&out_buffer, MP_OS_DEFAULT_BUFFER_SIZE);
        }
        size_t br = mp_io_fread(self->file, out_buffer.buf + out_buffer.len, MIN(size, out_buffer.alloc) - out_buffer.len);
        if (br > 0) {
            out_buffer.len += br;
        }
        else if (feof(self->file) || out_buffer.len) {
            break;
        }
        else {
            mp_raise_OSError(errno);
        }
    }
    return mp_obj_new_bytes_from_vstr(&out_buffer);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_buffer_read_obj, 1, 2, mp_io_buffer_read);

static mp_obj_t mp_io_buffer_readable(mp_obj_t self_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, 0);
    return mp_obj_new_bool(self->flags & FREAD);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_readable_obj, mp_io_buffer_readable);

static mp_obj_t mp_io_buffer_readinto(mp_obj_t self_in, mp_obj_t b_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, FREAD);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(b_in, &bufinfo, MP_BUFFER_WRITE);

    size_t br = mp_io_fread(self->file, bufinfo.buf, bufinfo.len);
    if ((br > 0) || feof(self->file)) {
        return mp_obj_new_int(br);
    }
    else {
        mp_raise_OSError(errno);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_buffer_readinto_obj, mp_io_buffer_readinto);

static int mp_io_freadline(FILE *file, vstr_t *out_buffer, size_t max_len) {
    int c = EOF;
    while (out_buffer->len < max_len) {
        c = fgetc(file);
        if (c == EOF) {
            break;
        }
        out_buffer->buf[out_buffer->len++] = c;
        if (c == '\n') {
            break;
        }
    }
    return c;
}

static mp_obj_t mp_io_buffer_readline(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(args[0], FREAD);
    size_t size = ((n_args > 1) && (args[1] != mp_const_none)) ? mp_obj_get_int(args[1]) : -1;

    vstr_t out_buffer;
    vstr_init(&out_buffer, MIN(size, MP_OS_DEFAULT_BUFFER_SIZE));
    while (vstr_len(&out_buffer) < size) {
        if (out_buffer.len == out_buffer.alloc) {
            vstr_hint_size(&out_buffer, MP_OS_DEFAULT_BUFFER_SIZE);
        }
        // fgets just calls fgetc in a loop, so it is easier to create our own loop
        int c;
        MP_OS_CALL(c, mp_io_freadline, self->file, &out_buffer, MIN(size, out_buffer.alloc));
        if (c != EOF) {
            if (c == '\n') {
                break;
            }             
        }
        else if (feof(self->file) || out_buffer.len) {
            break;
        } 
        else {
            mp_raise_OSError(errno);
        }
    }
    return mp_obj_new_bytes_from_vstr(&out_buffer);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_buffer_readline_obj, 1, 2, mp_io_buffer_readline);

static mp_obj_t mp_io_buffer_seek(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(args[0], FSEEK);
    long offset = mp_obj_get_int(args[1]);
    int whence = n_args > 2 ? mp_obj_get_int(args[2]) : SEEK_SET;
    mp_os_check_ret(fseek(self->file, offset, whence));
    return mp_os_check_ret(ftell(self->file));
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_buffer_seek_obj, 2, 3, mp_io_buffer_seek);

static mp_obj_t mp_io_buffer_seekable(mp_obj_t self_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, 0);
    return mp_obj_new_bool(self->flags & FSEEK);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_seekable_obj, mp_io_buffer_seekable);

static mp_obj_t mp_io_buffer_tell(mp_obj_t self_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, FSEEK);
    return mp_os_check_ret(ftell(self->file));
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_tell_obj, mp_io_buffer_tell);

static mp_obj_t mp_io_buffer_writable(mp_obj_t self_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, 0);
    return mp_obj_new_bool(self->flags & FWRITE);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_1(mp_io_buffer_writable_obj, mp_io_buffer_writable);

static size_t mp_io_fwrite(FILE *file, const void *buf, size_t size) {
    int ret;
    MP_OS_CALL(ret, fwrite, buf, 1, size, file);
    return ret;
}

static mp_obj_t mp_io_buffer_write(mp_obj_t self_in, mp_obj_t b_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, FWRITE);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(b_in, &bufinfo, MP_BUFFER_READ);

    size_t bw = mp_io_fwrite(self->file, bufinfo.buf, bufinfo.len);
    if ((bw > 0) || feof(self->file)) {
        return mp_obj_new_int(bw);
    }
    else {
        mp_raise_OSError(errno);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_buffer_write_obj, mp_io_buffer_write);

static mp_uint_t mp_io_buffer_stream_read(mp_obj_t obj, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(obj, FREAD);
    size_t br = mp_io_fread(self->file, buf, size);
    if (br > 0) {
        return br;
    }
    if (feof(self->file)) {
        return 0;
    }
    *errcode = errno;
    return MP_STREAM_ERROR;
}

static mp_uint_t mp_io_buffer_stream_write(mp_obj_t obj, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(obj, FWRITE);
    size_t bw = mp_io_fwrite(self->file, buf, size);
    if (bw > 0) {
        return bw;
    }
    if (feof(self->file)) {
        return 0;
    }    
    *errcode = errno;
    return MP_STREAM_ERROR;    
}

__attribute__((visibility("hidden")))
const mp_stream_p_t mp_io_buffer_stream_p = {
    .read = mp_io_buffer_stream_read,
    .write = mp_io_buffer_stream_write,
    .ioctl = mp_io_stream_ioctl,
};

static const mp_rom_map_elem_t mp_io_buffer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),     MP_ROM_PTR(&mp_io_buffer_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),   MP_ROM_PTR(&mp_io_base_enter_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),    MP_ROM_PTR(&mp_io_base_exit_obj) },

    { MP_ROM_QSTR(MP_QSTR_close),       MP_ROM_PTR(&mp_io_buffer_close_obj) },
    // { MP_ROM_QSTR(MP_QSTR_detach),      MP_ROM_PTR(&mp_io_buffer_detach_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno),      MP_ROM_PTR(&mp_io_buffer_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),       MP_ROM_PTR(&mp_io_buffer_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),      MP_ROM_PTR(&mp_io_buffer_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),        MP_ROM_PTR(&mp_io_buffer_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readable),    MP_ROM_PTR(&mp_io_buffer_readable_obj) },    
    { MP_ROM_QSTR(MP_QSTR_readinto),    MP_ROM_PTR(&mp_io_buffer_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline),    MP_ROM_PTR(&mp_io_buffer_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines),   MP_ROM_PTR(&mp_io_base_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek),        MP_ROM_PTR(&mp_io_buffer_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_seekable),    MP_ROM_PTR(&mp_io_buffer_seekable_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell),        MP_ROM_PTR(&mp_io_buffer_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR_writable),    MP_ROM_PTR(&mp_io_buffer_writable_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),       MP_ROM_PTR(&mp_io_buffer_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_writelines),  MP_ROM_PTR(&mp_io_base_writelines_obj) },
};
static MP_DEFINE_CONST_DICT(mp_io_buffer_locals_dict, mp_io_buffer_locals_dict_table);

__attribute__((visibility("hidden")))
MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_io_buffer,
    MP_QSTR_BufferedIO,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT | MP_TYPE_FLAG_TRUE_SELF,
    make_new, mp_io_buffer_make_new,
    // print, mp_io_buffer_print,
    attr, mp_io_buffer_attr,
    iter, &mp_io_base_iternext,
    protocol, &mp_io_buffer_stream_p,
    locals_dict, &mp_io_buffer_locals_dict,
    parent, &mp_type_io_base
    );
