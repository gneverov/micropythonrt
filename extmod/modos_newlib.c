// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/statvfs.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "morelib/dlfcn.h"
#include "morelib/mount.h"
#include "morelib/stat.h"
#include "morelib/vfs.h"
#include "morelib/lwip/tls.h"

#include "extmod/modos_newlib.h"
#include "extmod/ssl/modssl.h"
#include "py/builtin.h"
#include "py/extras.h"
#include "py/objint.h"
#include "py/objstr.h"
#include "py/parseargs.h"
#include "py/runtime.h"


__attribute__((visibility("hidden")))
mp_obj_t mp_os_check_ret_filename(int ret, mp_obj_t filename) {
    if (ret >= 0) {
        return mp_obj_new_int(ret);
    } else if ((errno == EPROTO) && (socket_tls_errno() != 0)) {
        mp_ssl_check_ret(socket_tls_errno());
        return mp_const_none;
    } else {
        mp_raise_OSError_with_filename(errno, filename);
    }
}

__attribute__((visibility("hidden")))
bool mp_os_nonblocking_ret(int ret) {
    return (ret < 0) && (errno == EAGAIN);
}

__attribute__((visibility("hidden")))
int mp_os_get_fd(mp_obj_t obj_in) {
    if (!mp_obj_is_int(obj_in)) {
        mp_obj_t args[2];
        mp_load_method_maybe(obj_in, MP_QSTR_fileno, args);
        if (args[0] == MP_OBJ_NULL) {
            mp_raise_TypeError(NULL);
        }
        obj_in = mp_call_method_n_kw(0, 0, args);
    }
    return mp_obj_get_int(obj_in);
}

__attribute__((visibility("hidden")))
const char *mp_os_get_fspath(mp_obj_t path_in) {
    size_t len;
    const char *path = mp_obj_str_get_data(path_in, &len);
    if (strlen(path) != len) {
        mp_raise_ValueError(NULL);
    }
    return path;
}

static mp_obj_t mp_os_fspath(mp_obj_t path_in);

static const MP_DEFINE_STR_OBJ(mp_os_name_obj, "posix");

MP_REGISTER_ROOT_POINTER(mp_obj_t environ_dict);

static mp_obj_t mp_os_get_environ(void) {
    if (!MP_STATE_VM(environ_dict)) {
        mp_obj_t dict = mp_obj_new_dict(0);
        extern char **environ;
        char **env = environ;
        while (*env) {
            char *equal = strchr(*env, '=');
            if (equal) {
                mp_obj_t key = mp_obj_new_str(*env, equal - *env);
                mp_obj_t value = mp_obj_new_str_copy(&mp_type_str, (byte *)equal + 1, strlen(equal) - 1);
                mp_obj_dict_store(dict, key, value);
            }
            env++;
        }
        MP_STATE_VM(environ_dict) = dict;
    }
    return MP_STATE_VM(environ_dict);
}


// Process Parameters
// ------------------
static mp_obj_t mp_os_getenv(mp_obj_t key_in) {
    const char *key = mp_obj_str_get_str(key_in);
    char *value = getenv(key);
    if (!value) {
        return mp_const_none;
    }
    size_t len = strlen(value);
    return mp_obj_new_str_copy(&mp_type_str, (byte *)value, len);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_getenv_obj, mp_os_getenv);

static mp_obj_t mp_os_getpid(void) {
    int pid = getpid();
    return mp_obj_new_int(pid);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_getpid_obj, mp_os_getpid);

static mp_obj_t mp_os_putenv(mp_obj_t key_in, mp_obj_t value_in) {
    size_t key_len;
    const char *key = mp_obj_str_get_data(key_in, &key_len);
    size_t value_len;
    const char *value = mp_obj_str_get_data(value_in, &value_len);
    if ((key_len != strnlen(key, key_len)) || strpbrk(key, "=") || (value_len != strnlen(value, value_len))) {
        mp_raise_ValueError(NULL);
    }
    int ret = setenv(key, value, 1);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_putenv_obj, mp_os_putenv);

static mp_obj_t mp_os_strerror(mp_obj_t code_in) {
    mp_int_t code = mp_obj_get_int(code_in);
    char *s = strerror(code);
    if (!s) {
        mp_raise_ValueError(NULL);
    }
    return mp_obj_new_str_copy(&mp_type_str, (byte *)s, strlen(s));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_strerror_obj, mp_os_strerror);

static mp_obj_t mp_os_uname(void) {
    // Allocate potentially large struct on heap
    struct utsname *name = m_malloc(sizeof(struct utsname));
    int ret = uname(name);
    mp_os_check_ret(ret);

    static const qstr mp_os_uname_attrs[] = {
        MP_QSTR_sysname,
        MP_QSTR_nodename,
        MP_QSTR_release,
        MP_QSTR_version,
        MP_QSTR_machine,
    };
    mp_obj_t items[] = {
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->sysname, strnlen(name->sysname, UTSNAME_LENGTH)),
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->nodename, strnlen(name->nodename, UTSNAME_LENGTH)),
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->release, strnlen(name->release, UTSNAME_LENGTH)),
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->version, strnlen(name->version, UTSNAME_LENGTH)),
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->machine, strnlen(name->machine, UTSNAME_LENGTH)),
    };
    return mp_obj_new_attrtuple(mp_os_uname_attrs, 5, items);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_uname_obj, mp_os_uname);

static mp_obj_t mp_os_unsetenv(mp_obj_t key_in) {
    const char *key = mp_obj_str_get_str(key_in);
    int ret = unsetenv(key);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_unsetenv_obj, mp_os_unsetenv);


// File Descriptor Operations
// --------------------------
static mp_obj_t mp_os_close(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret = close(fd);
    mp_os_check_ret(ret);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_os_close_obj, mp_os_close);

static mp_obj_t mp_os_dup(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret = dup(fd);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_dup_obj, mp_os_dup);

static mp_obj_t mp_os_dup2(mp_obj_t fd1_in, mp_obj_t fd2_in) {
    mp_int_t fd1 = mp_obj_get_int(fd1_in);
    mp_int_t fd2 = mp_obj_get_int(fd2_in);
    int ret = dup2(fd1, fd2);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_dup2_obj, mp_os_dup2);

static mp_obj_t mp_os_fsync(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret = fsync(fd);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_fsync_obj, mp_os_fsync);

static mp_obj_t mp_os_get_blocking(mp_obj_t fd_in) {
    int fd = mp_os_get_fd(fd_in);
    int flags = fcntl(fd, F_GETFL);
    mp_os_check_ret(flags);
    return mp_obj_new_bool(!(flags & O_NONBLOCK));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_get_blocking_obj, mp_os_get_blocking);

