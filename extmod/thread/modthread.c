// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <signal.h>

#include "extmod/thread/modthread.h"
#include "py/builtin.h"
#include "py/extras.h"
#include "py/mpstate.h"
#include "py/runtime.h"


static size_t mp_thread_stack_size = 0;

typedef struct {
    size_t stack_size;
    mp_obj_dict_t *dict_locals;
    mp_obj_dict_t *dict_globals;
    mp_obj_t fun;
    size_t n_args;
    size_t n_kw;    
    mp_obj_t args[];
} mp_thread_entry_args_t;

static void mp_thread_entry(void *pvParameters) {
    // Execution begins here for a new thread.  We do not have the GIL.
    mp_thread_entry_args_t *args = (mp_thread_entry_args_t *)pvParameters;

    mp_state_thread_t ts = { 0 };
    mp_thread_init_state(&ts, args->stack_size, args->dict_locals, args->dict_globals);

    #if MICROPY_ENABLE_PYSTACK
    #error threading and pystack is not fully supported, for now just make a small stack
    #endif

    MP_THREAD_GIL_ENTER();

    // TODO set more thread-specific state here:
    //  cur_exception (root pointer)

    // DEBUG_printf("[thread] start ts=%p args=%p stack=%p\n", &ts, &args, MP_STATE_THREAD(stack_top));

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_call_function_n_kw(args->fun, args->n_args, args->n_kw, args->args);
        nlr_pop();
    } else {
        // uncaught exception
        // check for SystemExit
        mp_obj_base_t *exc = (mp_obj_base_t *)nlr.ret_val;
        if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
            // swallow exception silently
        } else {
            // print exception out
            mp_printf(MICROPY_ERROR_PRINTER, "Unhandled exception in thread started by ");
            mp_obj_print_helper(MICROPY_ERROR_PRINTER, args->fun, PRINT_REPR);
            mp_printf(MICROPY_ERROR_PRINTER, "\n");
            mp_obj_print_exception(MICROPY_ERROR_PRINTER, MP_OBJ_FROM_PTR(exc));
        }
    }

    // DEBUG_printf("[thread] finish ts=%p\n", &ts);

    mp_thread_set_state(NULL);
    MP_THREAD_GIL_EXIT();
}

__attribute__((visibility("hidden")))
mp_obj_t mp_thread_start_new_thread(size_t n_args, const mp_obj_t *args) {
    // Check types of parameters
    if (!mp_obj_is_callable(args[0])) {
        mp_raise_TypeError(NULL);
    }
    size_t len;
    mp_obj_t *items;
    mp_obj_get_array(args[1], &len, &items);
    mp_obj_t kwargs = (n_args > 2) ? args[2] : MP_OBJ_FROM_PTR(&mp_const_empty_dict_obj);
    if (!mp_obj_is_dict_or_ordereddict(kwargs)) {
        mp_raise_TypeError(NULL);
    }
    mp_map_t *map = mp_obj_dict_get_map(kwargs);

    // Allocate thread entry struct and copy arguments into it
    mp_thread_entry_args_t *th_args = m_new_obj_var(mp_thread_entry_args_t, args, mp_obj_t, len + 2 * map->used);
    th_args->fun = args[0];
    th_args->n_args = len;
    th_args->n_kw = map->used;
    mp_obj_t *arg = &th_args->args[0];
    for (size_t i = 0; i < len; i++) {
        *arg++ = items[i];
    }
    for (size_t i = 0; i < map->alloc; i++) {
        if (mp_map_slot_is_filled(map, i)) {
            *arg++ = map->table[i].key;
            *arg++ = map->table[i].value;
        }
    }

    // pass our locals and globals into the new thread
    th_args->dict_locals = mp_locals_get();
    th_args->dict_globals = mp_globals_get();

    // set the stack size to use
    if (mp_thread_stack_size == 0) {
        th_args->stack_size = 4096; // default stack size
    } else if (mp_thread_stack_size < 2048) {
        th_args->stack_size = 2048; // minimum stack size
    } else {
        th_args->stack_size = mp_thread_stack_size;
    }

    // Round stack size to a multiple of the word size.
    size_t stack_num_words = th_args->stack_size / sizeof(StackType_t);
    th_args->stack_size = stack_num_words * sizeof(StackType_t);

    // Adjust stack_size to provide room to recover from hitting the limit.
    th_args->stack_size -= 512;

    // Create the thread
    thread_t *thread = thread_create(mp_thread_entry, "core1", stack_num_words, th_args, 1);
    if (!thread) {
        mp_raise_OSError(ENOMEM);
    }
 
    UBaseType_t id = thread->id;
    thread_detach(thread);
    return mp_obj_new_int_from_uint(id);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_thread_start_new_thread_obj, 2, 3, mp_thread_start_new_thread);

