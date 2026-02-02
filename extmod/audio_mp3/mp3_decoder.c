// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <memory.h>

#include "extmod/audio_mp3/mp3_decoder.h"
#include "extmod/io/modio.h"
#include "py/extras.h"
#include "py/objarray.h"
#include "py/runtime.h"

#define AUDIO_MP3_MIN_INPUT_LEN 32


static void audio_mp3_decoder_init(audio_mp3_obj_decoder_t *self) {
}

static void audio_mp3_decoder_deinit(audio_mp3_obj_decoder_t *self) {
    if (self->decoder != NULL) {
        MP3FreeDecoder(self->decoder);
        self->decoder = NULL;
    }
}

static bool audio_mp3_decoder_inited(audio_mp3_obj_decoder_t *self) {
    return self->decoder;
}

static audio_mp3_obj_decoder_t *audio_mp3_decoder_get(mp_obj_t self_in) {
    return MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&audio_mp3_type_decoder)));
}

static mp_int_t audio_mp3_decoder_refill_in_buffer(audio_mp3_obj_decoder_t *self) {
    mp_obj_array_t *array = MP_OBJ_TO_PTR(self->readinto_args[2]);
    array->items = self->decoder->mainBuf + self->extraDataBytes; 
    array->len = 256;
    array->len = MAX(array->len, self->decoder->nSlots + 6);
    array->len = MIN(array->len, MAINBUF_SIZE - self->extraDataBytes);
    assert(array->len > 0);
    mp_obj_t ret_obj = mp_call_method_n_kw(1, 0, self->readinto_args);
    array->items = NULL;
    array->len = 0;
    mp_int_t ret = mp_obj_get_int(ret_obj);
    self->extraDataBytes += ret;
    return ret;
}

static void audio_mp3_decoder_open(audio_mp3_obj_decoder_t *self, mp_obj_t stream_obj) {
    self->decoder = (MP3DecInfo *)MP3InitDecoder();
    if (!self->decoder) {
        mp_raise_type(&mp_type_MemoryError);
    }

    // self->stream_obj = stream_obj;
    mp_load_method(stream_obj, MP_QSTR_readinto, self->readinto_args);
    mp_obj_t new_args[] = { MP_OBJ_NEW_SMALL_INT(0) };
    self->readinto_args[2] = mp_obj_make_new(&mp_type_bytearray, 1, 0, new_args);

    for (;;) {
        if (self->extraDataBytes < AUDIO_MP3_MIN_INPUT_LEN) {
            if (audio_mp3_decoder_refill_in_buffer(self) == 0) {
                mp_raise_type(&mp_type_EOFError);
            }
        }
        
        MP3DecInfo *mp3DecInfo = self->decoder;
        unsigned char *inbuf = mp3DecInfo->mainBuf + mp3DecInfo->mainDataBytes;
        int bytes_left = self->extraDataBytes - mp3DecInfo->mainDataBytes;        
        int offset = MP3FindSyncWord(inbuf, bytes_left);
        if (offset >= 0) {
            memmove(inbuf, inbuf + offset, bytes_left - offset);
            self->extraDataBytes -= offset;
            int err = MP3GetNextFrameInfo(self->decoder, &self->frame_info, inbuf);
            if (err == ERR_MP3_NONE) {
                break;
            }
            else if (err != ERR_MP3_INVALID_FRAMEHEADER) {
                mp_raise_type(&mp_type_ValueError);
            }
        } else {
            inbuf[0] = inbuf[bytes_left - 1];
            self->extraDataBytes = 1;
        }
    }

    self->out_buffer_size = self->frame_info.outputSamps * sizeof(short);
}