static mp_obj_t mp_os_isatty(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret = isatty(fd);
    return mp_obj_new_bool(ret > 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_isatty_obj, mp_os_isatty);

static mp_obj_t mp_os_lseek(mp_obj_t fd_in, mp_obj_t pos_in, mp_obj_t whence_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    mp_int_t pos = mp_obj_get_int(pos_in);
    mp_int_t whence = mp_obj_get_int(whence_in);
    int ret = lseek(fd, pos, whence);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_3(mp_os_lseek_obj, mp_os_lseek);

static mp_obj_t mp_os_open(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    const qstr kws[] = {MP_QSTR_path, MP_QSTR_flags, MP_QSTR_mode, 0 };
    mp_obj_t path;
    mp_int_t flags;
    mp_int_t mode = 0777;
    parse_args_and_kw_map(n_args, args, kwargs, "O&i|i", kws, mp_os_fspath, &path, &flags, &mode);

    int ret;
    const char *path_str = mp_obj_str_get_str(path);
    MP_OS_CALL(ret, open, path_str, flags, mode);
    return mp_os_check_ret_filename(ret, path);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_KW(mp_os_open_obj, 0, mp_os_open);

static mp_obj_t mp_os_pipe(void) {
    int fds[2];
    int ret = pipe(fds);
    mp_os_check_ret(ret);
    mp_obj_t items[] = {
        MP_OBJ_NEW_SMALL_INT(fds[0]),
        MP_OBJ_NEW_SMALL_INT(fds[1]),
    };
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_pipe_obj, mp_os_pipe);

__attribute__((visibility("hidden")))
int mp_os_read_vstr(int fd, vstr_t *vstr, size_t size) {
    vstr_hint_size(vstr, size);
    int ret;
    MP_OS_CALL(ret, read, fd, vstr_str(vstr) + vstr_len(vstr), size);
    if (ret > 0) {
        vstr_add_len(vstr, ret);
    }
    return ret;
}

__attribute__((visibility("hidden")))
int mp_os_write_str(int fd, const char *str, size_t len) {
    int ret;
    MP_OS_CALL(ret, write, fd, str, len);
    return ret;
}

static mp_obj_t mp_os_read(mp_obj_t fd_in, mp_obj_t n_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    mp_int_t n = mp_obj_get_int(n_in);
    vstr_t buf;
    vstr_init(&buf, n);
    int ret = mp_os_read_vstr(fd, &buf, n);
    mp_os_check_ret(ret);
    return mp_obj_new_bytes_from_vstr(&buf);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_read_obj, mp_os_read);

static mp_obj_t mp_os_sendfile(size_t n_args, const mp_obj_t *args) {
    int out_fd = mp_os_get_fd(args[0]);
    int in_fd = mp_os_get_fd(args[1]);
    int count = mp_obj_get_int(args[3]);

    if (args[2] != mp_const_none) {
        mp_os_lseek(MP_OBJ_NEW_SMALL_INT(in_fd), args[2], MP_OBJ_NEW_SMALL_INT(SEEK_SET));
    }

    vstr_t buf;
    vstr_init(&buf, MP_OS_DEFAULT_BUFFER_SIZE);
    int progress = 0;
    while (progress < count) {
        int br = mp_os_read_vstr(in_fd, &buf, MIN(count - progress, MP_OS_DEFAULT_BUFFER_SIZE));
        if (br == 0) {
            break;
        }
        mp_os_check_ret(br);
        int bw = 0;
        while (bw < br) {
            int ret = mp_os_write_str(out_fd, vstr_str(&buf) + bw, br - bw);
            mp_os_check_ret(ret);
            bw += ret;
        }
        progress += br;
        vstr_clear(&buf);
    }
    return mp_obj_new_int(progress);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_sendfile_obj, 4, 4, mp_os_sendfile);

static mp_obj_t mp_os_set_blocking(mp_obj_t fd_in, mp_obj_t blocking_in) {
    int fd = mp_os_get_fd(fd_in);
    bool blocking = mp_obj_is_true(blocking_in);
    int flags = fcntl(fd, F_GETFL);
    mp_os_check_ret(flags);

    if (blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }

    int ret = fcntl(fd, F_SETFL, flags);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_set_blocking_obj, mp_os_set_blocking);

static mp_obj_t mp_os_write(mp_obj_t fd_in, mp_obj_t str_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    if (mp_obj_is_str(str_in)) {
        mp_raise_TypeError(NULL);
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(str_in, &bufinfo, MP_BUFFER_READ);
    int ret = mp_os_write_str(fd, bufinfo.buf, bufinfo.len);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_write_obj, mp_os_write);


// Files and Directories
// ---------------------
static mp_obj_t mp_os_chdir(mp_obj_t path_in) {
    path_in = mp_os_fspath(path_in);
    const char *path = mp_os_get_fspath(path_in);
    int ret = chdir(path);
    mp_os_check_ret_filename(ret, path_in);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_chdir_obj, mp_os_chdir);

static mp_obj_t mp_os_fsencode(mp_obj_t filename) {
    if (mp_obj_get_type(filename) == &mp_type_bytes) {
        return filename;
    }
    mp_obj_t args[2];
    mp_load_method(filename, MP_QSTR_encode, args);
    return mp_call_method_n_kw(0, 0, args);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_fsencode_obj, mp_os_fsencode);

static mp_obj_t mp_os_fsdecode(mp_obj_t filename) {
    if (mp_obj_get_type(filename) == &mp_type_str) {
        return filename;
    }
    mp_obj_t args[2];
    mp_load_method(filename, MP_QSTR_decode, args);
    return mp_call_method_n_kw(0, 0, args);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_fsdecode_obj, mp_os_fsdecode);

static mp_obj_t mp_os_fspath(mp_obj_t path_in) {
    if (mp_obj_is_str_or_bytes(path_in)) {
        size_t len;
        const char *path = mp_obj_str_get_data(path_in, &len);
        if (strlen(path) != len) {
            mp_raise_ValueError(NULL);
        }
        return path_in;
    }
    // mp_obj_t dest[2];
    // mp_load_method_maybe(path, MP_QSTR___fspath__, dest);
    // if (dest[1] == MP_OBJ_NULL) {
    //     mp_raise_TypeError(NULL);
    // }
    // return mp_call_method_n_kw(0, 0, dest);
    mp_raise_TypeError(NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_fspath_obj, mp_os_fspath);

static mp_obj_t mp_os_getcwd(void) {
    vstr_t buf;
    vstr_init(&buf, 256);
    const char *cwd = getcwd(vstr_str(&buf), 256);
    vstr_add_len(&buf, strnlen(cwd, 256));
    return mp_obj_new_str_from_vstr(&buf);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_getcwd_obj, mp_os_getcwd);

static mp_obj_t mp_os_scandir(size_t n_args, const mp_obj_t *args);

static mp_obj_t mp_os_listdir(size_t n_args, const mp_obj_t *args) {
    mp_obj_t iter = mp_os_scandir(n_args, args);
    mp_obj_t list = mp_obj_new_list(0, NULL);
    mp_obj_t next = mp_iternext(iter);
    while (next != MP_OBJ_STOP_ITERATION) {
        mp_obj_t name = mp_load_attr(next, MP_QSTR_name);
        mp_obj_list_append(list, name);
        next = mp_iternext(iter);
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_listdir_obj, 0, 1, mp_os_listdir);

static mp_obj_t mp_os_mkdir(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    const qstr kws[] = { MP_QSTR_name, MP_QSTR_mode, 0 };
    const char *path;
    mp_int_t mode = 0777;
    parse_args_and_kw_map(n_args, args, kwargs, "s|i", kws, &path, &mode);

    int ret = mkdir(path, mode);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_os_mkdir_obj, 0, mp_os_mkdir);

static mp_obj_t mp_os_path_isdir(mp_obj_t path_in);
static void mp_os_path_normpath_internal(mp_obj_t path_in, bool absolute, vstr_t *vstr);

static mp_obj_t mp_os_makedirs(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    const qstr kws[] = { MP_QSTR_name, MP_QSTR_mode, MP_QSTR_exist_ok, 0 };
    mp_obj_t mkdir_args[] = { MP_OBJ_NULL, MP_OBJ_NEW_SMALL_INT(0777) };
    mp_int_t exist_ok = 0;
    parse_args_and_kw_map(n_args, args, kwargs, "O|Op", kws, &mkdir_args[0], &mkdir_args[1], &exist_ok);

    vstr_t vstr;
    mp_os_path_normpath_internal(mkdir_args[0], false, &vstr);
    char *slash = vstr_null_terminated_str(&vstr);
    const char *begin = slash;
    for (;;) {
        slash = strchr(slash + 1, '/');
        if (!slash) {
            break;
        }
        *slash = '\0';
        mkdir(begin, 0777);
        *slash = '/';
    }

    nlr_buf_t buf;
    if (nlr_push(&buf) == 0) {
        mp_os_mkdir(2, mkdir_args, NULL);
        nlr_pop();
    } else if (!exist_ok || !mp_obj_exception_match(buf.ret_val, &mp_type_FileExistsError) || !mp_obj_is_true(mp_os_path_isdir(mkdir_args[0]))) {
        nlr_raise(buf.ret_val);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_os_makedirs_obj, 0, mp_os_makedirs);

static mp_obj_t mp_os_rename(mp_obj_t src_in, mp_obj_t dst_in) {
    src_in = mp_os_fspath(src_in);
    dst_in = mp_os_fspath(dst_in);
    const char *src = mp_os_get_fspath(src_in);
    const char *dst = mp_os_get_fspath(dst_in);
    int ret = rename(src, dst);
    mp_os_check_ret_filename(ret, src_in);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_rename_obj, mp_os_rename);

static mp_obj_t mp_os_rmdir(mp_obj_t path_in) {
    path_in = mp_os_fspath(path_in);
    const char *path = mp_os_get_fspath(path_in);
    int ret = rmdir(path);
    mp_os_check_ret_filename(ret, path_in);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_rmdir_obj, mp_os_rmdir);

static mp_obj_t mp_os_removedirs(mp_obj_t name) {
    mp_obj_t ret = mp_os_rmdir(name);

    vstr_t vstr;
    mp_os_path_normpath_internal(name, true, &vstr);
    for (;;) {
        char *slash = strrchr(vstr_null_terminated_str(&vstr), '/');
        if (slash == vstr.buf) {
            break;
        }
        vstr_cut_tail_bytes(&vstr, vstr.buf + vstr.len - slash);
        if (rmdir(vstr_null_terminated_str(&vstr)) < 0) {
            break;
        }
    }
    return ret;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_removedirs_obj, mp_os_removedirs);

typedef struct {
    mp_obj_base_t base;
    mp_obj_t name;
    mp_obj_t path;
    struct dirent entry;
} mp_obj_os_direntry_t;

static const mp_obj_type_t mp_type_os_direntry;

static mp_obj_t mp_os_path_join(size_t n_args, const mp_obj_t *args);

static mp_obj_t mp_os_direntry_new(mp_obj_t dir, const struct dirent *dp) {
    mp_obj_os_direntry_t *self = mp_obj_malloc(mp_obj_os_direntry_t, &mp_type_os_direntry);
    const mp_obj_type_t *type = mp_obj_get_type(dir);
    self->name = mp_obj_new_str_copy(type, (byte *)dp->d_name, strlen(dp->d_name));
    mp_obj_t join_args[2] = { dir, self->name };
    self->path = mp_os_path_join(2, join_args);
    self->entry = *dp;
    return MP_OBJ_FROM_PTR(self);
}

static void mp_os_direntry_attr(mp_obj_t self_in, qstr attr, mp_obj_t dest[]) {
    mp_obj_os_direntry_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] != MP_OBJ_NULL) {
        return;
    }
    switch (attr) {
        case MP_QSTR_name:
            dest[0] = self->name;
            break;
        case MP_QSTR_path:
            dest[0] = self->path;
            break;
        default:
            dest[1] = MP_OBJ_SENTINEL;
            break;
    }
}

static void mp_os_direntry_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_obj_os_direntry_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<%q '%s'>", (qstr)self->base.type->name, mp_obj_str_get_str(self->name));
}

// static mp_obj_t mp_os_direntry_fspath(mp_obj_t self_in) {
//     mp_obj_os_direntry_t *self = MP_OBJ_TO_PTR(self_in);
//     return self->path;
// }
// static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_direntry_fspath_obj, mp_os_direntry_fspath);

static mp_obj_t mp_os_direntry_inode(mp_obj_t self_in) {
    mp_obj_os_direntry_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_bool(self->entry.d_ino);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_direntry_inode_obj, mp_os_direntry_inode);

static mp_obj_t mp_os_direntry_isdir(mp_obj_t self_in) {
    mp_obj_os_direntry_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_bool(S_ISDIR(self->entry.d_type << 12));

}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_direntry_isdir_obj, mp_os_direntry_isdir);

static mp_obj_t mp_os_direntry_isfile(mp_obj_t self_in) {
    mp_obj_os_direntry_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_bool(S_ISREG(self->entry.d_type << 12));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_direntry_isfile_obj, mp_os_direntry_isfile);

static mp_obj_t mp_os_stat(mp_obj_t path_in);

static mp_obj_t mp_os_direntry_stat(mp_obj_t self_in) {
    mp_obj_os_direntry_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_os_stat(self->path);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_direntry_stat_obj, mp_os_direntry_stat);

static const mp_rom_map_elem_t mp_os_direntry_locals_dict_table[] = {
    // { MP_ROM_QSTR(MP_QSTR___fspath__),      MP_ROM_PTR(&mp_os_direntry_fspath_obj) },
    { MP_ROM_QSTR(MP_QSTR_inode),           MP_ROM_PTR(&mp_os_direntry_inode_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_dir),          MP_ROM_PTR(&mp_os_direntry_isdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_file),         MP_ROM_PTR(&mp_os_direntry_isfile_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat),            MP_ROM_PTR(&mp_os_direntry_stat_obj) },
};
static MP_DEFINE_CONST_DICT(mp_os_direntry_locals_dict, mp_os_direntry_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_os_direntry,
    MP_QSTR_DirEntry,
    MP_TYPE_FLAG_NONE,
    attr, mp_os_direntry_attr,
    print, mp_os_direntry_print,
    locals_dict, &mp_os_direntry_locals_dict
    );

typedef struct {
    mp_obj_base_t base;
    mp_obj_t path;
    DIR *dirp;
} mp_obj_os_scandir_iter_t;

static mp_obj_t mp_os_scandir_iter_del(mp_obj_t self_in) {
    mp_obj_os_scandir_iter_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->dirp) {
        closedir(self->dirp);
        self->dirp = NULL;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_scandir_iter_del_obj, mp_os_scandir_iter_del);

static mp_obj_t mp_os_scandir_iter_next(mp_obj_t self_in) {
    mp_obj_os_scandir_iter_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->dirp) {
        return MP_OBJ_STOP_ITERATION;
    }

    errno = 0;
    struct dirent *dp = readdir(self->dirp);
    if (dp) {
        return mp_os_direntry_new(self->path, dp);
    }
    int orig_errno = errno;
    mp_os_scandir_iter_del(self_in);
    if (orig_errno != 0) {
        mp_raise_OSError(orig_errno);
    } else {
        return MP_OBJ_STOP_ITERATION;
    }
}

static const mp_rom_map_elem_t mp_os_scandir_iter_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&mp_os_scandir_iter_del_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),       MP_ROM_PTR(&mp_enter_self_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),        MP_ROM_PTR(&mp_exit_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_os_scandir_iter_del_obj) },
};
static MP_DEFINE_CONST_DICT(mp_os_scandir_iter_locals_dict, mp_os_scandir_iter_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_os_scandir_iter,
    MP_QSTR_iterator,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    iter, mp_os_scandir_iter_next,
    locals_dict, &mp_os_scandir_iter_locals_dict
    );

