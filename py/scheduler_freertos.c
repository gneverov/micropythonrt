// SPDX-FileCopyrightText: 2017 Damien P. George
// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "py/mphal.h"
#include "py/runtime.h"

#if MICROPY_ENABLE_SCHEDULER

#define IDX_MASK(i) ((i) & (MICROPY_SCHEDULER_DEPTH - 1))

// This is a macro so it is guaranteed to be inlined in functions like
// mp_sched_schedule that may be located in a special memory region.
#define mp_sched_full() (mp_sched_num_pending() == MICROPY_SCHEDULER_DEPTH)

static inline bool mp_sched_empty(void) {
    MP_STATIC_ASSERT(MICROPY_SCHEDULER_DEPTH <= 255); // MICROPY_SCHEDULER_DEPTH must fit in 8 bits
    MP_STATIC_ASSERT((IDX_MASK(MICROPY_SCHEDULER_DEPTH) == 0)); // MICROPY_SCHEDULER_DEPTH must be a power of 2

    return mp_sched_num_pending() == 0;
}

static int mp_sched_run_pending(bool raise_exc) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    if (MP_STATE_VM(sched_state) != MP_SCHED_PENDING) {
        // Something else (e.g. hard IRQ) locked the scheduler while we
        // acquired the lock.
        MICROPY_END_ATOMIC_SECTION(atomic_state);
        return 0;
    }

    // Equivalent to mp_sched_lock(), but we're already in the atomic
    // section and know that we're pending.
    MP_STATE_VM(sched_state) = MP_SCHED_LOCKED;

    // Run at most one pending Python callback.
    mp_sched_item_t item = { 0 };
    if (!mp_sched_empty()) {
        item = MP_STATE_VM(sched_queue)[MP_STATE_VM(sched_idx)];
        MP_STATE_VM(sched_idx) = IDX_MASK(MP_STATE_VM(sched_idx) + 1);
        --MP_STATE_VM(sched_len);
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);

    nlr_buf_t nlr;
    mp_obj_t exc = MP_OBJ_NULL;
    if (nlr_push(&nlr) == 0) {
        if (item.func) {
            mp_call_function_1(item.func, item.arg);
        }
        nlr_pop();
    } else {
        exc = MP_OBJ_FROM_PTR(nlr.ret_val);
    }
    // Restore MP_STATE_VM(sched_state) to idle (or pending if there are still
    // tasks in the queue).
    mp_sched_unlock();

    if (exc == MP_OBJ_NULL) {
    } else if (raise_exc) {
        nlr_raise(exc);
    } else {
        mp_obj_print_exception(&mp_plat_print, exc);
    }
    return 1;
}

// Locking the scheduler prevents tasks from executing (does not prevent new
// tasks from being added). We lock the scheduler while executing scheduled
// tasks and also in hard interrupts or GC finalisers.
void mp_sched_lock(void) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    if (MP_STATE_VM(sched_state) < 0) {
        // Already locked, increment lock (recursive lock).
        --MP_STATE_VM(sched_state);
    } else {
        // Pending or idle.
        MP_STATE_VM(sched_state) = MP_SCHED_LOCKED;
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
}

void mp_sched_unlock(void) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    assert(MP_STATE_VM(sched_state) < 0);
    if (++MP_STATE_VM(sched_state) == 0) {
        // Scheduler became unlocked. Check if there are still tasks in the
        // queue and set sched_state accordingly.
        if (mp_sched_num_pending()) {
            MP_STATE_VM(sched_state) = MP_SCHED_PENDING;
        } else {
            MP_STATE_VM(sched_state) = MP_SCHED_IDLE;
        }
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
}

bool mp_sched_schedule(mp_obj_t function, mp_obj_t arg) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    bool ret;
    if (!mp_sched_full()) {
        if (MP_STATE_VM(sched_state) == MP_SCHED_IDLE) {
            MP_STATE_VM(sched_state) = MP_SCHED_PENDING;
        }
        uint8_t iput = IDX_MASK(MP_STATE_VM(sched_idx) + MP_STATE_VM(sched_len)++);
        MP_STATE_VM(sched_queue)[iput].func = function;
        MP_STATE_VM(sched_queue)[iput].arg = arg;
        MICROPY_SCHED_HOOK_SCHEDULED;
        ret = true;
    } else {
        // schedule queue is full
        ret = false;
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    return ret;
}

MP_REGISTER_ROOT_POINTER(mp_sched_item_t sched_queue[MICROPY_SCHEDULER_DEPTH]);

#endif // MICROPY_ENABLE_SCHEDULER

// Called periodically from the VM or from "waiting" code (e.g. sleep) to
// process background tasks and pending exceptions (e.g. KeyboardInterrupt).
void mp_handle_pending(bool raise_exc) {
    // Handle any pending exception.
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_t obj = MP_STATE_THREAD(mp_pending_exception);
    MP_STATE_THREAD(mp_pending_exception) = MP_OBJ_NULL;
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (obj != MP_OBJ_NULL && raise_exc) {
        nlr_raise(obj);
    }

    // Handle any pending callbacks.
    #if MICROPY_ENABLE_SCHEDULER
    if (mp_thread_is_main_thread()) {
        while (mp_sched_run_pending(raise_exc)) {
            ;
        }
    }
    #endif
}

// Handles any pending MicroPython events without waiting for an interrupt or event.
void mp_event_handle_nowait(void) {
    #if defined(MICROPY_EVENT_POLL_HOOK_FAST) && !MICROPY_PREVIEW_VERSION_2
    // For ports still using the old macros.
    MICROPY_EVENT_POLL_HOOK_FAST
    #else
    // Process any port layer (non-blocking) events.
    MICROPY_INTERNAL_EVENT_HOOK;
    mp_handle_pending(true);
    #endif
}
