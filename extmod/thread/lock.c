// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "extmod/modos_newlib.h"
#include "extmod/thread/modthread.h"
#include "py/extras.h"
#include "py/runtime.h"
#include "py/parseargs.h"

#include "FreeRTOS.h"
#include "semphr.h"


// Create a new Lock object
__attribute__((visibility("hidden")))
mp_obj_t mp_thread_lock_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    mp_obj_thread_lock_t *self = mp_obj_malloc_with_finaliser(mp_obj_thread_lock_t, type);
    if (type == &mp_type_thread_rlock) {
        self->mutex = xSemaphoreCreateRecursiveMutexStatic(&self->mutex_buffer);
    } else {
        self->mutex = xSemaphoreCreateBinaryStatic(&self->mutex_buffer);
        xSemaphoreGive(self->mutex);
    }
    return MP_OBJ_FROM_PTR(self);
}

// Finalizer for Lock objects
static mp_obj_t mp_thread_lock_del(mp_obj_t self_in) {
    mp_obj_thread_lock_t *self = MP_OBJ_TO_PTR(self_in);

    if (self->mutex != NULL) {
        vSemaphoreDelete(self->mutex);
        self->mutex = NULL;
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_lock_del_obj, mp_thread_lock_del);

// Helper function for lock acquire with timeout
static int mutex_take_timeout(SemaphoreHandle_t mutex, int recursive, TickType_t *pxTicksToWait) {
    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    for (;;) {
        if (thread_enable_interrupt()) {
            return -1;
        }
        BaseType_t result = recursive ? xSemaphoreTakeRecursive(mutex, *pxTicksToWait) : xSemaphoreTake(mutex, *pxTicksToWait);
        thread_disable_interrupt();
        BaseType_t timed_out = xTaskCheckForTimeOut(&xTimeOut, pxTicksToWait);
        if (thread_check_interrupted()) {
            return -1;
        }
        if (result == pdTRUE) {
            return 1;
        }
        if (timed_out) {
            return 0;
        }
    }
}

// acquire() method - Acquire the lock
static mp_obj_t mp_thread_lock_acquire(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    mp_obj_thread_lock_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    const qstr kws[] = { MP_QSTR_blocking, MP_QSTR_timeout, 0 };
    mp_int_t blocking = 1;
    mp_float_t timeout = MICROPY_FLOAT_CONST(-1.0);
    parse_args_and_kw_map(n_args - 1, pos_args + 1, kw_args, "|pf", kws, &blocking, &timeout);
    if (timeout != MICROPY_FLOAT_CONST(-1.0)) {
        if (!blocking || (timeout < MICROPY_FLOAT_CONST(0.0))) {
            mp_raise_ValueError(NULL);
        }
        if (timeout > MP_THREAD_TIMEOUT_MAX) {
            mp_raise_type(&mp_type_OverflowError);
        }
    }
    if (!blocking) {
        timeout = MICROPY_FLOAT_CONST(0.0);
    }
    TickType_t xTicksToWait = (timeout >= 0) ? (timeout * configTICK_RATE_HZ) : portMAX_DELAY;

    int ret;
    MP_OS_CALL(ret, mutex_take_timeout, self->mutex, self->base.type == &mp_type_thread_rlock, &xTicksToWait);
    return mp_obj_new_bool(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thread_lock_acquire_obj, 1, mp_thread_lock_acquire);

// release() method - Release the lock
static mp_obj_t mp_thread_lock_release(mp_obj_t self_in) {
    mp_obj_thread_lock_t *self = MP_OBJ_TO_PTR(self_in);
    
    BaseType_t result = pdFALSE;
    if (self->base.type != &mp_type_thread_rlock) {
        result = xSemaphoreGive(self->mutex);
    }
    else if (xSemaphoreGetMutexHolder(self->mutex) == xTaskGetCurrentTaskHandle()) {
        result = xSemaphoreGiveRecursive(self->mutex);
    }
    if (result != pdTRUE) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("cannot release un-acquired lock"));
    }
    
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_lock_release_obj, mp_thread_lock_release);

// locked() method - Return True if lock is currently held
static mp_obj_t mp_thread_lock_locked(mp_obj_t self_in) {
    mp_obj_thread_lock_t *self = MP_OBJ_TO_PTR(self_in);
  
    // Check if the mutex is held by any task
    int locked = uxSemaphoreGetCount(self->mutex) == 0;
    return mp_obj_new_bool(locked);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_lock_locked_obj, mp_thread_lock_locked);

