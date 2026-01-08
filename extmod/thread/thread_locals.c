// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "extmod/thread/modthread.h"
#include "py/runtime.h"


static mp_obj_t mp_thread_local_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    return MP_OBJ_FROM_PTR(mp_obj_malloc(mp_obj_base_t, type));
}

static mp_obj_t mp_thread_local_get_dict(mp_obj_t self) {
    mp_obj_t thread_locals = MP_STATE_THREAD(thread_locals);
    if (thread_locals == MP_OBJ_NULL) {
        thread_locals = mp_obj_new_dict(0);
        MP_STATE_THREAD(thread_locals) = thread_locals;
    }

    mp_obj_t key = MP_OBJ_NEW_SMALL_INT((uintptr_t) MP_OBJ_TO_PTR(self));
    mp_map_elem_t *elem = mp_map_lookup(mp_obj_dict_get_map(thread_locals), key, MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
    assert(elem);

    if (!elem->value) {
        elem->value = mp_obj_new_dict(0);
    }
    return elem->value;
}

static const mp_obj_dict_t mp_thread_local_locals_dict;

static void mp_thread_local_attr(mp_obj_t self, qstr attr, mp_obj_t *dest) {
    mp_obj_t dict = mp_thread_local_get_dict(self);
    mp_obj_t name = MP_OBJ_NEW_QSTR(attr);
    if (dest[0] != MP_OBJ_SENTINEL) {
        if (mp_obj_str_equal(name, MP_OBJ_NEW_QSTR(MP_QSTR___dict__))) {
            dest[0] = dict;
            return;
        }
        mp_map_elem_t *elem;
        elem = mp_map_lookup(mp_obj_dict_get_map(MP_OBJ_FROM_PTR(&mp_thread_local_locals_dict)), name, MP_MAP_LOOKUP);
        if (elem) {
            dest[0] = elem->value;
            return;
        }
        elem = mp_map_lookup(mp_obj_dict_get_map(dict), name, MP_MAP_LOOKUP);
        if (elem) {
            dest[0] = elem->value;
            return;
        }
    }
    else if (dest[1] != MP_OBJ_NULL) {
        if (!mp_obj_str_equal(name, MP_OBJ_NEW_QSTR(MP_QSTR___dict__))) {
            mp_obj_dict_store(dict, name, dest[1]);
            dest[0] = MP_OBJ_NULL;
        }
    }
    else {
        if (!mp_obj_str_equal(name, MP_OBJ_NEW_QSTR(MP_QSTR___dict__))) {
            mp_obj_dict_delete(dict, name);
            dest[0] = MP_OBJ_NULL;
        }
    }
}

static mp_obj_t mp_thread_local_getattr(mp_obj_t self, mp_obj_t name) {
    mp_obj_t dest[2] = { MP_OBJ_NULL, MP_OBJ_NULL };
    mp_thread_local_attr(self, mp_obj_str_get_qstr(name), dest);
    if (dest[0] == MP_OBJ_NULL) {
        mp_raise_type(&mp_type_AttributeError);
    }
    return dest[0];
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_thread_local_getattr_obj, mp_thread_local_getattr);

static mp_obj_t mp_thread_local_setattr(mp_obj_t self, mp_obj_t name, mp_obj_t value) {
    mp_obj_t dest[2] = { MP_OBJ_SENTINEL, value };
    mp_thread_local_attr(self, mp_obj_str_get_qstr(name), dest);
    if (dest[0] == MP_OBJ_SENTINEL) {
        mp_raise_type(&mp_type_AttributeError);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(mp_thread_local_setattr_obj, mp_thread_local_setattr);

static mp_obj_t mp_thread_local_delattr(mp_obj_t self, mp_obj_t name) {
    mp_obj_t dest[2] = { MP_OBJ_SENTINEL, MP_OBJ_NULL };
    mp_thread_local_attr(self, mp_obj_str_get_qstr(name), dest);
    if (dest[0] == MP_OBJ_SENTINEL) {
        mp_raise_type(&mp_type_AttributeError);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_thread_local_delattr_obj, mp_thread_local_delattr);

static const mp_rom_map_elem_t mp_thread_local_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___getattr__),     MP_ROM_PTR(&mp_thread_local_getattr_obj) },
    { MP_ROM_QSTR(MP_QSTR___setattr__),     MP_ROM_PTR(&mp_thread_local_setattr_obj) },
    { MP_ROM_QSTR(MP_QSTR___delattr__),     MP_ROM_PTR(&mp_thread_local_delattr_obj) },
};
static MP_DEFINE_CONST_DICT(mp_thread_local_locals_dict, mp_thread_local_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_thread_local,
    MP_QSTR_local,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS | MP_TYPE_FLAG_TRUE_SELF,
    make_new, mp_thread_local_make_new,
    attr, mp_thread_local_attr,
    locals_dict, &mp_thread_local_locals_dict
    );