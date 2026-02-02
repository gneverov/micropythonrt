// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "rp2/pio_sm.h"

#include "extmod/io/modio.h"
#include "py/obj.h"


typedef struct {
    mp_obj_io_file_t base;
    struct rp2_pio_sm_config sm_config;
    struct rp2_pio_pin_config pin_config;
} state_machine_obj_t;

extern const mp_obj_type_t state_machine_type;
