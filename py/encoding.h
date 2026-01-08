// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/obj.h"


qstr mp_get_encoding(mp_obj_t encoding);
mp_obj_t mp_decode(const char *bytes, size_t len, int encoding, int errors);
mp_obj_t mp_encode(const char *bytes, size_t len, int encoding, int errors);
