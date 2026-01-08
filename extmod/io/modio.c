// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

#include "extmod/io/modio.h"
#include "extmod/modos_newlib.h"
#include "py/builtin.h"
#include "py/extras.h"
#include "py/parseargs.h"
#include "py/runtime.h"


static_assert(MP_SEEK_SET == SEEK_SET);
static_assert(MP_SEEK_CUR == SEEK_CUR);
static_assert(MP_SEEK_END == SEEK_END);

mp_import_stat_t mp_import_stat(const char *path) {
    struct stat buf;
    int ret = stat(path, &buf);
    if (ret >= 0) {
        if (S_ISDIR(buf.st_mode)) {
            return MP_IMPORT_STAT_DIR;
        }
        if (S_ISREG(buf.st_mode)) {
            return MP_IMPORT_STAT_FILE;
        }
    }
    return MP_IMPORT_STAT_NO_EXIST;
}

mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    mp_obj_t file;
    mp_obj_t mode = MP_OBJ_NEW_QSTR(MP_QSTR_r);
    mp_int_t buffering = -1;
    mp_obj_t encoding = mp_const_none;
    mp_obj_t errors = mp_const_none;
    mp_obj_t newline = mp_const_none;
    int closefd = 1;
    mp_obj_t opener = mp_const_none;
    const qstr kws[] = { MP_QSTR_file, MP_QSTR_mode, MP_QSTR_buffering, MP_QSTR_encoding, MP_QSTR_errors, MP_QSTR_newline, MP_QSTR_closefd, MP_QSTR_opener, 0 };
    parse_args_and_kw_map(n_args, pos_args, kw_args, "O|OiOOOpO", kws, &file, &mode, &buffering, &encoding, &errors, &newline, &closefd, &opener);

    const char *mode_str = mp_obj_str_get_str(mode);
    bool text = true;
    while (*mode_str) {
        switch (*mode_str++) {
            case 'b':
                text = false;
                break;
            case 't':
                text = true;
                break;
        }
    }
    if (buffering < 0) {
        buffering = BUFSIZ;
    }
    if (!text) {
        if (buffering == 0) {
            mp_obj_t file_args[4] = { file, mode, mp_obj_new_bool(closefd), opener };
            return mp_obj_make_new(&mp_type_io_file, 4, 0, file_args);
        }
        else if (buffering > 0) {
            return mp_io_buffer_new(&mp_type_io_buffer, file, mode, buffering, closefd);
        }
        else {
            mp_raise_ValueError(NULL);
        }
    }
    else {
        if (buffering > 0) {
            return mp_io_buffer_new(&mp_type_io_text, file, mode, buffering, closefd);
        }
        else {
            mp_raise_ValueError(NULL);
        }        
    }
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);


void mp_io_print(void *data, const char *str, size_t len) {
    mp_obj_t obj = data;
    const mp_stream_p_t *stream = mp_get_stream_raise(obj, MP_STREAM_OP_WRITE);
    int errcode;
    if (stream->write(obj, str, len, &errcode) == MP_STREAM_ERROR) {
        mp_raise_OSError(errcode);
    }
}

// sys stdio
__attribute__((visibility("hidden")))
mp_obj_io_buffer_t mp_sys_stdin_obj = {
    .base = { &mp_type_io_text },
    .flags = FREAD,
};

__attribute__((visibility("hidden")))
mp_obj_io_buffer_t mp_sys_stdout_obj = {
    .base = { &mp_type_io_text },
    .flags = FWRITE,
};

__attribute__((visibility("hidden")))
mp_obj_io_buffer_t mp_sys_stderr_obj = {
    .base = { &mp_type_io_text },
    .flags = FWRITE,
};

__attribute__((constructor, visibility("hidden")))
void mp_sys_init_stdio(void) {
    mp_sys_stdin_obj.file = stdin;
    mp_sys_stdout_obj.file = stdout;
    mp_sys_stderr_obj.file = stderr;
}

static const mp_rom_map_elem_t mp_module_io_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_io) },
    { MP_ROM_QSTR(MP_QSTR_open),        MP_ROM_PTR(&mp_builtin_open_obj) },

    { MP_ROM_QSTR(MP_QSTR_StringIO),    MP_ROM_PTR(&mp_type_stringio) },
    #if MICROPY_PY_IO_BYTESIO
    { MP_ROM_QSTR(MP_QSTR_BytesIO),     MP_ROM_PTR(&mp_type_bytesio) },
    #endif

    { MP_ROM_QSTR(MP_QSTR_IOBase),      MP_ROM_PTR(&mp_type_io_base) },
    { MP_ROM_QSTR(MP_QSTR_FileIO),      MP_ROM_PTR(&mp_type_io_file) },
    { MP_ROM_QSTR(MP_QSTR_BufferedReader), MP_ROM_PTR(&mp_type_io_buffer) },
    { MP_ROM_QSTR(MP_QSTR_BufferedWriter), MP_ROM_PTR(&mp_type_io_buffer) },
    { MP_ROM_QSTR(MP_QSTR_BufferedRandom), MP_ROM_PTR(&mp_type_io_buffer) },
    { MP_ROM_QSTR(MP_QSTR_TextIOWrapper), MP_ROM_PTR(&mp_type_io_text) },

    { MP_ROM_QSTR(MP_QSTR_DEFAULT_BUFFER_SIZE), MP_ROM_INT(MP_OS_DEFAULT_BUFFER_SIZE) },
};

static MP_DEFINE_CONST_DICT(mp_module_io_globals, mp_module_io_globals_table);

const mp_obj_module_t mp_module_io = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_io_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_io, mp_module_io);
