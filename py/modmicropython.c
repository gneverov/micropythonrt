/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2023 Gregory Neverov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>

#include "py/builtin.h"
#include "py/cstack.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mphal.h"

#if MICROPY_FREERTOS
#include "FreeRTOS.h"
#include "task.h"

#include <malloc.h>
#include <string.h>
#endif

#if MICROPY_PY_MICROPYTHON

// Various builtins specific to MicroPython runtime,
// living in micropython module

#if MICROPY_ENABLE_COMPILER
static mp_obj_t mp_micropython_opt_level(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return MP_OBJ_NEW_SMALL_INT(MP_STATE_VM(mp_optimise_value));
    } else {
        MP_STATE_VM(mp_optimise_value) = mp_obj_get_int(args[0]);
        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_micropython_opt_level_obj, 0, 1, mp_micropython_opt_level);
#endif

#if MICROPY_PY_MICROPYTHON_MEM_INFO

#if MICROPY_MEM_STATS
static mp_obj_t mp_micropython_mem_total(void) {
    return MP_OBJ_NEW_SMALL_INT(m_get_total_bytes_allocated());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_mem_total_obj, mp_micropython_mem_total);

static mp_obj_t mp_micropython_mem_current(void) {
    return MP_OBJ_NEW_SMALL_INT(m_get_current_bytes_allocated());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_mem_current_obj, mp_micropython_mem_current);

static mp_obj_t mp_micropython_mem_peak(void) {
    return MP_OBJ_NEW_SMALL_INT(m_get_peak_bytes_allocated());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_mem_peak_obj, mp_micropython_mem_peak);
#endif

mp_obj_t mp_micropython_mem_info(size_t n_args, const mp_obj_t *args) {
    (void)args;
    #if MICROPY_MEM_STATS
    mp_printf(&mp_plat_print, "mem: total=" UINT_FMT ", current=" UINT_FMT ", peak=" UINT_FMT "\n",
        (mp_uint_t)m_get_total_bytes_allocated(), (mp_uint_t)m_get_current_bytes_allocated(), (mp_uint_t)m_get_peak_bytes_allocated());
    #endif
    #if MICROPY_STACK_CHECK
    mp_printf(&mp_plat_print, "stack: " UINT_FMT " out of " UINT_FMT "\n",
        mp_cstack_usage(), (mp_uint_t)MP_STATE_THREAD(stack_limit));
    #else
    mp_printf(&mp_plat_print, "stack: " UINT_FMT "\n", mp_cstack_usage());
    #endif
    #if MICROPY_ENABLE_GC
    gc_dump_info(&mp_plat_print);
    if (n_args == 1) {
        // arg given means dump gc allocation table
        gc_dump_alloc_table(&mp_plat_print);
    }
    #else
    (void)n_args;
    #endif
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_micropython_mem_info_obj, 0, 1, mp_micropython_mem_info);

#ifndef NDEBUG
static mp_obj_t mp_micropython_from_ptr(mp_obj_t ptr_in) {
    void *ptr = (void *)mp_obj_get_int(ptr_in);
    ptr = gc_verify_ptr(ptr);
    return ptr ? MP_OBJ_FROM_PTR(ptr) : mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_micropython_from_ptr_obj, mp_micropython_from_ptr);

static mp_obj_t mp_micropython_to_ptr(mp_obj_t obj_in) {
    if (!mp_obj_is_obj(obj_in)) {
        mp_raise_TypeError(NULL);
    }
    return mp_obj_new_int((intptr_t)MP_OBJ_TO_PTR(obj_in));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_micropython_to_ptr_obj, mp_micropython_to_ptr);

static mp_obj_t mp_micropython_trace_ptr(size_t n_args, const mp_obj_t *args) {
    void *addr;
    if (mp_obj_is_obj(args[0])) {
        addr = MP_OBJ_TO_PTR(args[0]);
    } else {
        addr = (void *)mp_obj_get_int(args[0]);
    }

    size_t max_len = (n_args > 1) ? mp_obj_get_int(args[1]) : 16;
    void **buf = malloc(max_len * sizeof(void *));
    size_t len = 0;
    gc_collect();
    while (len < max_len) {
        buf[len++] = addr;
        if (addr == NULL) {
            break;
        }

        for (size_t i = 0; i < len - 1; i++) {
            if (buf[i] == addr) {
                goto end;
            }
        }

        void **next;
        switch (gc_find_ptrs(addr, 1, &next)) {
            case 0:
                addr = NULL;
                break;
            case 1:
                addr = (void *)next;
                break;
            default:
                goto end;
        }
    }

end:
    mp_obj_t list_obj = mp_obj_new_list(len, NULL);
    mp_obj_list_t *list = MP_OBJ_TO_PTR(list_obj);
    for (size_t i = 0; i < len; i++) {
        list->items[i] = mp_obj_new_int((intptr_t)buf[i]);
    }
    free(buf);
    return list_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_micropython_trace_ptr_obj, 1, 2, mp_micropython_trace_ptr);