static mp_obj_t mp_os_scandir(size_t n_args, const mp_obj_t *args) {
    mp_obj_t path_in = (n_args > 0) ? args[0] : MP_OBJ_NEW_QSTR(MP_QSTR__dot_);
    path_in = mp_os_fspath(path_in);
    const char *path = mp_os_get_fspath(path_in);

    mp_obj_os_scandir_iter_t *iter = mp_obj_malloc_with_finaliser(mp_obj_os_scandir_iter_t, &mp_type_os_scandir_iter);
    iter->path = path_in;
    iter->dirp = opendir(path);
    if (!iter->dirp) {
        mp_raise_OSError_with_filename(errno, path_in);
    }
    return MP_OBJ_FROM_PTR(iter);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_scandir_obj, 0, 1, mp_os_scandir);

typedef struct {
    mp_obj_tuple_t base;
    mp_obj_t items[10];
    struct stat stat;
} mp_obj_os_stat_result_t;

static const mp_obj_type_t mp_type_os_stat_result;

static mp_obj_t mp_os_stat_result(const struct stat *sb) {
    mp_obj_os_stat_result_t *self = mp_obj_malloc(mp_obj_os_stat_result_t, &mp_type_os_stat_result);
    self->base.len = 10;
    self->items[0] = mp_obj_new_int_from_uint(sb->st_mode),
    self->items[1] = mp_obj_new_int_from_uint(sb->st_ino),
    self->items[2] = mp_obj_new_int_from_uint(sb->st_dev),
    self->items[3] = mp_obj_new_int_from_uint(sb->st_nlink),
    self->items[4] = mp_obj_new_int_from_uint(sb->st_uid),
    self->items[5] = mp_obj_new_int_from_uint(sb->st_gid),
    self->items[6] = mp_obj_new_int_from_uint(sb->st_size),
    self->items[7] = mp_obj_new_int_from_ll(sb->st_atime),
    self->items[8] = mp_obj_new_int_from_ll(sb->st_mtime),
    self->items[9] = mp_obj_new_int_from_ll(sb->st_ctime),
    self->stat = *sb;
    return MP_OBJ_FROM_PTR(self);
}

void mp_obj_to_time(mp_obj_t obj, time_t *t) {
    if (mp_obj_is_small_int(obj)) {
        *t = MP_OBJ_SMALL_INT_VALUE(obj);
    } else if (mp_obj_is_exact_type(obj, &mp_type_int)) {
        if (!mp_obj_int_to_bytes_impl(obj, false, sizeof(*t), (byte *)t)) {
            mp_raise_type(&mp_type_OverflowError);
        }
    } else if (mp_obj_is_exact_type(obj, &mp_type_float)) {
        *t = mp_obj_get_float(obj);
    } else {
        mp_raise_TypeError(NULL);
    }
}

