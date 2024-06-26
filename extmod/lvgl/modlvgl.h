// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "py/obj.h"

extern const mp_obj_module_t lvgl_module;

void lvgl_lock(void);

void lvgl_lock_init(void);

void lvgl_unlock(void);

bool lvgl_is_locked(void);

mp_obj_t lvgl_check_result(lv_result_t res);
