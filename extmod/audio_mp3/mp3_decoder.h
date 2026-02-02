// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lib/audio/src/libhelix-mp3/mp3common.h"
#include "lib/audio/src/libhelix-mp3/mp3dec.h"

#include "py/obj.h"


typedef struct {
    mp_obj_base_t base;
    MP3DecInfo *decoder;
    MP3FrameInfo frame_info;

    mp_obj_t readinto_args[3];
    size_t extraDataBytes;

    size_t out_buffer_size;
} audio_mp3_obj_decoder_t;

extern const mp_obj_type_t audio_mp3_type_decoder;
