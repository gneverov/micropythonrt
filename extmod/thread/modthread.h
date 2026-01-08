// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "FreeRTOS.h"
#include "morelib/thread.h"

#include "py/obj.h"

#define MP_THREAD_TIMEOUT_MAX (portMAX_DELAY / configTICK_RATE_HZ)


typedef struct {
    mp_obj_base_t base;
    mp_obj_t name;
    SemaphoreHandle_t started;
    thread_t *thread;
    mp_obj_t locals;

    StaticSemaphore_t xSemaphoreBuffer;
    mp_obj_t target;
    mp_obj_t args;
    mp_obj_t kwargs;
} mp_obj_thread_t;

// Lock object structure
typedef struct {
    mp_obj_base_t base;
    SemaphoreHandle_t mutex;
    StaticSemaphore_t mutex_buffer;
} mp_obj_thread_lock_t;

extern const mp_obj_type_t mp_type_thread;

extern const mp_obj_type_t mp_type_thread_local;

extern const mp_obj_type_t mp_type_thread_lock;

extern const mp_obj_type_t mp_type_thread_rlock;

mp_obj_t mp_thread_lock_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);

mp_obj_t mp_thread_start_new_thread(size_t n_args, const mp_obj_t *args);

mp_obj_t mp_thread_get_ident(void);

void mp_thread_main_init(void);