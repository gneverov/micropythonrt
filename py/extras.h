// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/obj.h"


void mp_super_attr(mp_obj_t self_in, const mp_obj_type_t *type, qstr attr, mp_obj_t *dest);

static inline mp_obj_t mp_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    return MP_OBJ_TYPE_GET_SLOT(type, make_new)(type, n_args, n_kw, args);
}

size_t mp_obj_list_get_checked(mp_obj_t self_in, mp_obj_t **items);

size_t mp_obj_tuple_get_checked(mp_obj_t self_in, mp_obj_t **items);

MP_DECLARE_CONST_FUN_OBJ_1(mp_enter_self_obj);

MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_exit_close_obj);
