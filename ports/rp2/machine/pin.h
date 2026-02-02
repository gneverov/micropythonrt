#pragma once

// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "py/mphal.h"

typedef struct {
    mp_obj_base_t base;
    mp_hal_pin_obj_t pin;
    uint32_t flags;
    uint32_t debounce_time_us;
    int fd;
} pin_obj_t;

extern const mp_obj_type_t pin_type;
