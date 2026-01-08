// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/obj.h"
#include "py/stream.h"


typedef struct {
    mp_obj_base_t base;
    int fd;
    int flags;
    mp_obj_t name;
    mp_obj_t mode;
    bool closefd;
} mp_obj_io_file_t;

typedef struct  {
    void *buffer;
    size_t size;
    size_t begin;
    size_t end;
} mp_io_buffer_t;

typedef struct {
    mp_obj_base_t base;
    FILE *file;
    int flags;
} mp_obj_io_buffer_t;

// Shared methods of IOBase
mp_obj_t mp_io_base_iternext(mp_obj_t self_in);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_base_enter_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_base_exit_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_base_none_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_base_readlines_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_base_tell_obj); 
MP_DECLARE_CONST_FUN_OBJ_2(mp_io_base_writelines_obj);

// Shared methods of BufferedIOBase
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_buffer_close_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_buffer_fileno_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_buffer_flush_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_buffer_isatty_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_buffer_readable_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_buffer_seek_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_buffer_seekable_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_buffer_tell_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_io_buffer_writable_obj);

// Type for FileIO
extern const mp_obj_type_t mp_type_io_file;

// Type for BufferedReader, BufferedWriter, BufferedRandom
extern const mp_obj_type_t mp_type_io_buffer;

// Type for TextIOWrapper
extern const mp_obj_type_t mp_type_io_text;

// Type for IOBase
extern const mp_obj_type_t mp_type_io_base;

void mp_io_print(void *data, const char *str, size_t len);

void mp_io_buffer_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);
mp_obj_io_buffer_t *mp_io_buffer_get(mp_obj_t self_in, int flags);
mp_obj_t mp_io_buffer_new(const mp_obj_type_t *type, mp_obj_t name, mp_obj_t mode_obj, size_t buffer_size, bool closefd);
mp_uint_t mp_io_stream_ioctl(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode);
extern const mp_stream_p_t mp_io_buffer_stream_p;