static mp_obj_t mp_micropython_find_ptrs(mp_obj_t addr_in) {
    void *addr;
    if (mp_obj_is_obj(addr_in)) {
        addr = MP_OBJ_TO_PTR(addr_in);
    } else {
        addr = (void *)mp_obj_get_int(addr_in);
    }
    gc_collect();
    size_t len = gc_find_ptrs(addr, 0, NULL);
    void **buf = malloc(len * sizeof(void *));
    len = gc_find_ptrs(addr, len, &buf);

    mp_obj_t list_obj = mp_obj_new_list(len, NULL);
    mp_obj_list_t *list = MP_OBJ_TO_PTR(list_obj);
    for (size_t i = 0; i < len; i++) {
        list->items[i] = mp_obj_new_int((intptr_t)buf[i]);
    }
    free(buf);
    return list_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_micropython_find_ptrs_obj, mp_micropython_find_ptrs);

static mp_obj_t mp_micropython_from_qstr(mp_obj_t qstr_in) {
    mp_int_t qst = mp_obj_get_int(qstr_in);
    const qstr_pool_t *pool = MP_STATE_VM(last_pool);
    if (qst >= pool->total_prev_len + pool->len) {
        mp_raise_ValueError(NULL);
    }
    return MP_OBJ_NEW_QSTR(qst);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_micropython_from_qstr_obj, mp_micropython_from_qstr);

