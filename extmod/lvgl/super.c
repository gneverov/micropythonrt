// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./super.h"

#include "py/runtime.h"


// mp_obj_t lvgl_super_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
//     assert(MP_OBJ_TYPE_HAS_SLOT(type, parent));
//     const mp_obj_type_t *parent_type = MP_OBJ_TYPE_GET_SLOT(type, parent);
//     assert(parent_type->base.type == &mp_type_type);

//     assert(MP_OBJ_TYPE_HAS_SLOT(parent_type, make_new));
//     mp_make_new_fun_t make_new_fun = MP_OBJ_TYPE_GET_SLOT(parent_type, make_new);
//     return make_new_fun(type, n_args, n_kw, args);
// }

void lvgl_super_update(mp_obj_t self_in, size_t n_kw, const mp_map_elem_t *kw) {
    const mp_map_elem_t *kw_end = kw + n_kw;
    for (; kw < kw_end; kw++) {
        if (kw->key) {
            mp_store_attr(self_in, mp_obj_str_get_qstr(kw->key), kw->value);
        }
    }
}

void lvgl_super_attr_check(qstr attr, bool getter, bool setter, bool deleter, mp_obj_t *dest) {
    if (dest[0] != MP_OBJ_SENTINEL) {
        if (!getter) {
            mp_raise_msg_varg(&mp_type_AttributeError, MP_ERROR_TEXT("can't get attribute '%q'"), attr);
        }
    }
    else if (dest[1] != MP_OBJ_NULL) {
        if (!setter) {
            mp_raise_msg_varg(&mp_type_AttributeError, MP_ERROR_TEXT("can't set attribute '%q'"), attr);
        }
    }
    else {
        if (!deleter) {
            mp_raise_msg_varg(&mp_type_AttributeError, MP_ERROR_TEXT("can't delete attribute '%q'"), attr);
        } 
    }
}

void lvgl_super_subscr_check(const mp_obj_type_t *type, bool getter, bool setter, bool deleter, mp_obj_t value) {
    qstr type_name = type->name;
    if (value == MP_OBJ_SENTINEL) {
        if (!getter) {
            mp_raise_msg_varg(&mp_type_TypeError, MP_ERROR_TEXT("'%q' object is not subscriptable"), type_name);
        }
    }
    else if (value == MP_OBJ_NULL) {
        if (!deleter) {
            mp_raise_msg_varg(&mp_type_TypeError, MP_ERROR_TEXT("'%q' object does not support item deletion"), type_name);
        }
    }
    else {
        if (!setter) {
            mp_raise_msg_varg(&mp_type_TypeError, MP_ERROR_TEXT("'%q' object does not support item assignment"), type_name);
        }
    }
}