static mp_obj_t audio_mp3_decoder_close(mp_obj_t self_in) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_in);
    mp_obj_t args[2];
    mp_load_method(self->readinto_args[1], MP_QSTR_close, args);
    mp_call_method_n_kw(0, 0, args);

    audio_mp3_decoder_deinit(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(audio_mp3_decoder_close_obj, audio_mp3_decoder_close);

static int audio_mp3_decoder_decode(audio_mp3_obj_decoder_t *self, short *outbuf) {
    MP3DecInfo *mp3DecInfo = self->decoder;
    for (;;) {
        if (audio_mp3_decoder_refill_in_buffer(self) == 0) {
            return 0;
        }

        MP_THREAD_GIL_EXIT();
        unsigned char *inbuf = mp3DecInfo->mainBuf + mp3DecInfo->mainDataBytes;
        int bytes_left = self->extraDataBytes - mp3DecInfo->mainDataBytes;
        int result = MP3Decode(self->decoder, &inbuf, &bytes_left, outbuf,  0);
        MP_THREAD_GIL_ENTER();
        if (result == ERR_MP3_NONE) {
            memcpy(mp3DecInfo->mainBuf + mp3DecInfo->mainDataBytes, inbuf, bytes_left);
            self->extraDataBytes = mp3DecInfo->mainDataBytes + bytes_left;
            return 1;
        } else if (result != ERR_MP3_INDATA_UNDERFLOW) {
            mp_raise_type(&mp_type_ValueError);
        }
    }
//     if (MP3GetNextFrameInfo(self->decoder, &self->frame_info, mp3DecInfo->mainBuf + mp3DecInfo->mainDataBytes) != ERR_MP3_NONE) {
//         mp_raise_type(&mp_type_ValueError);
//     }    
}

static mp_int_t audio_mp3_decoder_read_internal(audio_mp3_obj_decoder_t *self, void *buf, size_t alloc) {
    return audio_mp3_decoder_decode(self, buf) ? self->out_buffer_size : 0;
}

static mp_obj_t audio_mp3_decoder_read(mp_obj_t self_obj, mp_obj_t size_in) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_obj);
    if (!audio_mp3_decoder_inited(self)) {
        mp_raise_type(&mp_type_ValueError);
    }
    mp_int_t size = mp_obj_get_int(size_in);
    if (size < self->out_buffer_size) {
        mp_raise_ValueError(NULL);
    }

    vstr_t vstr;
    vstr_init(&vstr, size);
    vstr.len = audio_mp3_decoder_read_internal(self, vstr.buf, vstr.alloc);
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_2(audio_mp3_decoder_read_obj, audio_mp3_decoder_read);

static mp_obj_t audio_mp3_decoder_readinto(mp_obj_t self_obj, mp_obj_t b_in) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_obj);
    if (!audio_mp3_decoder_inited(self)) {
        mp_raise_type(&mp_type_ValueError);
    }
    mp_buffer_info_t buf;
    mp_get_buffer_raise(b_in, &buf, MP_BUFFER_WRITE);   
    if (buf.len < self->out_buffer_size) {
        mp_raise_ValueError(NULL);
    }

    mp_int_t ret = audio_mp3_decoder_read_internal(self, buf.buf, buf.len);
    return MP_OBJ_NEW_SMALL_INT(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_2(audio_mp3_decoder_readinto_obj, audio_mp3_decoder_readinto);

static mp_obj_t audio_mp3_decoder_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    mp_obj_t stream_obj = args[0];

    audio_mp3_obj_decoder_t *self = mp_obj_malloc_with_finaliser(audio_mp3_obj_decoder_t, type);
    audio_mp3_decoder_init(self);

    audio_mp3_decoder_open(self, stream_obj);

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t audio_mp3_decoder_del(mp_obj_t self_in) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_in);
    audio_mp3_decoder_deinit(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(audio_mp3_decoder_del_obj, audio_mp3_decoder_del);

static void audio_mp3_decoder_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_in);
    if (attr == MP_QSTR_num_channels) {
        dest[0] = MP_OBJ_NEW_SMALL_INT(self->frame_info.nChans);
    }
    else if(attr == MP_QSTR_sample_rate) {
        dest[0] = MP_OBJ_NEW_SMALL_INT(self->frame_info.samprate);
    }
    else if (attr == MP_QSTR_bits_per_sample) {
        dest[0] = MP_OBJ_NEW_SMALL_INT(self->frame_info.bitsPerSample);
    } else {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

static const mp_rom_map_elem_t audio_mp3_decoder_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&audio_mp3_decoder_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&audio_mp3_decoder_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),            MP_ROM_PTR(&audio_mp3_decoder_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),        MP_ROM_PTR(&audio_mp3_decoder_readinto_obj) },
};
static MP_DEFINE_CONST_DICT(audio_mp3_decoder_locals_dict, audio_mp3_decoder_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    audio_mp3_type_decoder,
    MP_ROM_QSTR_CONST(MP_QSTR_AudioMP3Decoder),
    MP_TYPE_FLAG_TRUE_SELF,
    make_new, audio_mp3_decoder_make_new,
    attr, audio_mp3_decoder_attr,
    locals_dict, &audio_mp3_decoder_locals_dict,
    parent, &mp_type_io_base
    );
MP_REGISTER_OBJECT(audio_mp3_type_decoder);
