// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <string.h>

#include "extmod/modos_newlib.h"
#include "extmod/thread/modthread.h"
#include "py/parseargs.h"
#include "py/objstr.h"
#include "py/objtype.h"
#include "py/runtime.h"


static mp_obj_thread_t *mp_thread_get(mp_obj_t self_in) {
    self_in = mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&mp_type_thread));
    mp_obj_thread_t *self = MP_OBJ_TO_PTR(self_in);
    assert(self);
    if (self->name == MP_OBJ_NULL) {
        mp_raise_ValueError(NULL);
    }
    return self;
}

static mp_obj_t mp_thread_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[]= { MP_QSTR_group, MP_QSTR_target, MP_QSTR_name, MP_QSTR_args, MP_QSTR_kwargs, MP_QSTR_daemon, 0 };
    mp_obj_t group = mp_const_none;
    mp_obj_t target = mp_const_none;
    mp_obj_t name = mp_const_none;
    mp_obj_t t_args = mp_const_empty_tuple;
    mp_obj_t t_kwargs = MP_OBJ_FROM_PTR(&mp_const_empty_dict_obj);
    mp_obj_t daemon = mp_const_none;
    parse_args_and_kw(n_args, n_kw, args, "|OOO!OO$O", kws, &group, &target, &mp_type_str, &name, &t_args, &t_kwargs, &daemon);
    
    if (group != mp_const_none) {
        mp_raise_ValueError(NULL);
    }
    if (name == mp_const_none) {
        static int thread_id;
        char tmp[20];
        snprintf(tmp, 20, "Thread-%d", ++thread_id);
        name = mp_obj_new_str_from_cstr(tmp);
    }
    else {
        mp_obj_str_get_str(name);
    }
    if (mp_obj_is_true(daemon)) {
        mp_raise_ValueError(NULL);
    }

    mp_obj_thread_t *self = mp_obj_malloc_with_finaliser(mp_obj_thread_t, type);
    self->name = name;
    self->started = NULL;
    self->thread = NULL;
    self->target = target;
    self->args = t_args;
    self->kwargs = t_kwargs;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mp_thread_del(mp_obj_t self_in) {
    self_in = mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&mp_type_thread));
    mp_obj_thread_t *self = MP_OBJ_TO_PTR(self_in);
    assert(self);    
    if (self->started) {
        vSemaphoreDelete(self->started);
    }
    thread_detach(self->thread);
    self->thread = NULL;
    self->name = MP_OBJ_NULL;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_del_obj, mp_thread_del);

static void mp_thread_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_obj_thread_t *self = mp_thread_get(self_in);

    switch (attr) {
        case MP_QSTR_name:
            if (dest[0] != MP_OBJ_SENTINEL) {
                dest[0] = self->name;
            }
            else if (dest[1] != MP_OBJ_NULL) {
                mp_obj_str_get_str(dest[1]);
                self->name = dest[1];
                dest[0] = MP_OBJ_NULL;
            }
            break;
        case MP_QSTR_ident:
            if (dest[0] != MP_OBJ_SENTINEL) {
                dest[0] = self->thread ? MP_OBJ_NEW_SMALL_INT(self->thread->id) : mp_const_none;
            }
            break;
        case MP_QSTR_daemon:
            if (dest[0] != MP_OBJ_SENTINEL) {
                dest[0] = mp_const_false;
            }
            break;
        default:
            dest[1] = MP_OBJ_SENTINEL;
            break;
    }
}

static mp_obj_t mp_thread_run(mp_obj_t self_in) {
    mp_obj_thread_t *self = mp_thread_get(self_in);
    if (self->target == mp_const_none) {
        return mp_const_none;
    }

    size_t n_args;
    mp_obj_t *items;
    mp_obj_get_array(self->args, &n_args, &items);
    self->args = mp_const_none;

    if (!mp_obj_is_dict_or_ordereddict(self->kwargs)) {
        mp_raise_TypeError(NULL);
    }
    mp_map_t *map = mp_obj_dict_get_map(self->kwargs);
    size_t n_kw = map->used;
    self->kwargs = mp_const_none;

    mp_obj_t *args = m_malloc((n_args + 2 * n_kw) * sizeof(mp_obj_t));
    mp_obj_t *arg = args;
    for (size_t i = 0; i < n_args; i++) {
        *arg++ = items[i];
    }
    items = NULL;
    
    for (size_t i = 0; i < map->alloc; i++) {
        if (mp_map_slot_is_filled(map, i)) {
            *arg++ = map->table[i].key;
            *arg++ = map->table[i].value;
        }
    }
    map = NULL;

    mp_call_function_n_kw(self->target, n_args, n_kw, args);
    self->target = mp_const_none;
    m_free(args);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_run_obj, mp_thread_run);

