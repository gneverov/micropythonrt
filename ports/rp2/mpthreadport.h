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
#ifndef MICROPY_INCLUDED_RP2_MPTHREADPORT_H
#define MICROPY_INCLUDED_RP2_MPTHREADPORT_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "morelib/thread.h"

#include "py/mpthread.h"

#define MP_THREAD_DEFAULT_STACK_SIZE 4096

typedef struct {
    SemaphoreHandle_t handle;
    StaticSemaphore_t buffer;
} mp_thread_mutex_t;

void mp_thread_init(void);
void mp_thread_deinit(void);
void mp_thread_gc_others(void);
bool mp_thread_iterate(thread_t **pthread, struct _mp_state_thread_t **pstate);

static inline void mp_thread_set_state(struct _mp_state_thread_t *state) {
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_APP, state);
}

static inline struct _mp_state_thread_t *mp_thread_get_state(void) {
    return (struct _mp_state_thread_t *)pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_APP);
}

void mp_thread_mutex_init(mp_thread_mutex_t *m);

int mp_thread_mutex_lock(mp_thread_mutex_t *m, int wait);

void mp_thread_mutex_unlock(mp_thread_mutex_t *m);

bool mp_thread_mutex_check(mp_thread_mutex_t *m);

#endif // MICROPY_INCLUDED_RP2_MPTHREADPORT_H
