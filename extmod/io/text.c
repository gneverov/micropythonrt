// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "extmod/io/modio.h"
#include "extmod/modos_newlib.h"
#include "py/parseargs.h"
#include "py/runtime.h"


// static mp_obj_t mp_io_text_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
//     const qstr kws[] = { MP_QSTR_buffer, MP_QSTR_encoding, MP_QSTR_errors, MP_QSTR_newline, MP_QSTR_line_buffering, 0 };
//     mp_obj_t buffer_obj;
//     parse_args_and_kw(n_args, n_kw, args, "O|OOOp", kws, &buffer_obj, NULL, NULL, NULL, NULL);

// }

static int mp_io_text_readchar(mp_obj_io_buffer_t *self, vstr_t *vstr) {
    char buf[4];
    int c = fgetc(self->file);
    if (c == EOF) {
        return feof(self->file) ? 0 : -1;
    }
    buf[0] = c;    

    size_t n = 0;  // parsing an n-byte code point sequence
    // at the start of a new sequence
    if (buf[0] < 0x80) {
        // ASCII code point (1 byte sequence)
        n = 1;
    } else if (buf[0] < 0xC2) {
        // error
    } else if (buf[0] < 0xE0) {
        // 2 byte sequence
        n = 2;
    } else if (buf[0] < 0xF0) {
        // 3 byte sequence
        n = 3;
    } else if (buf[0] < 0xF5) {
        // 4 byte sequence
        n = 4;
    } else {
        // error;
    }

    size_t i = 1;  // at the ith byte of an n-byte code point sequence
    while (i < n) {
        c = fgetc(self->file);
        if (c == EOF) {
            break;
        }        
        buf[i] = c;
        // in the middle of a sequence
        if (buf[i] < 0x80) {
            // error
            break;
        } else if (buf[i] < 0xC0) {
            // continuation byte
        } else {
            // error
            break;
        }
        i++;
    }
 
    if (i != n) {
        // error occurred, emit surrogate escape
        char *dst = vstr_add_len(vstr, 2);
        dst[0] = 0xDC;
        dst[1] = 0x80 | (buf[0] & 0x7F);
        return 1;
    } else {
        // emit valid utf-8 sequence
        vstr_add_strn(vstr, buf, n);
        return n;
    }
}


static mp_obj_t mp_io_text_read_until(size_t n_args, const mp_obj_t *args, int nl) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(args[0], FREAD);
    size_t size = n_args > 1 ? mp_obj_get_int(args[1]) : -1;

    vstr_t out_buffer;
    vstr_init(&out_buffer, MIN(size, MP_OS_DEFAULT_BUFFER_SIZE));
    size_t count = 0;
    while (count < size) {
        if (out_buffer.len == out_buffer.alloc) {
            vstr_hint_size(&out_buffer, MP_OS_DEFAULT_BUFFER_SIZE);
        }
        int ret;
        MP_OS_CALL(ret, mp_io_text_readchar, self, &out_buffer);
        if (ret > 0) {
            count++;
            if (ret == nl) {
                break;
            }
        }
        else if ((ret == 0) || out_buffer.len) {
            break;
        }
        else {
            mp_raise_OSError(errno);
        }
    }
    return mp_obj_new_str_from_vstr(&out_buffer);
}

static mp_obj_t mp_io_text_read(size_t n_args, const mp_obj_t *args) {
    return mp_io_text_read_until(n_args, args, EOF);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_text_read_obj, 1, 2, mp_io_text_read);

static mp_obj_t mp_io_text_readline(size_t n_args, const mp_obj_t *args) {
    return mp_io_text_read_until(n_args, args, '\n');
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_text_readline_obj, 1, 2, mp_io_text_readline);

static int mp_io_text_writechar(mp_obj_io_buffer_t *self, const char *buf, size_t len) {
    assert(len > 0);
    size_t n = -1;  // parsing an n-byte code point sequence
    // at the start of a new sequence
    if (buf[0] < 0x80) {
        // ASCII code point (1 byte sequence)
        n = 1;
    } else if (buf[0] < 0xC2) {
        // error
    } else if (buf[0] < 0xE0) {
        // 2 byte sequence
        n = 2;
    } else if (buf[0] < 0xF0) {
        // 3 byte sequence
        n = 3;
    } else if (buf[0] < 0xF5) {
        // 4 byte sequence
        n = 4;
    } else {
        errno = EILSEQ;
        return -1;
    }
    size_t m = n;
    if (buf[0] == 0xDC) {
        buf++;
        m--;
    }
    for (size_t i = 0; i < m; i++) {
        if (fputc(buf[i], self->file) == EOF) {
            return -1;
        }
    }
    return n;
}

static mp_obj_t mp_io_text_write(mp_obj_t self_in, mp_obj_t b_in) {
    mp_obj_io_buffer_t *self = mp_io_buffer_get(self_in, FWRITE);
    size_t len;
    const char *buf = mp_obj_str_get_data(b_in, &len);

    size_t bw = 0;
    size_t cw = 0;
    while (bw < len) {
        size_t ret;
        MP_OS_CALL(ret, mp_io_text_writechar, self, buf + bw, len - bw);
        if (ret >= 0) {
            assert(ret > 0);
            bw += ret;
            cw++;
        }        
        else if (errno == EILSEQ) {
            mp_raise_type(&mp_type_UnicodeError);
        }
        else {
            mp_raise_OSError(errno);
        }
    }
    return mp_obj_new_int(cw);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_text_write_obj, mp_io_text_write);

static const mp_rom_map_elem_t mp_io_text_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),     MP_ROM_PTR(&mp_io_buffer_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),   MP_ROM_PTR(&mp_io_base_enter_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),    MP_ROM_PTR(&mp_io_base_exit_obj) },

    { MP_ROM_QSTR(MP_QSTR_close),       MP_ROM_PTR(&mp_io_buffer_close_obj) },
    // { MP_ROM_QSTR(MP_QSTR_detach),      MP_ROM_PTR(&mp_io_buffer_detach_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno),      MP_ROM_PTR(&mp_io_buffer_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),       MP_ROM_PTR(&mp_io_buffer_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),      MP_ROM_PTR(&mp_io_buffer_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),        MP_ROM_PTR(&mp_io_text_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readable),    MP_ROM_PTR(&mp_io_buffer_readable_obj) },    
    { MP_ROM_QSTR(MP_QSTR_readline),    MP_ROM_PTR(&mp_io_text_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines),   MP_ROM_PTR(&mp_io_base_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek),        MP_ROM_PTR(&mp_io_buffer_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_seekable),    MP_ROM_PTR(&mp_io_buffer_seekable_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell),        MP_ROM_PTR(&mp_io_buffer_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR_writable),    MP_ROM_PTR(&mp_io_buffer_writable_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),       MP_ROM_PTR(&mp_io_text_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_writelines),  MP_ROM_PTR(&mp_io_base_writelines_obj) },
};
static MP_DEFINE_CONST_DICT(mp_io_text_locals_dict, mp_io_text_locals_dict_table);

__attribute__((visibility("hidden")))
MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_io_text,
    MP_QSTR_TextIOWrapper,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    // make_new, mp_io_text_make_new,
    // print, mp_io_buffer_print,
    attr, mp_io_buffer_attr,
    iter, &mp_io_base_iternext,
    protocol, &mp_io_buffer_stream_p,
    locals_dict, &mp_io_text_locals_dict
    );