// Context manager methods
static mp_obj_t mp_thread_lock_enter(mp_obj_t self_in) {
    mp_obj_t args[] = {self_in};
    mp_obj_t result = mp_thread_lock_acquire(1, args, NULL);
    if (!mp_obj_is_true(result)) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("could not acquire lock"));
    }
    return self_in;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_lock_enter_obj, mp_thread_lock_enter);

static mp_obj_t mp_thread_lock_exit(size_t n_args, const mp_obj_t *args) {
    mp_thread_lock_release(args[0]);
    return mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_thread_lock_exit_obj, 4, 4, mp_thread_lock_exit);

// Lock class locals dict
static const mp_rom_map_elem_t mp_thread_lock_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&mp_thread_lock_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_acquire),         MP_ROM_PTR(&mp_thread_lock_acquire_obj) },
    { MP_ROM_QSTR(MP_QSTR_release),         MP_ROM_PTR(&mp_thread_lock_release_obj) },
    { MP_ROM_QSTR(MP_QSTR_locked),          MP_ROM_PTR(&mp_thread_lock_locked_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),       MP_ROM_PTR(&mp_thread_lock_enter_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),        MP_ROM_PTR(&mp_thread_lock_exit_obj) },
};
static MP_DEFINE_CONST_DICT(mp_thread_lock_locals_dict, mp_thread_lock_locals_dict_table);

// Lock type definition
MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_thread_lock,
    MP_QSTR_Lock,
    MP_TYPE_FLAG_NONE,
    locals_dict, &mp_thread_lock_locals_dict
);


static mp_obj_t mp_thread_rlock_is_owned(mp_obj_t self_in) {
    mp_obj_thread_lock_t *self = MP_OBJ_TO_PTR(self_in);    
    return mp_obj_new_bool(xSemaphoreGetMutexHolder(self->mutex) == xTaskGetCurrentTaskHandle());
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_rlock_is_owned_obj, mp_thread_rlock_is_owned);

static mp_obj_t mp_thread_rlock_acquire_restore(mp_obj_t self_in, mp_obj_t state) {
    mp_int_t count;
    mp_obj_t owner;
    parse_args(1, &state, "(iO)", &count, &owner);
    if (!mp_obj_equal(owner, mp_thread_get_ident())) {
        mp_raise_type(&mp_type_RuntimeError);
    }

    mp_thread_lock_enter(self_in);
    while (--count) {
        mp_thread_lock_acquire(1, &self_in, NULL);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_thread_rlock_acquire_restore_obj, mp_thread_rlock_acquire_restore);

static mp_obj_t mp_thread_rlock_release_save(mp_obj_t self_in) {
    mp_thread_lock_release(self_in);
    mp_int_t count = 1;
    while (mp_obj_is_true(mp_thread_rlock_is_owned(self_in))) {
        mp_thread_lock_release(self_in);
        count++;
    }

    mp_obj_t items[] = { MP_OBJ_NEW_SMALL_INT(count), mp_thread_get_ident() };
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_thread_rlock_release_save_obj, mp_thread_rlock_release_save);

// RLock class locals dict
static const mp_rom_map_elem_t mp_thread_rlock_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&mp_thread_lock_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_acquire),         MP_ROM_PTR(&mp_thread_lock_acquire_obj) },
    { MP_ROM_QSTR(MP_QSTR_release),         MP_ROM_PTR(&mp_thread_lock_release_obj) },
    { MP_ROM_QSTR(MP_QSTR__acquire_restore), MP_ROM_PTR(&mp_thread_rlock_acquire_restore_obj) },
    { MP_ROM_QSTR(MP_QSTR__release_save),   MP_ROM_PTR(&mp_thread_rlock_release_save_obj) },
    { MP_ROM_QSTR(MP_QSTR__is_owned),       MP_ROM_PTR(&mp_thread_rlock_is_owned_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),       MP_ROM_PTR(&mp_thread_lock_enter_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),        MP_ROM_PTR(&mp_thread_lock_exit_obj) },
};
static MP_DEFINE_CONST_DICT(mp_thread_rlock_locals_dict, mp_thread_rlock_locals_dict_table);

// RLock type definition
MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_thread_rlock,
    MP_QSTR_RLock,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thread_lock_make_new,
    locals_dict, &mp_thread_rlock_locals_dict
);