void mp_obj_to_timespec(mp_obj_t obj, struct timespec *tp) {
    mp_float_t val = mp_obj_get_float(obj);
    mp_float_t ipart;
    tp->tv_nsec = MICROPY_FLOAT_CONST(1e9) * MICROPY_FLOAT_C_FUN(modf)(val, &ipart);
    tp->tv_sec = ipart;
}

void mp_ns_to_timespec(mp_obj_t obj, struct timespec *tp) {
    time_t t;
    mp_obj_to_time(obj, &t);
    lldiv_t divmod = lldiv(t, 1000000000);
    tp->tv_sec = divmod.quot;
    tp->tv_nsec = divmod.rem;
}

static void mp_os_stat_result_attr(mp_obj_t self_in, qstr attr, mp_obj_t dest[]) {
    mp_obj_os_stat_result_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] != MP_OBJ_NULL) {
        return;
    }
    switch (attr) {
        case MP_QSTR_st_mode:
            dest[0] = self->items[0];
            break;
        case MP_QSTR_st_ino:
            dest[0] = self->items[1];
            break;
        case MP_QSTR_st_dev:
            dest[0] = self->items[2];
            break;
        case MP_QSTR_st_nlink:
            dest[0] = self->items[3];
            break;
        case MP_QSTR_st_uid:
            dest[0] = self->items[4];
            break;
        case MP_QSTR_st_gid:
            dest[0] = self->items[5];
            break;
        case MP_QSTR_st_size:
            dest[0] = self->items[6];
            break;
        case MP_QSTR_st_atime:
            dest[0] = mp_timespec_to_obj(&self->stat.st_atim);
            break;
        case MP_QSTR_st_mtime:
            dest[0] = mp_timespec_to_obj(&self->stat.st_mtim);
            break;
        case MP_QSTR_st_ctime:
            dest[0] = mp_timespec_to_obj(&self->stat.st_ctim);
            break;
        case MP_QSTR_st_atime_ns:
            dest[0] = mp_timespec_to_ns(&self->stat.st_atim);
            break;
        case MP_QSTR_st_mtime_ns:
            dest[0] = mp_timespec_to_ns(&self->stat.st_mtim);
            break;
        case MP_QSTR_st_ctime_ns:
            dest[0] = mp_timespec_to_ns(&self->stat.st_ctim);
            break;
        case MP_QSTR_st_blocks:
            dest[0] = mp_obj_new_int_from_uint(self->stat.st_blocks);
            break;
        case MP_QSTR_st_blksize:
            dest[0] = mp_obj_new_int_from_uint(self->stat.st_blksize);
            break;
        case MP_QSTR_st_rdev:
            dest[0] = mp_obj_new_int_from_uint(self->stat.st_rdev);
            break;
    }
}

static void mp_os_stat_result_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    static const qstr mp_os_stat_result_attrs[] = {
        MP_QSTR_st_mode,
        MP_QSTR_st_ino,
        MP_QSTR_st_dev,
        MP_QSTR_st_nlink,
        MP_QSTR_st_uid,
        MP_QSTR_st_gid,
        MP_QSTR_st_size,
        MP_QSTR_st_atime,
        MP_QSTR_st_mtime,
        MP_QSTR_st_ctime,
    };
    mp_obj_os_stat_result_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_attrtuple_print_helper(print, mp_os_stat_result_attrs, &self->base);
}

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_os_stat_result,
    MP_QSTR_stat_result,
    MP_TYPE_FLAG_ITER_IS_GETITER,
    print, mp_os_stat_result_print,
    unary_op, mp_obj_tuple_unary_op,
    binary_op, mp_obj_tuple_binary_op,
    attr, mp_os_stat_result_attr,
    subscr, mp_obj_tuple_subscr,
    iter, mp_obj_tuple_getiter,
    parent, &mp_type_tuple
    );

static mp_obj_t mp_os_stat(mp_obj_t path_in) {
    int ret;
    struct stat sb;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        ret = fstat(fd, &sb);
    } else {
        path_in = mp_os_fspath(path_in);
        const char *path = mp_os_get_fspath(path_in);
        ret = stat(path, &sb);
    }
    mp_os_check_ret_filename(ret, path_in);
    return mp_os_stat_result(&sb);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_stat_obj, mp_os_stat);

static mp_obj_t mp_os_statvfs_result(const struct statvfs *sb) {
    static const qstr mp_os_statvfs_attrs[] = {
        MP_QSTR_f_bsize,
        MP_QSTR_f_frsize,
        MP_QSTR_f_blocks,
        MP_QSTR_f_bfree,
        MP_QSTR_f_bavail,
        MP_QSTR_f_files,
        MP_QSTR_f_ffree,
        MP_QSTR_f_favail,
        MP_QSTR_f_flag,
        MP_QSTR_f_namemax,
    };
    mp_obj_t items[] = {
        mp_obj_new_int_from_uint(sb->f_bsize),
        mp_obj_new_int_from_uint(sb->f_frsize),
        mp_obj_new_int_from_uint(sb->f_blocks),
        mp_obj_new_int_from_uint(sb->f_bfree),
        mp_obj_new_int_from_uint(sb->f_bavail),
        mp_obj_new_int_from_uint(sb->f_files),
        mp_obj_new_int_from_uint(sb->f_ffree),
        mp_obj_new_int_from_uint(sb->f_favail),
        mp_obj_new_int_from_uint(sb->f_flag),
        mp_obj_new_int_from_uint(sb->f_namemax),
    };
    return mp_obj_new_attrtuple(mp_os_statvfs_attrs, 10, items);
}

static mp_obj_t mp_os_statvfs(mp_obj_t path_in) {
    int ret;
    struct statvfs sb;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        ret = fstatvfs(fd, &sb);
    } else {
        path_in = mp_os_fspath(path_in);
        const char *path = mp_os_get_fspath(path_in);
        ret = statvfs(path, &sb);
    }
    mp_os_check_ret_filename(ret, path_in);
    return mp_os_statvfs_result(&sb);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_statvfs_obj, mp_os_statvfs);

static mp_obj_t mp_os_sync(void) {
    MP_THREAD_GIL_EXIT();
    sync();
    MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_sync_obj, mp_os_sync);

static mp_obj_t mp_os_truncate(mp_obj_t path_in, mp_obj_t length_in) {
    int ret;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        mp_int_t length = mp_obj_get_int(length_in);
        ret = ftruncate(fd, length);
    } else {
        path_in = mp_os_fspath(path_in);
        const char *path = mp_os_get_fspath(path_in);
        mp_int_t length = mp_obj_get_int(length_in);
        ret = truncate(path, length);
    }
    mp_os_check_ret_filename(ret, path_in);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_truncate_obj, mp_os_truncate);

static mp_obj_t mp_os_unlink(mp_obj_t path_in) {
    path_in = mp_os_fspath(path_in);
    const char *path = mp_os_get_fspath(path_in);
    int ret = unlink(path);
    mp_os_check_ret_filename(ret, path_in);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_unlink_obj, mp_os_unlink);

static mp_obj_t mp_os_utime(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    const qstr kws[] = { MP_QSTR_path, MP_QSTR_times, MP_QSTR_ns, 0  };
    mp_obj_t path;
    mp_obj_t times = mp_const_none;
    mp_obj_t ns = mp_const_none;
    parse_args_and_kw_map(n_args, args, kwargs, "O&|O$O", kws, mp_os_fspath, &path, &times, &ns);

    struct timespec ts[2] = {
        { .tv_sec = 0, .tv_nsec = UTIME_NOW },
        { .tv_sec = 0, .tv_nsec = UTIME_NOW },
    };
    if (times != mp_const_none) {
        if (ns != mp_const_none) {
            mp_raise_ValueError(NULL);
        }
        mp_obj_t atime, mtime;
        parse_args(1, &times, "(OO)", &atime, &mtime);
        mp_obj_to_timespec(atime, &ts[0]);
        mp_obj_to_timespec(mtime, &ts[1]);

    }
    if (ns != mp_const_none) {
        mp_obj_t atime_ns, mtime_ns;
        parse_args(1, &ns, "(OO)", &atime_ns, &mtime_ns);
        mp_ns_to_timespec(atime_ns, &ts[0]);
        mp_ns_to_timespec(mtime_ns, &ts[1]);
    }

    int ret;
    if (mp_obj_is_int(path)) {
        mp_int_t fd = mp_obj_get_int(path);
        ret = futimens(fd, ts);
    } else {
        const char *path_str = mp_os_get_fspath(path);
        ret = utimensat(AT_FDCWD, path_str, ts, 0);
    }
    mp_os_check_ret_filename(ret, path);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_os_utime_obj, 0, mp_os_utime);

