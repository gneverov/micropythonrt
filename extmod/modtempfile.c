// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "morelib/vfs.h"

#include "extmod/modos_newlib.h"
#include "py/objstr.h"
#include "py/parseargs.h"
#include "py/runtime.h"


static mp_obj_t mp_tempfile_tempdir = mp_const_none;

static const MP_DEFINE_STR_OBJ(mp_tempfile_tempprefix_obj, "tmp");

static mp_obj_t mp_tempfile_gettempdir(void) {
    if (mp_tempfile_tempdir == mp_const_none) {
        const char *tempdir = NULL;
        const char *env_names[] = { "TMPDIR", "TEMP", "TMP", NULL };
        for (const char **env_name = env_names; *env_name && !tempdir; env_name++) {
            tempdir = getenv(*env_name);
        }
        mp_tempfile_tempdir = mp_obj_new_str_from_cstr(tempdir ? tempdir : ".");
    }
    return mp_tempfile_tempdir;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_tempfile_gettempdir_obj, mp_tempfile_gettempdir);

static mp_obj_t mp_tempfile_gettempprefix(void) {
    return MP_ROM_PTR((mp_obj_str_t *)&mp_tempfile_tempprefix_obj);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_tempfile_gettempprefix_obj, mp_tempfile_gettempprefix);

static mp_obj_t mp_tempfile_mkstemp(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    const qstr kws[] = { MP_QSTR_suffix, MP_QSTR_prefix, MP_QSTR_dir, MP_QSTR_text, 0 };
    const char *suffix = NULL;
    const char *prefix = NULL;
    const char *dir = NULL;
    parse_args_and_kw_map(n_args, args, kwargs, "|zzzp", kws, &suffix, &prefix, &dir, NULL);

    if (!suffix) {
        suffix = "";
    }
    if (!prefix) {
        prefix = mp_obj_str_get_str(MP_ROM_PTR((mp_obj_str_t *)&mp_tempfile_tempprefix_obj));
    }
    if (!dir) {
        dir = mp_obj_str_get_str(mp_tempfile_gettempdir());
    }
    char *template = malloc(256);
    if (!template) {
        mp_raise_OSError(errno);
    }
    snprintf(template, 256, "%s/%sXXXXXX%s", dir, prefix, suffix);
    vfs_path_buffer_t vfs_path;
    int ret = vfs_expand_path(&vfs_path, template);
    free(template);
    mp_os_check_ret(ret);

    template = vfs_path.begin;
    int fd = mkstemps(template, strlen(suffix));
    mp_os_check_ret(fd);

    mp_obj_t items[2] = {
        MP_OBJ_NEW_SMALL_INT(fd),
        mp_obj_new_str_from_cstr(template),
    };
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_tempfile_mkstemp_obj, 0, mp_tempfile_mkstemp);

static mp_obj_t mp_tempfile_getattr(mp_obj_t attr) {
    switch (MP_OBJ_QSTR_VALUE(attr)) {
        case MP_QSTR_tempdir:
            return mp_tempfile_tempdir;
        default:
            return MP_OBJ_NULL;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_tempfile_getattr_obj, mp_tempfile_getattr);

// static mp_obj_t mp_tempfile_NamedTemporaryFile(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
//     const qstr kws[] = {
//         MP_QSTR_mode, MP_QSTR_buffering, MP_QSTR_encoding, MP_QSTR_newline,
//         MP_QSTR_suffix, MP_QSTR_prefix, MP_QSTR_dir, MP_QSTR_delete,
//         MP_QSTR_errors, MP_QSTR_delete_on_close, 0
//     };
//     mp_obj_t mode = mp_obj_new_str_from_cstr("w+b");
//     mp_obj_t buffering = MP_OBJ_NEW_SMALL_INT(-1);
//     mp_obj_t encoding = mp_const_none;
//     mp_obj_t newline = mp_const_none;
//     mp_obj_t suffix = mp_const_none;
//     mp_obj_t prefix = mp_const_none;
//     mp_obj_t dir = mp_const_none;
//     mp_obj_t delete = mp_const_true;
//     mp_obj_t errors = mp_const_none;
//     mp_obj_t delete_on_close = mp_const_true;
//     parse_args_and_kw(n_args, n_kw, args, "|OOOOOOOO$OO", kws);

//     mp_obj_t mkstemp_args[] = { suffix, prefix, dir, };
//     mp_obj_t file = mp_tempfile_mkstemp(3, mkstemp_args, NULL);

//     mp_obj_t open_args[] = { file, mode, buffering, encoding, errors, newline };
//     mp_builtin_open(6, open_args, NULL);

// }

static const mp_rom_map_elem_t mp_module_tempfile_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_tempfile) },
    { MP_ROM_QSTR(MP_QSTR___getattr__),     MP_ROM_PTR(&mp_tempfile_getattr_obj) },

    { MP_ROM_QSTR(MP_QSTR_mkstemp),         MP_ROM_PTR(&mp_tempfile_mkstemp_obj) },

    { MP_ROM_QSTR(MP_QSTR_gettempdir),      MP_ROM_PTR(&mp_tempfile_gettempdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_gettempprefix),   MP_ROM_PTR(&mp_tempfile_gettempprefix_obj) },
};
static MP_DEFINE_CONST_DICT(mp_module_tempfile_globals, mp_module_tempfile_globals_table);

const mp_obj_module_t mp_module_tempfile = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_tempfile_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_tempfile, mp_module_tempfile);
