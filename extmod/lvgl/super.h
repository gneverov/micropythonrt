// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/extras.h"
#include "py/obj.h"


// mp_obj_t lvgl_super_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);

void lvgl_super_update(mp_obj_t self_in, size_t n_kw, const mp_map_elem_t *kw);

void lvgl_super_attr_check(qstr attr, bool getter, bool setter, bool deleter, mp_obj_t *dest);

void lvgl_super_subscr_check(const mp_obj_type_t *type, bool getter, bool setter, bool deleter, mp_obj_t value);

static inline void lvgl_super_attr(mp_obj_t self_in, const mp_obj_type_t *type, qstr attr, mp_obj_t *dest) {
    mp_super_attr(self_in, type, attr, dest);
}