static const mp_rom_obj_tuple_t mp_os_supports_fd_obj = {
    .base = { .type = &mp_type_tuple },
    .len = 2,
    .items = {
        MP_ROM_PTR(&mp_os_stat_obj),
        MP_ROM_PTR(&mp_os_truncate_obj),
        // MP_ROM_PTR(&mp_os_utime_obj),
    },
};

static mp_obj_t mp_os_eventfd(size_t n_args, const mp_obj_t *args) {
    uint32_t initval = mp_obj_get_int(args[0]);
    int flags = (n_args > 1) ? mp_obj_get_int(args[1]) : 0;
    int ret = eventfd(initval, flags);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_eventfd_obj, 1, 2, mp_os_eventfd);


// Process Management
// ------------------
static mp_obj_t mp_os_abort(void) {
    abort();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_abort_obj, mp_os_abort);

static mp_obj_t mp_os__exit(mp_obj_t n_in) {
    mp_int_t n = mp_obj_get_int(n_in);
    _exit(n);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os__exit_obj, mp_os__exit);

static mp_obj_t mp_os_kill(mp_obj_t pid_in, mp_obj_t sig_in) {
    mp_int_t pid = mp_obj_get_int(pid_in);
    mp_int_t sig = mp_obj_get_int(sig_in);
    MP_THREAD_GIL_EXIT();
    int ret = kill(pid, sig);
    MP_THREAD_GIL_ENTER();
    mp_os_check_ret(ret);
    mp_handle_pending(true);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_kill_obj, mp_os_kill);

static mp_obj_t mp_os_times_result(clock_t elapsed, const struct tms *buf) {
    static const qstr mp_os_times_attrs[] = {
        MP_QSTR_user,
        MP_QSTR_system,
        MP_QSTR_children_user,
        MP_QSTR_children_system,
        MP_QSTR_elapsed,
    };
    mp_obj_t items[] = {
        mp_obj_new_float(((mp_float_t)buf->tms_utime) / CLOCKS_PER_SEC),
        mp_obj_new_float(((mp_float_t)buf->tms_stime) / CLOCKS_PER_SEC),
        mp_obj_new_float(((mp_float_t)buf->tms_cutime) / CLOCKS_PER_SEC),
        mp_obj_new_float(((mp_float_t)buf->tms_cstime) / CLOCKS_PER_SEC),
        mp_obj_new_float(((mp_float_t)elapsed) / CLOCKS_PER_SEC),
    };
    return mp_obj_new_attrtuple(mp_os_times_attrs, 5, items);
}

static mp_obj_t mp_os_times(void) {
    struct tms buf;
    clock_t elapsed = times(&buf);
    if (elapsed == -1) {
        mp_raise_OSError(errno);
    }
    return mp_os_times_result(elapsed, &buf);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_times_obj, mp_os_times);