static mp_obj_t mp_thread_interrupt_main(size_t n_args, const mp_obj_t *args) {
    int signum = (n_args > 0) ? mp_obj_get_int(args[0]) : SIGINT;
    mp_obj_t import_args[1] = { MP_ROM_QSTR(MP_QSTR_signal) };
    mp_obj_t signal = mp_builtin___import___default(1, import_args);
    mp_obj_t getsignal = mp_load_attr(signal, MP_QSTR_getsignal);
    mp_obj_t handler = mp_call_function_1(getsignal, MP_OBJ_NEW_SMALL_INT(signum));
    if (!mp_obj_is_int(handler)) {
        raise(signum);
        mp_handle_pending(true);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_thread_interrupt_main_obj, 0, 1, mp_thread_interrupt_main);

static mp_obj_t mp_thread_exit(void) {
    mp_raise_type(&mp_type_SystemExit);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_thread_exit_obj, mp_thread_exit);

static mp_obj_t mp_thread_allocate_lock(void) {
    return mp_thread_lock_make_new(&mp_type_thread_lock, 0, 0, NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_thread_allocate_lock_obj, mp_thread_allocate_lock);

__attribute__((visibility("hidden")))
mp_obj_t mp_thread_get_ident(void) {
    thread_t *thread = thread_current();
    UBaseType_t id = thread->id;
    return mp_obj_new_int_from_uint(id);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_thread_get_ident_obj, mp_thread_get_ident);

static mp_obj_t mp_thread_stack_size_fun(size_t n_args, const mp_obj_t *args) {
    mp_obj_t ret = mp_obj_new_int_from_uint(mp_thread_stack_size);
    if (n_args == 0) {
        mp_thread_stack_size = 0;
    } else {
        mp_thread_stack_size = mp_obj_get_int(args[0]);
    }
    return ret;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_thread_stack_size_obj, 0, 1, mp_thread_stack_size_fun);


__attribute__((visibility("hidden")))
void mp_thread_main_init(void) {
    // Create main thread object
    mp_obj_t kwargs[2] = {
        MP_OBJ_NEW_QSTR(MP_QSTR_name),
        mp_obj_new_str_from_cstr("MainThread"),
    };
    mp_obj_t main = mp_obj_make_new(&mp_type_thread, 0, 1, kwargs);
    mp_obj_thread_t *self = MP_OBJ_TO_PTR(main);
    self->started = xSemaphoreCreateBinaryStatic(&self->xSemaphoreBuffer);
    self->thread = thread_attach(thread_current());
    MP_STATE_MAIN_THREAD(thread_obj) = main;
}

static mp_obj_t mp_thread_enumerate(void) {
    mp_obj_t list =  mp_obj_new_list(0, NULL);
    thread_t *thread = NULL;
    mp_state_thread_t *state;
    while (mp_thread_iterate(&thread, &state)) {
        if (state->thread_obj) {
            mp_obj_list_append(list, state->thread_obj);
        }
        thread_detach(thread);
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_thread_enumerate_obj, mp_thread_enumerate);

static mp_obj_t mp_thread_main_thread(void) {
    mp_obj_t thread = MP_STATE_MAIN_THREAD(thread_obj);
    return thread ? thread : mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_thread_main_thread_obj, mp_thread_main_thread);

static mp_obj_t mp_thread_current_thread(void) {
    mp_obj_t thread = MP_STATE_THREAD(thread_obj);
    return thread ? thread : mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_thread_current_thread_obj, mp_thread_current_thread);

static const mp_rom_map_elem_t mp_module_thread_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR__thread) },
    { MP_ROM_QSTR(MP_QSTR_error),           MP_ROM_PTR(&mp_type_RuntimeError) },
    { MP_ROM_QSTR(MP_QSTR_LockType),        MP_ROM_PTR(&mp_type_thread_lock) },
    { MP_ROM_QSTR(MP_QSTR_start_new_thread), MP_ROM_PTR(&mp_thread_start_new_thread_obj) },
    { MP_ROM_QSTR(MP_QSTR_interrupt_main),  MP_ROM_PTR(&mp_thread_interrupt_main_obj) },
    { MP_ROM_QSTR(MP_QSTR_exit),            MP_ROM_PTR(&mp_thread_exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_allocate_lock),   MP_ROM_PTR(&mp_thread_allocate_lock_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_ident),       MP_ROM_PTR(&mp_thread_get_ident_obj) },
    { MP_ROM_QSTR(MP_QSTR_stack_size),      MP_ROM_PTR(&mp_thread_stack_size_obj) },   
    { MP_ROM_QSTR(MP_QSTR_TIMEOUT_MAX),     MP_ROM_INT(MP_THREAD_TIMEOUT_MAX) },

    // C implementation of functions from threading module
    { MP_ROM_QSTR(MP_QSTR_RLock),           MP_ROM_PTR(&mp_type_thread_rlock) },
    { MP_ROM_QSTR(MP_QSTR_Thread),          MP_ROM_PTR(&mp_type_thread) },
    { MP_ROM_QSTR(MP_QSTR_current_thread),  MP_ROM_PTR(&mp_thread_current_thread_obj) },
    { MP_ROM_QSTR(MP_QSTR_enumerate),       MP_ROM_PTR(&mp_thread_enumerate_obj) },
    { MP_ROM_QSTR(MP_QSTR_local),           MP_ROM_PTR(&mp_type_thread_local) },
    { MP_ROM_QSTR(MP_QSTR_main_thread),     MP_ROM_PTR(&mp_thread_main_thread_obj) },
};

static MP_DEFINE_CONST_DICT(mp_module_thread_globals, mp_module_thread_globals_table);

const mp_obj_module_t mp_module_thread = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_thread_globals,
};

MP_REGISTER_MODULE(MP_QSTR__thread, mp_module_thread);
