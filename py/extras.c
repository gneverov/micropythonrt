// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "py/extras.h"
#include "py/objtuple.h"
#include "py/runtime.h"


void mp_super_attr(mp_obj_t self_in, const mp_obj_type_t *type, qstr attr, mp_obj_t *dest) {
    if (MP_OBJ_TYPE_HAS_SLOT(type, locals_dict)) {
        mp_obj_dict_t *locals_dict = MP_OBJ_TYPE_GET_SLOT(type, locals_dict);
        assert(mp_obj_is_dict_or_ordereddict(MP_OBJ_FROM_PTR(locals_dict)));
        mp_map_elem_t *member = mp_map_lookup(&locals_dict->map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
        if (member) {
            mp_convert_member_lookup(self_in, type, member->value, dest);
            return;
        }
    }

    if (MP_OBJ_TYPE_HAS_SLOT(type, parent)) {
        const mp_obj_type_t *parent_type = MP_OBJ_TYPE_GET_SLOT(type, parent);
        assert(parent_type->base.type == &mp_type_type);

        if (MP_OBJ_TYPE_HAS_SLOT(parent_type, attr) && (parent_type != &mp_type_object)) {
            mp_attr_fun_t attr_fun = MP_OBJ_TYPE_GET_SLOT(parent_type, attr);
            attr_fun(self_in, attr, dest);
        }
    }
}

size_t mp_obj_list_get_checked(mp_obj_t self_in, mp_obj_t **items) {
    if (!mp_obj_is_exact_type(self_in, &mp_type_list)) {
        mp_raise_TypeError("expected a list");
    }
    size_t len;
    mp_obj_list_get(self_in, &len, items);
    return len;
}

size_t mp_obj_tuple_get_checked(mp_obj_t self_in, mp_obj_t **items) {
    if (!mp_obj_is_obj(self_in) || !mp_obj_is_tuple_compatible(self_in)) {
        mp_raise_TypeError("expected a tuple");
    }
    size_t len;
    mp_obj_tuple_get(self_in, &len, items);
    return len;
}

static mp_obj_t mp_enter_self(mp_obj_t self_in) {
    return self_in;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_enter_self_obj, mp_enter_self);

static mp_obj_t mp_exit_close(size_t n_args, const mp_obj_t *args) {
    mp_obj_t dest[2];
    mp_load_method(args[0], MP_QSTR_close, dest);
    return mp_call_method_n_kw(0, 0, dest);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_exit_close_obj, 1, 4, mp_exit_close);