static mp_obj_t mp_micropython_to_qstr(mp_obj_t str_in) {
    if (mp_obj_is_qstr(str_in)) {
        return MP_OBJ_NEW_SMALL_INT(MP_OBJ_QSTR_VALUE(str_in));
    }
    if (mp_obj_is_str(str_in)) {
        return mp_const_none;
    }
    mp_raise_TypeError(NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_micropython_to_qstr_obj, mp_micropython_to_qstr);

static mp_obj_t mp_micropython_debug_break(void) {
    __breakpoint();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_debug_break_obj, mp_micropython_debug_break);
#endif

#if MICROPY_FREERTOS
mp_obj_t mp_micropython_malloc_stats() {
    malloc_stats();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_malloc_stats_obj, mp_micropython_malloc_stats);

mp_obj_t mp_micropython_tasks() {
    UBaseType_t n_tasks = uxTaskGetNumberOfTasks();
    vstr_t vstr;
    vstr_init(&vstr, 40 * n_tasks);
    vTaskList(vstr.buf);
    vstr.len = strlen(vstr.buf);
    mp_print_str(&mp_plat_print, vstr.buf);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_tasks_obj, mp_micropython_tasks);
#endif

static mp_obj_t mp_micropython_qstr_info(size_t n_args, const mp_obj_t *args) {
    (void)args;
    size_t n_pool, n_qstr, n_str_data_bytes, n_total_bytes;
    qstr_pool_info(&n_pool, &n_qstr, &n_str_data_bytes, &n_total_bytes);
    mp_printf(&mp_plat_print, "qstr pool: n_pool=%u, n_qstr=%u, n_str_data_bytes=%u, n_total_bytes=%u\n",
        (uint)n_pool, (uint)n_qstr, (uint)n_str_data_bytes, (uint)n_total_bytes);
    if (n_args == 1) {
        // arg given means dump qstr data
        qstr_dump_data();
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_micropython_qstr_info_obj, 0, 1, mp_micropython_qstr_info);

#endif // MICROPY_PY_MICROPYTHON_MEM_INFO

#if MICROPY_PY_MICROPYTHON_STACK_USE
static mp_obj_t mp_micropython_stack_use(void) {
    return MP_OBJ_NEW_SMALL_INT(mp_cstack_usage());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_stack_use_obj, mp_micropython_stack_use);
#endif

#if MICROPY_ENABLE_PYSTACK
static mp_obj_t mp_micropython_pystack_use(void) {
    return MP_OBJ_NEW_SMALL_INT(mp_pystack_usage());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_pystack_use_obj, mp_micropython_pystack_use);
#endif

#if MICROPY_ENABLE_GC
static mp_obj_t mp_micropython_heap_lock(void) {
    gc_lock();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_heap_lock_obj, mp_micropython_heap_lock);

static mp_obj_t mp_micropython_heap_unlock(void) {
    gc_unlock();
    return MP_OBJ_NEW_SMALL_INT(MP_STATE_THREAD(gc_lock_depth) >> GC_LOCK_DEPTH_SHIFT);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_heap_unlock_obj, mp_micropython_heap_unlock);

#if MICROPY_PY_MICROPYTHON_HEAP_LOCKED
static mp_obj_t mp_micropython_heap_locked(void) {
    return MP_OBJ_NEW_SMALL_INT(MP_STATE_THREAD(gc_lock_depth) >> GC_LOCK_DEPTH_SHIFT);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_micropython_heap_locked_obj, mp_micropython_heap_locked);
#endif
#endif

#if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF && (MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE == 0)
static MP_DEFINE_CONST_FUN_OBJ_1(mp_alloc_emergency_exception_buf_obj, mp_alloc_emergency_exception_buf);
#endif

#if MICROPY_KBD_EXCEPTION
static mp_obj_t mp_micropython_kbd_intr(mp_obj_t int_chr_in) {
    mp_hal_set_interrupt_char(mp_obj_get_int(int_chr_in));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_micropython_kbd_intr_obj, mp_micropython_kbd_intr);
#endif

#if MICROPY_ENABLE_SCHEDULER
static mp_obj_t mp_micropython_schedule(mp_obj_t function, mp_obj_t arg) {
    if (!mp_sched_schedule(function, arg)) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("schedule queue full"));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_micropython_schedule_obj, mp_micropython_schedule);
#endif

static const mp_rom_map_elem_t mp_module_micropython_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_micropython) },
    { MP_ROM_QSTR(MP_QSTR_const), MP_ROM_PTR(&mp_identity_obj) },
    #if MICROPY_ENABLE_COMPILER
    { MP_ROM_QSTR(MP_QSTR_opt_level), MP_ROM_PTR(&mp_micropython_opt_level_obj) },
    #endif
    #if MICROPY_PY_MICROPYTHON_MEM_INFO
    #if MICROPY_MEM_STATS
    { MP_ROM_QSTR(MP_QSTR_mem_total), MP_ROM_PTR(&mp_micropython_mem_total_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_current), MP_ROM_PTR(&mp_micropython_mem_current_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_peak), MP_ROM_PTR(&mp_micropython_mem_peak_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_mem_info), MP_ROM_PTR(&mp_micropython_mem_info_obj) },
    { MP_ROM_QSTR(MP_QSTR_qstr_info), MP_ROM_PTR(&mp_micropython_qstr_info_obj) },
    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_find_ptrs), MP_ROM_PTR(&mp_micropython_find_ptrs_obj) },
    { MP_ROM_QSTR(MP_QSTR_trace_ptr), MP_ROM_PTR(&mp_micropython_trace_ptr_obj) },
    { MP_ROM_QSTR(MP_QSTR_from_ptr), MP_ROM_PTR(&mp_micropython_from_ptr_obj) },
    { MP_ROM_QSTR(MP_QSTR_to_ptr), MP_ROM_PTR(&mp_micropython_to_ptr_obj) },
    { MP_ROM_QSTR(MP_QSTR_from_qstr), MP_ROM_PTR(&mp_micropython_from_qstr_obj) },
    { MP_ROM_QSTR(MP_QSTR_to_qstr), MP_ROM_PTR(&mp_micropython_to_qstr_obj) },
    { MP_ROM_QSTR(MP_QSTR_debug_break), MP_ROM_PTR(&mp_micropython_debug_break_obj) },
    #endif
    #if MICROPY_FREERTOS
    { MP_ROM_QSTR(MP_QSTR_malloc_stats), MP_ROM_PTR(&mp_micropython_malloc_stats_obj) },
    { MP_ROM_QSTR(MP_QSTR_tasks), MP_ROM_PTR(&mp_micropython_tasks_obj) },
    #endif
    #endif
    #if MICROPY_PY_MICROPYTHON_STACK_USE
    { MP_ROM_QSTR(MP_QSTR_stack_use), MP_ROM_PTR(&mp_micropython_stack_use_obj) },
    #endif
    #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF && (MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE == 0)
    { MP_ROM_QSTR(MP_QSTR_alloc_emergency_exception_buf), MP_ROM_PTR(&mp_alloc_emergency_exception_buf_obj) },
    #endif
    #if MICROPY_ENABLE_PYSTACK
    { MP_ROM_QSTR(MP_QSTR_pystack_use), MP_ROM_PTR(&mp_micropython_pystack_use_obj) },
    #endif
    #if MICROPY_ENABLE_GC
    { MP_ROM_QSTR(MP_QSTR_heap_lock), MP_ROM_PTR(&mp_micropython_heap_lock_obj) },
    { MP_ROM_QSTR(MP_QSTR_heap_unlock), MP_ROM_PTR(&mp_micropython_heap_unlock_obj) },
    #if MICROPY_PY_MICROPYTHON_HEAP_LOCKED
    { MP_ROM_QSTR(MP_QSTR_heap_locked), MP_ROM_PTR(&mp_micropython_heap_locked_obj) },
    #endif
    #endif
    #if MICROPY_KBD_EXCEPTION
    { MP_ROM_QSTR(MP_QSTR_kbd_intr), MP_ROM_PTR(&mp_micropython_kbd_intr_obj) },
    #endif
    #if MICROPY_PY_MICROPYTHON_RINGIO
    { MP_ROM_QSTR(MP_QSTR_RingIO), MP_ROM_PTR(&mp_type_ringio) },
    #endif
    #if MICROPY_ENABLE_SCHEDULER
    { MP_ROM_QSTR(MP_QSTR_schedule), MP_ROM_PTR(&mp_micropython_schedule_obj) },
    #endif
};

static MP_DEFINE_CONST_DICT(mp_module_micropython_globals, mp_module_micropython_globals_table);

const mp_obj_module_t mp_module_micropython = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_micropython_globals,
};

MP_REGISTER_MODULE(MP_QSTR_micropython, mp_module_micropython);

#endif // MICROPY_PY_MICROPYTHON
