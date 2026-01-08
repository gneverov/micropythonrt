/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
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

#include <malloc.h>
#include <unistd.h>

#include "py/runtime.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "mpthreadport.h"


#if MICROPY_PY_THREAD
uint32_t mp_thread_begin_atomic_section(void) {
    taskENTER_CRITICAL();
    return 0;
}

void mp_thread_end_atomic_section(uint32_t state) {
    taskEXIT_CRITICAL();
}

__attribute__((visibility("hidden")))
bool mp_thread_iterate(thread_t **pthread, mp_state_thread_t **pstate) {
    while (thread_iterate(pthread)) {
        thread_t *thread = *pthread;
        *pstate = pvTaskGetThreadLocalStoragePointer(thread->handle, TLS_INDEX_APP);
        if (*pstate) {
            return true;
        }
        thread_detach(thread);
    }
    *pstate = NULL;
    return false;
}

// Initialise threading support.
void mp_thread_init(void) {
}

// Shutdown threading support -- stops the second thread.
void mp_thread_deinit(void) {
    mp_obj_t exc = mp_obj_new_exception(&mp_type_SystemExit);
    thread_t *thread = NULL;
    mp_state_thread_t *state;
    while (mp_thread_iterate(&thread, &state)) {
        if (thread == thread_current()) {
            thread_detach(thread);
            continue;
        }
        state->mp_pending_exception = exc;
        thread_interrupt(thread);

        MP_THREAD_GIL_EXIT();
        thread_join(thread, portMAX_DELAY);
        MP_THREAD_GIL_ENTER();

        thread_detach(thread);
        thread = NULL;
    }
}

void mp_thread_gc_others(void) {
    thread_t *thread = NULL;
    mp_state_thread_t *state;
    while (mp_thread_iterate(&thread, &state)) {
        TaskHandle_t handle = thread_suspend(thread);
        if (handle != xTaskGetCurrentTaskHandle()) {
            // Trace root pointers.  This relies on the root pointers being organised
            // correctly in the mp_state_ctx structure.
            void **root_start = (void **)&state->dict_locals;
            void **root_end = (void **)(state + 1);
            gc_collect_root(root_start, root_end - root_start);

            void **stack_top = (void **)task_pxTopOfStack(handle);
            void **stack_bottom = (void **)state->stack_top;
            gc_collect_root(stack_top, stack_bottom - stack_top);
        }
        thread_resume(handle);
        thread_detach(thread);
    }
}

void mp_thread_mutex_init(mp_thread_mutex_t *m) {
    m->handle = xSemaphoreCreateRecursiveMutexStatic(&m->buffer);
}

int mp_thread_mutex_lock(mp_thread_mutex_t *m, int wait) {
    return xSemaphoreTake(m->handle, wait ? portMAX_DELAY: 0);
}

void mp_thread_mutex_unlock(mp_thread_mutex_t *m) {
    xSemaphoreGive(m->handle);
}

#ifndef NDEBUG
bool mp_thread_mutex_check(mp_thread_mutex_t *m) {
    return xSemaphoreGetMutexHolder(m->handle) == xTaskGetCurrentTaskHandle();
}
#endif

#endif // MICROPY_PY_THREAD
