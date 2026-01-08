// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "py/gc.h"
#include "py/mpconfig.h"
#include "py/objtype.h"
#include "py/runtime.h"

#if MICROPY_PY_WEAKREF
typedef struct mp_obj_weakref {
    mp_obj_base_t base;
    struct mp_obj_weakref *next;
    uintptr_t target;
    mp_obj_t callback;
} mp_obj_weakref_t;

static mp_obj_t mp_weakref_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 2, false);

    const mp_obj_type_t *target_type = mp_obj_get_type(args[0]);
    if (!mp_obj_is_instance_type(target_type)) {
        mp_raise_ValueError(NULL);
    }
    mp_obj_instance_t *target = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t callback = (n_args > 1) ? args[1] : MP_OBJ_NULL;
    if (callback && !mp_obj_is_callable(callback)) {
        mp_raise_TypeError(NULL);
    }

    mp_obj_weakref_t *self = mp_obj_malloc_with_finaliser(mp_obj_weakref_t, type);
    self->next = target->weakref;
    self->target = ~(uintptr_t)target;
    self->callback = callback;
    target->weakref = self;
    gc_set_weakref(target, true);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mp_weakref_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    mp_obj_weakref_t *self = MP_OBJ_TO_PTR(self_in);
    return MP_OBJ_FROM_PTR((void *)~self->target);
}

void mp_weakref_finalize(mp_obj_t obj) {
    const mp_obj_type_t *type = mp_obj_get_type(obj);
    if (!mp_obj_is_instance_type(type)) {
        return;
    }
    mp_sched_lock();
    mp_obj_instance_t *inst = MP_OBJ_TO_PTR(obj);
    for (mp_obj_weakref_t *self = inst->weakref; self; self = self->next) {
        if (self->target != ~(uintptr_t)MP_OBJ_TO_PTR(obj)) {
            continue;
        }
        if (self->callback) {
            mp_obj_t arg = MP_OBJ_FROM_PTR(self);
            mp_call_function_n_protected(self->callback, 1, &arg);
        }
        self->target = ~(uintptr_t)MP_OBJ_TO_PTR(mp_const_none);
    }
    mp_sched_unlock();
}

static const mp_rom_map_elem_t mp_weakref_locals_dict_table[] = {
};
static MP_DEFINE_CONST_DICT(mp_weakref_locals_dict, mp_weakref_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_weakref,
    MP_QSTR_weakref,
    MP_TYPE_FLAG_NONE,
    make_new, mp_weakref_make_new,
    call, mp_weakref_call,
    locals_dict, &mp_weakref_locals_dict
    );


static const mp_rom_map_elem_t mp_module_weakref_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_weakref) },
    { MP_ROM_QSTR(MP_QSTR_ref),         MP_ROM_PTR(&mp_type_weakref) },
};

static MP_DEFINE_CONST_DICT(mp_module_weakref_globals, mp_module_weakref_globals_table);

const mp_obj_module_t mp_module_weakref = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_weakref_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_weakref, mp_module_weakref);

#endif