static mp_obj_t mp_thread_bootstrap(mp_obj_t self_in) {
    mp_obj_thread_t *self = mp_thread_get(self_in);

    // Set FreeRTOS task name
    const char *name = mp_obj_str_get_str(self->name);
    strncpy(pcTaskGetName(NULL), name, configMAX_TASK_NAME_LEN);
    // Grab a copy of our thread handle
    self->thread = thread_attach(thread_current());
    // Register the thread object
    MP_STATE_THREAD(thread_obj) = self_in;
    // Signal that we have started
    xSemaphoreGive(self->started);

    // Call run method
    mp_obj_t run_args[2];
    mp_load_method(self_in, MP_QSTR_run, run_args);
    mp_call_method_n_kw(0, 0, run_args);
    return mp_const_none;    
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_bootstrap_obj, mp_thread_bootstrap);

static mp_obj_t mp_threading_start(mp_obj_t self_in) {
    mp_obj_thread_t *self = mp_thread_get(self_in);
    if (self->started) {
        mp_raise_type(&mp_type_RuntimeError);
    }
    self->started = xSemaphoreCreateBinaryStatic(&self->xSemaphoreBuffer);

    // Start the thread
    mp_obj_t args[] = {
        mp_obj_new_bound_meth(MP_OBJ_FROM_PTR(&mp_thread_bootstrap_obj), self_in),
        MP_OBJ_FROM_PTR(&mp_const_empty_tuple_obj),
    };
    mp_thread_start_new_thread(2, args);

    // Wait for the thread to start
    MP_THREAD_GIL_EXIT();
    xSemaphoreTake(self->started, portMAX_DELAY);
    MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_threading_start_obj, mp_threading_start);

static mp_obj_t mp_thread_join(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    mp_obj_thread_t *self = mp_thread_get(args[0]);
    const qstr kws[] = { MP_QSTR_timeout, 0 };
    mp_obj_t timeout = mp_const_none;
    parse_args_and_kw_map(n_args - 1, args + 1, kwargs, "|O", kws, &timeout);
    TickType_t xTicksToWait = portMAX_DELAY;
    if (timeout != mp_const_none) {
        xTicksToWait = mp_obj_get_float(timeout) * configTICK_RATE_HZ;
    }

    if (!self->thread || (self->thread == thread_current())) {
        mp_raise_type(&mp_type_RuntimeError);
    }
    int ret;
    MP_OS_CALL(ret, thread_join, self->thread, xTicksToWait);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thread_join_obj, 1, mp_thread_join);

static mp_obj_t mp_thread_is_alive(mp_obj_t self_in) {
    mp_obj_thread_t *self = mp_thread_get(self_in);

    return mp_obj_new_bool(self->thread && self->thread->handle);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_is_alive_obj, mp_thread_is_alive);

static void mp_thread_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_obj_thread_t *self = mp_thread_get(self_in);
    mp_printf(print, "<%q(%s", mp_obj_get_type(self_in)->name, mp_obj_str_get_str(self->name));
    if (self->target) {
        mp_print_str(print, " (");
        mp_obj_print_helper(print, self->target, PRINT_REPR);
        mp_print_str(print, ")");
    }
    if (!self->thread) {
        mp_print_str(print, ", initial");
    }
    else if (self->thread->handle) {
        mp_printf(print, ", started %u", self->thread->id);
    }
    else {
        mp_printf(print, ", stopped %u", self->thread->id);
    }
    mp_print_str(print, ")>");
}

static const mp_rom_map_elem_t mp_thread_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___init__),        MP_ROM_PTR(&native_base_init_wrapper_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&mp_thread_del_obj) },

    { MP_ROM_QSTR(MP_QSTR_start),           MP_ROM_PTR(&mp_threading_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_run),             MP_ROM_PTR(&mp_thread_run_obj) },
    { MP_ROM_QSTR(MP_QSTR_join),            MP_ROM_PTR(&mp_thread_join_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_alive),        MP_ROM_PTR(&mp_thread_is_alive_obj) },    
};
static MP_DEFINE_CONST_DICT(mp_thread_locals_dict, mp_thread_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_thread,
    MP_QSTR_Thread,
    MP_TYPE_FLAG_TRUE_SELF,
    make_new, mp_thread_make_new,
    attr, mp_thread_attr,
    print, mp_thread_print,
    locals_dict, &mp_thread_locals_dict
    );