// Interface to the scheduler
// --------------------------
#if MICROPY_PY_OS_SCHED
static mp_obj_t mp_os_sched_getaffinity(mp_obj_t pid) {
    size_t len = 0;
    mp_obj_t items[configNUMBER_OF_CORES];
    UBaseType_t uxCoreAffinityMask = vTaskCoreAffinityGet(NULL);
    for (size_t i = 0; i < configNUMBER_OF_CORES; i++) {
        if (uxCoreAffinityMask & (1u << i)) {
            items[len++] = MP_OBJ_NEW_SMALL_INT(i);
        }
    }
    return mp_obj_new_tuple(len, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_sched_getaffinity_obj, mp_os_sched_getaffinity);

static mp_obj_t mp_os_sched_setaffinity(mp_obj_t pid, mp_obj_t mask) {
    size_t len;
    mp_obj_t *items;
    mp_obj_get_array(mask, &len, &items);

    UBaseType_t uxCoreAffinityMask = 0;
    for (size_t i = 0; i < len; i++) {
        mp_int_t core = mp_obj_get_int(items[i]);
        if ((core < 0) || (core >= configNUMBER_OF_CORES)) {
            mp_raise_OSError(EINVAL);
        }
        uxCoreAffinityMask |= 1u << core;
    }

    vTaskCoreAffinitySet(NULL, uxCoreAffinityMask);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_sched_setaffinity_obj, mp_os_sched_setaffinity);
#endif

// Miscellaneous System Information
// --------------------------------
static mp_obj_t mp_os_cpu_count(void) {
    return MP_OBJ_NEW_SMALL_INT(configNUMBER_OF_CORES);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_cpu_count_obj, mp_os_cpu_count);

// Random numbers
// --------------
static mp_obj_t mp_os_urandom(mp_obj_t size_in) {
    mp_int_t size = mp_obj_get_int(size_in);
    vstr_t buf;
    vstr_init(&buf, size);
    MP_THREAD_GIL_EXIT();
    ssize_t ret = getrandom(vstr_str(&buf), size, 0);
    MP_THREAD_GIL_ENTER();
    mp_os_check_ret(ret);
    vstr_add_len(&buf, ret);
    return mp_obj_new_bytes_from_vstr(&buf);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_urandom_obj, mp_os_urandom);


// MicroPython extensions
// ----------------------
static mp_obj_t mp_os_dlerror(void) {
    void *error = dlerror();
    if (!error) {
        return mp_const_none;
    }
    size_t error_len = strlen(error);
    mp_obj_t args[] = {
        MP_OBJ_NEW_SMALL_INT(errno),
        mp_obj_new_str_copy(&mp_type_str, error, error_len),
    };
    nlr_raise(mp_obj_exception_make_new(&mp_type_OSError, error_len ? 2 : 1, 0, args));
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_dlerror_obj, mp_os_dlerror);

static mp_obj_t mp_os_dlflash(mp_obj_t file_in) {
    const char *file = mp_os_get_fspath(file_in);
    void *result = dl_flash(file);
    if (!result) {
        mp_raise_OSError(errno);
    }
    return mp_obj_new_int((intptr_t)result);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_dlflash_obj, mp_os_dlflash);

static mp_obj_t mp_os_dlopen(mp_obj_t file_in) {
    const char *file = mp_os_get_fspath(file_in);
    void *result = dlopen(file, 0);
    if (!result) {
        mp_raise_OSError(errno);
    }
    return mp_obj_new_int((intptr_t)result);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_dlopen_obj, mp_os_dlopen);

static mp_obj_t mp_os_dlsym(mp_obj_t handle_in, mp_obj_t symbol_in) {
    void *handle = (void *)mp_obj_get_int(handle_in);
    const char *symbol = mp_obj_str_get_str(symbol_in);

    if (handle) {
        // Validate that handle is a valid pointer
        const flash_heap_header_t *header = NULL;
        while (dl_iterate(&header) && (header != handle)) {
            ;
        }
        if (header != handle) {
            mp_raise_ValueError(NULL);
        }
    }

    void *value = dlsym(handle, symbol);
    if (!value) {
        mp_raise_type(&mp_type_KeyError);
    }
    return mp_obj_new_int((intptr_t)value);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_dlsym_obj, mp_os_dlsym);

static mp_obj_t mp_os_dllist(void) {
    mp_obj_t list = mp_obj_new_list(0, NULL);
    const flash_heap_header_t *header = NULL;
    while (dl_iterate(&header)) {
        Elf32_Addr strtab = 0;
        Elf32_Word soname = 0;
        for (const Elf32_Dyn *dyn = header->entry; dyn->d_tag != DT_NULL; dyn++) {
            switch (dyn->d_tag) {
                case DT_STRTAB:
                    strtab = dyn->d_un.d_ptr;
                    break;
                case DT_SONAME:
                    soname = dyn->d_un.d_val;
                    break;
            }
        }
        if (strtab && soname) {
            void *addr = (void *)(strtab + soname);
            mp_obj_t item = mp_obj_new_str_copy(&mp_type_str, addr, strlen(addr));
            mp_obj_list_append(list, item);
        }
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_dllist_obj, mp_os_dllist);

#if defined(__ARM_ARCH) && (__ARM_ARCH >= 8) && defined(__thumb__)
__attribute__((naked))
static int call_func_arm(void *func, int nargs, void **args) {
    __asm__ volatile (
        "push {r4-r7, lr}\n"
        "mov r4, r0\n"        // Save func pointer
        "mov r5, r1\n"        // Save nargs
        "mov r6, r2\n"        // Save args array

        // Handle stack arguments (args 4+)
        "cmp r5, #4\n"
        "ble load_regs\n"     // If nargs <= 4, skip stack setup

        // Calculate number of stack args
        "sub r7, r5, #4\n"    // r7 = nargs - 4

        // Ensure 8-byte stack alignment
        "mov r0, r7\n"
        "and r0, r0, #1\n"    // Check if odd number of stack args
        "cmp r0, #1\n"
        "bne push_args\n"
        "push {r0}\n"         // Push dummy word for 8-byte alignment

        "push_args:\n"
        // Push arguments in reverse order (right to left)
        // Start from args[nargs-1] down to args[4]
        "mov r0, r5\n"
        "sub r0, r0, #1\n"    // r0 = nargs - 1 (index of last arg)

        "push_loop:\n"
        "cmp r0, #3\n"        // While index > 3
        "ble load_regs\n"

        "lsl r1, r0, #2\n"    // r1 = index * 4 (byte offset)
        "ldr r2, [r6, r1]\n"  // r2 = args[index]
        "push {r2}\n"         // Push to stack

        "sub r0, r0, #1\n"    // index--
        "b push_loop\n"

        "load_regs:\n"
        // Load up to 4 args into r0-r3
        "cmp r5, #0\n"
        "beq call_it\n"
        "ldr r0, [r6, #0]\n"

        "cmp r5, #1\n"
        "beq call_it\n"
        "ldr r1, [r6, #4]\n"

        "cmp r5, #2\n"
        "beq call_it\n"
        "ldr r2, [r6, #8]\n"

        "cmp r5, #3\n"
        "beq call_it\n"
        "ldr r3, [r6, #12]\n"

        "call_it:\n"
        "blx r4\n"            // Call function, result in r0

        // Clean up stack (remove pushed arguments)
        "cmp r5, #4\n"
        "ble done\n"

        "sub r1, r5, #4\n"    // Number of stack args
        "and r2, r1, #1\n"    // Check if we added padding
        "add r1, r1, r2\n"    // Total words to pop (including padding)
        "lsl r1, r1, #2\n"    // Convert to bytes
        "add sp, sp, r1\n"    // Restore stack pointer

        "done:\n"
        "pop {r4-r7, pc}\n"   // Return (result already in r0)
        );
}

static mp_obj_t mp_os_dlcall(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    void *cfunc = (void *)mp_obj_get_int(args[0]);
    void **cargs = m_malloc((n_args - 1) * sizeof(void *));
    for (int i = 0; i < n_args - 1; i++) {
        if (mp_obj_is_int(args[i + 1])) {
            cargs[i] = (void *)mp_obj_get_int(args[i + 1]);
        } else if (mp_obj_is_str_or_bytes(args[i + 1])) {
            cargs[i] = (void *)mp_obj_str_get_str(args[i + 1]);
        } else {
            mp_buffer_info_t bufinfo;
            mp_get_buffer_raise(args[i + 1], &bufinfo, MP_BUFFER_WRITE);
            cargs[i] = bufinfo.buf;
        }
    }
    int ret = call_func_arm(cfunc, n_args - 1, cargs);
    m_free(cargs);
    mp_map_elem_t *elem = mp_map_lookup(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_raise_oserror), MP_MAP_LOOKUP);
    if (elem && mp_obj_is_true(elem->value)) {
        return mp_os_check_ret(ret);
    } else {
        return mp_obj_new_int(ret);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_os_dlcall_obj, 1, mp_os_dlcall);
#endif

#ifndef NDEBUG
static mp_obj_t mp_os_dldebug(void) {
    puts("Module               Flash base RAM base");
    const flash_heap_header_t *header = NULL;
    while (dl_iterate(&header)) {
        Elf32_Addr strtab = 0;
        Elf32_Word soname = 0;
        for (const Elf32_Dyn *dyn = header->entry; dyn->d_tag != DT_NULL; dyn++) {
            switch (dyn->d_tag) {
                case DT_STRTAB:
                    strtab = dyn->d_un.d_ptr;
                    break;
                case DT_SONAME:
                    soname = dyn->d_un.d_val;
                    break;
            }
        }
        const char *name = (strtab && soname) ? (char *)(strtab + soname) : NULL;
        uintptr_t flash_base = (((uintptr_t)header) + sizeof(flash_heap_header_t) + 7) & ~7;
        uintptr_t ram_base = (((uintptr_t)header->ram_base) + 7) & ~7;
        printf("%-20s 0x%08x 0x%08x\n", name, flash_base, ram_base);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_dldebug_obj, mp_os_dldebug);
#endif

static mp_obj_t mp_os_mkfs(mp_obj_t source_in, mp_obj_t type_in) {
    const char *source = mp_obj_str_get_str(source_in);
    const char *type = mp_obj_str_get_str(type_in);
    int ret;
    MP_OS_CALL(ret, mkfs, source, type, NULL);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_mkfs_obj, mp_os_mkfs);

static mp_obj_t mp_os_mount(size_t n_args, const mp_obj_t *args) {
    const char *source = mp_obj_str_get_str(args[0]);
    const char *target = mp_obj_str_get_str(args[1]);
    const char *type = mp_obj_str_get_str(args[2]);
    mp_int_t flags = n_args > 3 ? mp_obj_get_int(args[3]) : 0;
    int ret;
    MP_OS_CALL(ret, mount, source, target, type, flags, NULL);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_mount_obj, 3, 4, mp_os_mount);

static mp_obj_t mp_os_umount(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret;
    MP_OS_CALL(ret, umount, path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_umount_obj, mp_os_umount);

static mp_obj_t mp_os_getattr(mp_obj_t attr) {
    switch (MP_OBJ_QSTR_VALUE(attr)) {
        case MP_QSTR_environ:
            return mp_os_get_environ();
        default:
            return MP_OBJ_NULL;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_getattr_obj, mp_os_getattr);

static const MP_DEFINE_STR_OBJ(mp_os_devnull, "/dev/null");


// path submodule
static void mp_os_path_normpath_internal(mp_obj_t path_in, bool absolute, vstr_t *vstr) {
    size_t len;
    const char *path = mp_obj_str_get_data(path_in, &len);
    const char *end = path + len;
    vstr_init(vstr, len + 1);
    if (!len) {
        return;
    }

    if (*path == '/') {
        // absolute path
        vstr_add_char(vstr, '/');
        path++;
    } else if (absolute) {
        vstr_hint_size(vstr, 256);
        char *cwd = getcwd(vstr->buf + vstr->len, vstr->alloc - vstr->len);
        vstr_add_len(vstr, strlen(cwd));
    }
    size_t min_len = vstr->len;

    while (path < end) {
        const char *next = strchr(path, '/');
        next = next ? next : end;
        size_t len = next - path;
        if ((len == 0) || (strncmp(path, ".", len) == 0)) {
            // current dir
        } else if (strncmp(path, "..", len) == 0) {
            // parent dir
            if (vstr->len == min_len) {
                if (min_len == 0) {
                    vstr_add_str(vstr, "..");
                } else if (vstr->buf[0] != '/') {
                    vstr_add_str(vstr, "/..");
                }
                min_len = vstr->len;
            } else {
                while ((vstr->len > min_len) && (vstr->buf[--vstr->len] != '/')) {
                    ;
                }
            }
        } else {
            if ((vstr->len != min_len) || (min_len > 1)) {
                vstr_add_char(vstr, '/');
            }
            vstr_add_strn(vstr, path, len);
        }
        path = next + 1;
    }
    if (vstr->len == 0) {
        vstr_add_char(vstr, '.');
    }
}

static mp_obj_t mp_os_path_abspath(mp_obj_t path_in) {
    vstr_t vstr;
    mp_os_path_normpath_internal(path_in, true, &vstr);
    const mp_obj_type_t *type = mp_obj_get_type(path_in);
    return mp_obj_new_str_type_from_vstr(type, &vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_abspath_obj, mp_os_path_abspath);

static mp_obj_t mp_os_path_basename(mp_obj_t path) {
    size_t len;
    const char *data = mp_obj_str_get_data(path, &len);
    vstr_t vstr;
    vstr_init(&vstr, len);
    vstr_add_strn(&vstr, data, len);
    char *dir = basename(vstr_str(&vstr));
    const mp_obj_type_t *type = mp_obj_get_type(path);
    return mp_obj_new_str_of_type(type, (byte *)dir, strlen(dir));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_basename_obj, mp_os_path_basename);

static mp_obj_t mp_os_path_dirname(mp_obj_t path) {
    size_t len;
    const char *data = mp_obj_str_get_data(path, &len);
    vstr_t vstr;
    vstr_init(&vstr, len);
    vstr_add_strn(&vstr, data, len);
    char *dir = dirname(vstr_str(&vstr));
    const mp_obj_type_t *type = mp_obj_get_type(path);
    return mp_obj_new_str_of_type(type, (byte *)dir, strlen(dir));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_dirname_obj, mp_os_path_dirname);

static mp_obj_t mp_os_path_exists(mp_obj_t path_in) {
    const char *path = mp_os_get_fspath(path_in);
    struct stat buf;
    int ret = stat(path, &buf);
    return mp_obj_new_bool(ret >= 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_exists_obj, mp_os_path_exists);

static mp_obj_t mp_os_path_isdir(mp_obj_t path_in) {
    const char *path = mp_os_get_fspath(path_in);
    struct stat buf;
    int ret = stat(path, &buf);
    return mp_obj_new_bool((ret >= 0) && S_ISDIR(buf.st_mode));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_isdir_obj, mp_os_path_isdir);

static mp_obj_t mp_os_path_isfile(mp_obj_t path_in) {
    const char *path = mp_os_get_fspath(path_in);
    struct stat buf;
    int ret = stat(path, &buf);
    return mp_obj_new_bool((ret >= 0) && S_ISREG(buf.st_mode));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_isfile_obj, mp_os_path_isfile);

static mp_obj_t mp_os_path_join(size_t n_args, const mp_obj_t *args) {
    const mp_obj_type_t *type = mp_obj_get_type(args[0]);

    vstr_t vstr;
    vstr_init(&vstr, 0);
    for (size_t i = 0; i < n_args; i++) {
        size_t len;
        const char *str = mp_obj_str_get_data(args[i], &len);
        if (len == 0) {
            continue;
        }
        if (str[0] == '/') {
            vstr.len = 0;
        }
        if ((vstr.len > 0) && (vstr.buf[vstr.len - 1] != '/')) {
            vstr_add_byte(&vstr, '/');
        }
        vstr_add_strn(&vstr, str, len);
    }
    return mp_obj_new_str_type_from_vstr(type, &vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR(mp_os_path_join_obj, 1, mp_os_path_join);

static mp_obj_t mp_os_path_normpath(mp_obj_t path) {
    vstr_t vstr;
    mp_os_path_normpath_internal(path, false, &vstr);
    const mp_obj_type_t *type = mp_obj_get_type(path);
    return mp_obj_new_str_type_from_vstr(type, &vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_normpath_obj, mp_os_path_normpath);

static mp_obj_t mp_os_path_split(mp_obj_t path) {
    mp_obj_t items[2];
    items[0] = mp_os_path_dirname(path);
    items[1] = mp_os_path_basename(path);
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_split_obj, mp_os_path_split);

static mp_obj_t mp_os_path_splitdrive(mp_obj_t path) {
    const mp_obj_type_t *type = mp_obj_get_type(path);
    mp_obj_t items[2];
    if (type == &mp_type_str) {
        items[0] = MP_OBJ_NEW_QSTR(MP_QSTR_);
    } else if (type == &mp_type_bytes) {
        items[0] = mp_const_empty_bytes;
    } else {
        mp_raise_TypeError(NULL);
    }
    items[1] = path;
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_splitdrive_obj, mp_os_path_splitdrive);

static mp_obj_t mp_os_path_splitext(mp_obj_t path) {
    size_t len;
    const char *data = mp_obj_str_get_data(path, &len);
    const char *dot = data + len;
    while (--dot > data) {
        if (*dot == '.') {
            break;
        }
    }
    if (dot == data) {
        dot = data + len;
    }
    mp_obj_t items[2];
    const mp_obj_type_t *type = mp_obj_get_type(path);
    items[0] = mp_obj_new_str_of_type(type, (byte *)data, dot - data);
    items[1] = mp_obj_new_str_of_type(type, (byte *)dot, data + len - dot);
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_path_splitext_obj, mp_os_path_splitext);

static const mp_rom_map_elem_t os_path_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_os_path) },
    { MP_ROM_QSTR(MP_QSTR_abspath),     MP_ROM_PTR(&mp_os_path_abspath_obj) },
    { MP_ROM_QSTR(MP_QSTR_basename),    MP_ROM_PTR(&mp_os_path_basename_obj) },
    { MP_ROM_QSTR(MP_QSTR_dirname),     MP_ROM_PTR(&mp_os_path_dirname_obj) },
    { MP_ROM_QSTR(MP_QSTR_exists),      MP_ROM_PTR(&mp_os_path_exists_obj) },
    { MP_ROM_QSTR(MP_QSTR_lexists),     MP_ROM_PTR(&mp_os_path_exists_obj) },
    { MP_ROM_QSTR(MP_QSTR_isdir),       MP_ROM_PTR(&mp_os_path_isdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_isfile),      MP_ROM_PTR(&mp_os_path_isfile_obj) },
    { MP_ROM_QSTR(MP_QSTR_join),        MP_ROM_PTR(&mp_os_path_join_obj) },
    { MP_ROM_QSTR(MP_QSTR_normpath),    MP_ROM_PTR(&mp_os_path_normpath_obj) },
    { MP_ROM_QSTR(MP_QSTR_realpath),    MP_ROM_PTR(&mp_os_path_abspath_obj) },
    { MP_ROM_QSTR(MP_QSTR_split),       MP_ROM_PTR(&mp_os_path_split_obj) },
    { MP_ROM_QSTR(MP_QSTR_splitdrive),  MP_ROM_PTR(&mp_os_path_splitdrive_obj) },
    { MP_ROM_QSTR(MP_QSTR_splitext),    MP_ROM_PTR(&mp_os_path_splitext_obj) },
};
static MP_DEFINE_CONST_DICT(os_path_module_globals, os_path_module_globals_table);

static const mp_obj_module_t mp_module_os_path = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&os_path_module_globals,
};


static const mp_rom_map_elem_t os_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_os) },
    { MP_ROM_QSTR(MP_QSTR___getattr__), MP_ROM_PTR(&mp_os_getattr_obj) },
    { MP_ROM_QSTR(MP_QSTR_path),        MP_ROM_PTR(&mp_module_os_path) },
    { MP_ROM_QSTR(MP_QSTR_error),       MP_ROM_PTR(&mp_type_OSError) },
    { MP_ROM_QSTR(MP_QSTR_name),        MP_ROM_PTR(&mp_os_name_obj) },

    // Process Parameters
    { MP_ROM_QSTR(MP_QSTR_getenv),      MP_ROM_PTR(&mp_os_getenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpid),      MP_ROM_PTR(&mp_os_getpid_obj) },
    { MP_ROM_QSTR(MP_QSTR_putenv),      MP_ROM_PTR(&mp_os_putenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_supports_bytes_environ), MP_ROM_FALSE },
    { MP_ROM_QSTR(MP_QSTR_strerror),    MP_ROM_PTR(&mp_os_strerror_obj) },
    { MP_ROM_QSTR(MP_QSTR_uname),       MP_ROM_PTR(&mp_os_uname_obj) },
    { MP_ROM_QSTR(MP_QSTR_unsetenv),    MP_ROM_PTR(&mp_os_unsetenv_obj) },

    // File Object Creation
    { MP_ROM_QSTR(MP_QSTR_fdopen),      MP_ROM_PTR(&mp_builtin_open_obj) },

    // File Descriptor Operations
    { MP_ROM_QSTR(MP_QSTR_close),       MP_ROM_PTR(&mp_os_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_dup),         MP_ROM_PTR(&mp_os_dup_obj) },
    { MP_ROM_QSTR(MP_QSTR_dup2),        MP_ROM_PTR(&mp_os_dup2_obj) },
    { MP_ROM_QSTR(MP_QSTR_fsync),       MP_ROM_PTR(&mp_os_fsync_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_blocking), MP_ROM_PTR(&mp_os_get_blocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),      MP_ROM_PTR(&mp_os_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_lseek),       MP_ROM_PTR(&mp_os_lseek_obj) },
    { MP_ROM_QSTR(MP_QSTR_open),        MP_ROM_PTR(&mp_os_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_pipe),        MP_ROM_PTR(&mp_os_pipe_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),        MP_ROM_PTR(&mp_os_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendfile),    MP_ROM_PTR(&mp_os_sendfile_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_blocking), MP_ROM_PTR(&mp_os_set_blocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),       MP_ROM_PTR(&mp_os_write_obj) },

    // Files and Directories
    { MP_ROM_QSTR(MP_QSTR_chdir),       MP_ROM_PTR(&mp_os_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_fsencode),    MP_ROM_PTR(&mp_os_fsencode_obj) },
    { MP_ROM_QSTR(MP_QSTR_fsdecode),    MP_ROM_PTR(&mp_os_fsdecode_obj) },
    { MP_ROM_QSTR(MP_QSTR_fspath),      MP_ROM_PTR(&mp_os_fspath_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd),      MP_ROM_PTR(&mp_os_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_listdir),     MP_ROM_PTR(&mp_os_listdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_lstat),       MP_ROM_PTR(&mp_os_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir),       MP_ROM_PTR(&mp_os_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_makedirs),    MP_ROM_PTR(&mp_os_makedirs_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove),      MP_ROM_PTR(&mp_os_unlink_obj) },
    { MP_ROM_QSTR(MP_QSTR_removedirs),  MP_ROM_PTR(&mp_os_removedirs_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename),      MP_ROM_PTR(&mp_os_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_replace),     MP_ROM_PTR(&mp_os_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir),       MP_ROM_PTR(&mp_os_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_scandir),     MP_ROM_PTR(&mp_os_scandir_obj) },
    { MP_ROM_QSTR(MP_QSTR_DirEntry),    MP_ROM_PTR(&mp_type_os_direntry) },
    { MP_ROM_QSTR(MP_QSTR_stat),        MP_ROM_PTR(&mp_os_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat_result), MP_ROM_PTR(&mp_type_os_stat_result) },
    { MP_ROM_QSTR(MP_QSTR_statvfs),     MP_ROM_PTR(&mp_os_statvfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_supports_dir_fd), MP_ROM_PTR(&mp_const_empty_tuple_obj) },
    { MP_ROM_QSTR(MP_QSTR_supports_fd), MP_ROM_PTR(&mp_os_supports_fd_obj) },
    { MP_ROM_QSTR(MP_QSTR_supports_follow_symlinks), MP_ROM_PTR(&mp_const_empty_tuple_obj) },
    { MP_ROM_QSTR(MP_QSTR_sync),        MP_ROM_PTR(&mp_os_sync_obj) },
    { MP_ROM_QSTR(MP_QSTR_truncate),    MP_ROM_PTR(&mp_os_truncate_obj) },
    { MP_ROM_QSTR(MP_QSTR_unlink),      MP_ROM_PTR(&mp_os_unlink_obj) },
    { MP_ROM_QSTR(MP_QSTR_utime),       MP_ROM_PTR(&mp_os_utime_obj) },

    { MP_ROM_QSTR(MP_QSTR_eventfd),     MP_ROM_PTR(&mp_os_eventfd_obj) },

    // Process Management
    { MP_ROM_QSTR(MP_QSTR_abort),       MP_ROM_PTR(&mp_os_abort_obj) },
    { MP_ROM_QSTR(MP_QSTR__exit),       MP_ROM_PTR(&mp_os__exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_kill),        MP_ROM_PTR(&mp_os_kill_obj) },
    #if MICROPY_PY_OS_SYSTEM
    { MP_ROM_QSTR(MP_QSTR_system),      MP_ROM_PTR(&mp_os_system_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_times),       MP_ROM_PTR(&mp_os_times_obj) },

    // Interface to the scheduler
    #if MICROPY_PY_OS_SCHED
    { MP_ROM_QSTR(MP_QSTR_sched_getaffinity), MP_ROM_PTR(&mp_os_sched_getaffinity_obj) },
    { MP_ROM_QSTR(MP_QSTR_sched_setaffinity), MP_ROM_PTR(&mp_os_sched_setaffinity_obj) },
    #endif

    // Miscellaneous System Information
    { MP_ROM_QSTR(MP_QSTR_cpu_count),   MP_ROM_PTR(&mp_os_cpu_count_obj) },
    { MP_ROM_QSTR(MP_QSTR_curdir),      MP_ROM_QSTR(MP_QSTR__dot_) },
    { MP_ROM_QSTR(MP_QSTR_pardir),      MP_ROM_QSTR(MP_QSTR__dot__dot_) },
    { MP_ROM_QSTR(MP_QSTR_sep),         MP_ROM_QSTR(MP_QSTR__slash_) },
    { MP_ROM_QSTR(MP_QSTR_altsep),      MP_ROM_NONE },
    { MP_ROM_QSTR(MP_QSTR_extsep),      MP_ROM_QSTR(MP_QSTR__dot_) },
    { MP_ROM_QSTR(MP_QSTR_pathsep),     MP_ROM_QSTR(MP_QSTR__colon_) },
    { MP_ROM_QSTR(MP_QSTR_linesep),     MP_ROM_QSTR(MP_QSTR__0x0a_) },
    { MP_ROM_QSTR(MP_QSTR_devnull),     MP_ROM_PTR(&mp_os_devnull) },

    // Random numbers
    { MP_ROM_QSTR(MP_QSTR_urandom),     MP_ROM_PTR(&mp_os_urandom_obj) },

    // The following are MicroPython extensions.
    { MP_ROM_QSTR(MP_QSTR_dlerror),     MP_ROM_PTR(&mp_os_dlerror_obj) },
    { MP_ROM_QSTR(MP_QSTR_dlflash),     MP_ROM_PTR(&mp_os_dlflash_obj) },
    { MP_ROM_QSTR(MP_QSTR_dllist),      MP_ROM_PTR(&mp_os_dllist_obj) },
    { MP_ROM_QSTR(MP_QSTR_dlopen),      MP_ROM_PTR(&mp_os_dlopen_obj) },
    { MP_ROM_QSTR(MP_QSTR_dlsym),       MP_ROM_PTR(&mp_os_dlsym_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkfs),        MP_ROM_PTR(&mp_os_mkfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_mount),       MP_ROM_PTR(&mp_os_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount),      MP_ROM_PTR(&mp_os_umount_obj) },
    #if defined(__ARM_ARCH) && (__ARM_ARCH >= 8) && defined(__thumb__)
    { MP_ROM_QSTR(MP_QSTR_dlcall),      MP_ROM_PTR(&mp_os_dlcall_obj) },
    #endif
    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_dldebug),     MP_ROM_PTR(&mp_os_dldebug_obj) },
    #endif


    // Flags for lseek
    { MP_ROM_QSTR(MP_QSTR_SEEK_SET),    MP_ROM_INT(SEEK_SET) },
    { MP_ROM_QSTR(MP_QSTR_SEEK_CUR),    MP_ROM_INT(SEEK_CUR) },
    { MP_ROM_QSTR(MP_QSTR_SEEK_END),    MP_ROM_INT(SEEK_END) },

    // Flags for open
    { MP_ROM_QSTR(MP_QSTR_O_RDONLY),    MP_ROM_INT(O_RDONLY) },
    { MP_ROM_QSTR(MP_QSTR_O_WRONLY),    MP_ROM_INT(O_WRONLY) },
    { MP_ROM_QSTR(MP_QSTR_O_RDWR),      MP_ROM_INT(O_RDWR) },
    { MP_ROM_QSTR(MP_QSTR_O_APPEND),    MP_ROM_INT(O_APPEND) },
    { MP_ROM_QSTR(MP_QSTR_O_CREAT),     MP_ROM_INT(O_CREAT) },
    { MP_ROM_QSTR(MP_QSTR_O_EXCL),      MP_ROM_INT(O_EXCL) },
    { MP_ROM_QSTR(MP_QSTR_O_TRUNC),     MP_ROM_INT(O_TRUNC) },
    { MP_ROM_QSTR(MP_QSTR_O_SYNC),      MP_ROM_INT(O_SYNC) },
    { MP_ROM_QSTR(MP_QSTR_O_NONBLOCK),  MP_ROM_INT(O_NONBLOCK) },
    { MP_ROM_QSTR(MP_QSTR_O_NOCTTY),    MP_ROM_INT(O_NOCTTY) },

    // Flags for mount
    { MP_ROM_QSTR(MP_QSTR_MS_RDONLY),   MP_ROM_INT(MS_RDONLY) },
    { MP_ROM_QSTR(MP_QSTR_MS_REMOUNT),  MP_ROM_INT(MS_REMOUNT) },
};
static MP_DEFINE_CONST_DICT(os_module_globals, os_module_globals_table);

const mp_obj_module_t mp_module_os = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&os_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_os, mp_module_os);
