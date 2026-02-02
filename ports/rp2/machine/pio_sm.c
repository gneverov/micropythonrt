// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <malloc.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "rp2/pioasm.h"

#include "./pio_sm.h"
#include "machine_pin.h"
#include "extmod/modos_newlib.h"
#include "py/extras.h"
#include "py/parseargs.h"
#include "py/runtime.h"


#define OUT_PIN 1
#define SET_PIN 2
#define IN_PIN 3
#define SIDESET_PIN 4
#define JMP_PIN 5


static state_machine_obj_t *state_machine_get(mp_obj_t self_in) {
    return MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&state_machine_type)));
}

static state_machine_obj_t *state_machine_get_raise(mp_obj_t self_in) {
    state_machine_obj_t *self = state_machine_get(self_in);
    if (self->base.fd < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed file"));
    }
    return self;
}

static uint32_t state_machine_pin_list_to_mask(mp_obj_t pin_list) {
    uint32_t pin_mask = 0u;
    mp_obj_t *pins;
    size_t num_pins = mp_obj_list_get_checked(pin_list, &pins);
    for (uint i = 0; i < num_pins; i++) {
        uint pin = mp_hal_get_pin_obj(pins[i]);
        pin_mask |= 1u << pin;
    }
    return pin_mask;
}

static mp_obj_t state_machine_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_instructions, MP_QSTR_pins, MP_QSTR_sm_config, MP_QSTR_origin, MP_QSTR_version, MP_QSTR_wrap_target, MP_QSTR_wrap, MP_QSTR_defines, MP_QSTR_name, 0 };
    mp_buffer_info_t instructions;
    uint32_t pin_mask;
    mp_int_t origin = -1, version = 0;
    parse_args_and_kw(n_args, n_kw, args, "s*O&|$ii", kws, &instructions, state_machine_pin_list_to_mask, &pin_mask, &origin, &version);

    struct rp2_pio_request req = {
        .program.instructions = instructions.buf,
        .program.length = instructions.len / sizeof(uint16_t),
        .program.origin = origin,
        .program.pio_version = version,
        .pin_mask = pin_mask,
    };

    int fd = open("/dev/pio", O_RDONLY);
    mp_os_check_ret(fd);

    int ret = ioctl(fd, RP2_PIO_SM_OPEN_IOCTL, &req);
    close(fd);
    mp_os_check_ret(ret);
    fd = ret;

    state_machine_obj_t *self = mp_obj_malloc_with_finaliser(state_machine_obj_t, type);
    self->base.fd = fd;
    self->base.flags = FREAD | FWRITE;
    self->base.name = MP_OBJ_NEW_SMALL_INT(fd);
    self->base.mode = MP_OBJ_NEW_QSTR(MP_QSTR_r);
    self->base.closefd = 1;
    self->pin_config.mask = pin_mask;
    self->sm_config.config = pio_get_default_sm_config();
    self->sm_config.wrap_target = 0;
    self->sm_config.wrap = req.program.length - 1;
    return MP_OBJ_FROM_PTR(self);
}

static void state_machine_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    if (dest[0] == MP_OBJ_SENTINEL) {
        return;
    }
    switch (attr) {
        default:
            mp_super_attr(self_in, &state_machine_type, attr, dest);
            break;
    }
}

static mp_obj_t state_machine_configure_fifo(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_tx, MP_QSTR_fifo_size, MP_QSTR_threshold, MP_QSTR_dma_transfer_size, MP_QSTR_bswap, 0 };
    state_machine_obj_t *self;
    mp_int_t tx, fifo_size = 16, threshold = 0, dma_transfer_size = DMA_SIZE_8, bswap = false;
    parse_args_and_kw_map(n_args, args, kw_args, "O&p|iiip", kws, state_machine_get_raise, &self, &tx, &fifo_size, &threshold, &dma_transfer_size, &bswap);

    struct rp2_pio_fifo_config config = {
        .fifo_size = fifo_size,
        .tx = tx,
        .dma_transfer_size = dma_transfer_size,
        .bswap = bswap,
    };
    int ret = ioctl(self->base.fd, RP2_PIO_SM_CONFIGURE_FIFO_IOCTL, &config);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_configure_fifo_obj, 1, state_machine_configure_fifo);

static mp_obj_t state_machine_set_config(mp_obj_t self_in, mp_obj_t config_in) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    mp_buffer_info_t config;
    mp_get_buffer_raise(config_in, &config, MP_BUFFER_READ);
    if (config.len < sizeof(pio_sm_config)) {
        mp_raise_ValueError(NULL);
    }
    memcpy(&self->sm_config.config, config.buf, sizeof(pio_sm_config));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(state_machine_set_config_obj, state_machine_set_config);

static mp_obj_t state_machine_set_pins(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_pin_type, MP_QSTR_pin_base, MP_QSTR_pin_count, 0 };
    state_machine_obj_t *self;
    mp_int_t pin_type, pin_base, pin_count;
    parse_args_and_kw_map(n_args, args, kw_args, "O&iO&i", kws, state_machine_get_raise, &self, &pin_type, mp_hal_get_pin_obj, &pin_base, &pin_count);

    uint pin_mask = ((1u << pin_count) - 1) << pin_base;
    if ((self->pin_config.mask & pin_mask) != pin_mask) {
        mp_raise_ValueError(NULL);
    }

    switch (pin_type) {
        case OUT_PIN:
            sm_config_set_out_pins(&self->sm_config.config, pin_base, pin_count);
            break;
        case SET_PIN:
            sm_config_set_set_pins(&self->sm_config.config, pin_base, pin_count);
            break;
        case IN_PIN:
            sm_config_set_in_pins(&self->sm_config.config, pin_base);
            #if PICO_PIO_VERSION > 0
            sm_config_set_in_pin_count(&self->sm_config.config, pin_count);
            #endif
            // gpio_set_dir_in_masked(pin_mask);
            break;
        case SIDESET_PIN:
            sm_config_set_sideset_pins(&self->sm_config.config, pin_base);
            break;
        case JMP_PIN:
            sm_config_set_jmp_pin(&self->sm_config.config, pin_base);
            // gpio_set_dir_in_masked(pin_mask);
            break;
        default:
            mp_raise_ValueError(NULL);
            break;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_pins_obj, 1, state_machine_set_pins);

static void state_machine_configure_pins(state_machine_obj_t *self) {
    int ret = ioctl(self->base.fd, RP2_PIO_SM_CONFIGURE_PINS_IOCTL, &self->pin_config);
    mp_os_check_ret(ret);
}

static mp_obj_t state_machine_set_pulls(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_pull_up, MP_QSTR_pull_down, 0 };
    state_machine_obj_t *self;
    uint32_t pull_up_mask, pull_down_mask;
    parse_args_and_kw_map(n_args, args, kw_args, "O&O&O&", kws, state_machine_get_raise, &self, state_machine_pin_list_to_mask, &pull_up_mask, state_machine_pin_list_to_mask, &pull_down_mask);

    uint32_t pull_mask = pull_up_mask | pull_down_mask;
    if ((self->pin_config.mask & pull_mask) != pull_mask) {
        mp_raise_ValueError(NULL);
    }

    self->pin_config.pull_ups = pull_up_mask;
    self->pin_config.pull_downs = pull_down_mask;
    state_machine_configure_pins(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_pulls_obj, 1, state_machine_set_pulls);

static mp_obj_t state_machine_set_sideset(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_bit_count, MP_QSTR_optional, MP_QSTR_pindirs, 0 };
    state_machine_obj_t *self;
    mp_int_t bit_count, optional, pindirs;
    parse_args_and_kw_map(n_args, args, kw_args, "O&ipp", kws, state_machine_get_raise, &self, &bit_count, &optional, &pindirs);

    sm_config_set_sideset(&self->sm_config.config, bit_count, optional, pindirs);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_sideset_obj, 1, state_machine_set_sideset);

static mp_obj_t state_machine_set_frequency(size_t n_args, const mp_obj_t *args) {
    state_machine_obj_t *self;
    mp_float_t freq;
    parse_args(n_args, args, "O&f", state_machine_get_raise, &self, &freq);

    uint32_t sysclk = clock_get_hz(clk_sys);
    sm_config_set_clkdiv(&self->sm_config.config, sysclk / freq);
    uint32_t clkdiv = self->sm_config.config.clkdiv >> 8;
    return mp_obj_new_float(sysclk / (clkdiv / 256.0f));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_frequency_obj, 1, 2, state_machine_set_frequency);

static mp_obj_t state_machine_set_wrap(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_wrap_target, MP_QSTR_wrap, 0 };
    state_machine_obj_t *self;
    mp_int_t wrap_target, wrap;
    parse_args_and_kw_map(n_args, args, kw_args, "O&ii", kws, state_machine_get_raise, &self, &wrap_target, &wrap);

    self->sm_config.wrap_target = wrap_target;
    self->sm_config.wrap = wrap;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_wrap_obj, 1, state_machine_set_wrap);

static mp_obj_t state_machine_set_shift(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_out, MP_QSTR_shift_right, MP_QSTR_auto, MP_QSTR_threshold, 0 };
    state_machine_obj_t *self;
    mp_int_t pin_type, shift_right, _auto, threshold;
    parse_args_and_kw_map(n_args, args, kw_args, "O&ippi", kws, state_machine_get_raise, &self, &pin_type, &shift_right, &_auto, &threshold);

    switch (pin_type) {
        case OUT_PIN:
            sm_config_set_out_shift(&self->sm_config.config, shift_right, _auto, threshold);
            break;
        case IN_PIN:
            sm_config_set_in_shift(&self->sm_config.config, shift_right, _auto, threshold);
            break;
        default:
            mp_raise_ValueError(NULL);
            break;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_shift_obj, 1, state_machine_set_shift);

static mp_obj_t state_machine_set_fifo_join(size_t n_args, const mp_obj_t *args) {
    state_machine_obj_t *self;
    mp_int_t join;
    parse_args(n_args, args, "O&i", state_machine_get_raise, &self, &join);

    sm_config_set_fifo_join(&self->sm_config.config, join);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_fifo_join_obj, 1, 2, state_machine_set_fifo_join);

static mp_obj_t state_machine_set_out_special(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_sticky, MP_QSTR_has_enable_pin, MP_QSTR_enable_bit_index, 0 };
    state_machine_obj_t *self;
    mp_int_t sticky, has_enable_pin, enable_bit_index;
    parse_args_and_kw_map(n_args, args, kw_args, "O&ppp", kws, state_machine_get_raise, &self, &sticky, &has_enable_pin, &enable_bit_index);

    sm_config_set_out_special(&self->sm_config.config, sticky, has_enable_pin, enable_bit_index);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_out_special_obj, 1, state_machine_set_out_special);

static mp_obj_t state_machine_set_mov_status(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_status_sel, MP_QSTR_status_n, 0 };
    state_machine_obj_t *self;
    mp_int_t status_sel, status_n;
    parse_args_and_kw_map(n_args, args, kw_args, "O&ii", kws, state_machine_get_raise, &self, &status_sel, &status_n);

    sm_config_set_mov_status(&self->sm_config.config, status_sel, status_n);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_mov_status_obj, 1, state_machine_set_mov_status);

static mp_obj_t state_machine_reset(size_t n_args, const mp_obj_t *args) {
    state_machine_obj_t *self = state_machine_get_raise(args[0]);
    uint initial_pc = 0;
    if (n_args > 1) {
        initial_pc = mp_obj_get_int(args[1]);
    }
    self->sm_config.initial_pc = initial_pc;

    int ret = ioctl(self->base.fd, RP2_PIO_SM_CONFIGURE_IOCTL, &self->sm_config);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_reset_obj, 1, 2, state_machine_reset);

static mp_obj_t state_machine_set_enabled(mp_obj_t self_in, mp_obj_t enabled_obj) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    int ret = ioctl(self->base.fd, RP2_PIO_SM_SET_ENABLED_IOCTL, mp_obj_is_true(enabled_obj));
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(state_machine_set_enabled_obj, state_machine_set_enabled);

static mp_obj_t state_machine_get_pc(mp_obj_t self_in) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    int ret = ioctl(self->base.fd, RP2_PIO_SM_GET_PC_IOCTL);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_1(state_machine_get_pc_obj, state_machine_get_pc);

static mp_obj_t state_machine_exec(mp_obj_t self_in, mp_obj_t instr_obj) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    uint instr = mp_obj_get_int(instr_obj);
    int ret = ioctl(self->base.fd, RP2_PIO_SM_EXEC_IOCTL, instr);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_2(state_machine_exec_obj, state_machine_exec);

static mp_obj_t state_machine_set_pin_values(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_set, MP_QSTR_clear, 0 };
    state_machine_obj_t *self;
    uint32_t set_mask, clear_mask;
    parse_args_and_kw_map(n_args, args, kw_args, "O&O&O&", kws, state_machine_get_raise, &self, state_machine_pin_list_to_mask, &set_mask, state_machine_pin_list_to_mask, &clear_mask);

    uint32_t mask = set_mask | clear_mask;
    if ((set_mask & clear_mask) || ((self->pin_config.mask & mask) != mask)) {
        mp_raise_ValueError(NULL);
    }

    self->pin_config.values &= ~clear_mask;
    self->pin_config.values |= set_mask;
    state_machine_configure_pins(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_pin_values_obj, 1, state_machine_set_pin_values);

static mp_obj_t state_machine_set_pindirs(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_in, MP_QSTR_out, 0 };
    state_machine_obj_t *self;
    uint32_t in_mask, out_mask;
    parse_args_and_kw_map(n_args, args, kw_args, "O&O&O&", kws, state_machine_get_raise, &self, state_machine_pin_list_to_mask, &in_mask, state_machine_pin_list_to_mask, &out_mask);

    uint mask = in_mask | out_mask;
    if ((in_mask & out_mask) || ((self->pin_config.mask & mask) != mask)) {
        mp_raise_ValueError(NULL);
    }

    self->pin_config.dirs &= ~in_mask;
    self->pin_config.dirs |= out_mask;
    state_machine_configure_pins(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_set_pindirs_obj, 1, state_machine_set_pindirs);

static mp_obj_t state_machine_drain(mp_obj_t self_in) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    int ret = ioctl(self->base.fd, RP2_PIO_SM_DRAIN_IOCTL, 0);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(state_machine_drain_obj, state_machine_drain);

static mp_obj_t state_machine_assemble(mp_obj_t cls_in, mp_obj_t source_in) {
    const char *source = mp_obj_str_get_str(source_in);

    pioasm_parser_t *parser = m_malloc(sizeof(pioasm_parser_t));
    pioasm_assemble(source, parser);
    if (parser->error != PIOASM_OK) {
        mp_raise_type_arg(
            &mp_type_ValueError,
            mp_obj_new_str_from_cstr(pioasm_error_str(parser->error)));
    }

    mp_obj_t dict = mp_obj_new_dict(3);
    mp_obj_t instructions = mp_obj_new_bytes((byte *)parser->program.instructions, parser->program.length * sizeof(uint16_t));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_instructions), instructions);

    mp_obj_t sm_config = mp_obj_new_bytes((byte *)&parser->sm_config, sizeof(pio_sm_config));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_default_config), sm_config);

    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_pio_version), MP_OBJ_NEW_SMALL_INT(parser->program.pio_version));

    if (parser->program.origin >= 0) {
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_origin), MP_OBJ_NEW_SMALL_INT(parser->program.origin));
    }
    if (parser->wrap_target >= 0) {
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_wrap_target), MP_OBJ_NEW_SMALL_INT(parser->wrap_target));
    }
    if (parser->wrap >= 0) {
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_wrap), MP_OBJ_NEW_SMALL_INT(parser->wrap));
    }

    for (uint i = 0; i < parser->symbol_count; i++) {
        pioasm_symbol_t *sym = &parser->symbols[i];
        if (!sym->is_public) {
            continue;
        }
        mp_obj_t key = mp_obj_new_str(sym->name, strnlen(sym->name, PIOASM_MAX_SYMBOL_NAME));
        mp_obj_t value = mp_obj_new_int(sym->value);
        mp_obj_dict_store(dict, key, value);
    }

    mp_obj_t args[3];
    args[0] = mp_obj_new_str(parser->name, strnlen(parser->name, PIOASM_MAX_SYMBOL_NAME));
    args[1] = mp_obj_new_tuple(1, &cls_in);
    args[2] = dict;
    mp_obj_t cls_out = mp_obj_make_new(&mp_type_type, 3, 0, args);

    m_free(parser);
    return cls_out;
}
static MP_DEFINE_CONST_FUN_OBJ_2(state_machine_assemble_fun, state_machine_assemble);
static MP_DEFINE_CONST_CLASSMETHOD_OBJ(state_machine_assemble_obj, MP_ROM_PTR(&state_machine_assemble_fun));

#ifndef NDEBUG
static mp_obj_t state_machine_debug(mp_obj_t self_in) {
    state_machine_obj_t *self = state_machine_get(self_in);
    rp2_pio_sm_debug(self->base.fd);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(state_machine_debug_obj, state_machine_debug);
#endif

static const mp_rom_map_elem_t state_machine_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_assemble),        MP_ROM_PTR(&state_machine_assemble_obj) },
    { MP_ROM_QSTR(MP_QSTR_configure_fifo),  MP_ROM_PTR(&state_machine_configure_fifo_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_config),      MP_ROM_PTR(&state_machine_set_config_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pins),        MP_ROM_PTR(&state_machine_set_pins_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pulls),       MP_ROM_PTR(&state_machine_set_pulls_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_sideset),     MP_ROM_PTR(&state_machine_set_sideset_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_frequency),   MP_ROM_PTR(&state_machine_set_frequency_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_wrap),        MP_ROM_PTR(&state_machine_set_wrap_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_shift),       MP_ROM_PTR(&state_machine_set_shift_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_fifo_join),   MP_ROM_PTR(&state_machine_set_fifo_join_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_out_special), MP_ROM_PTR(&state_machine_set_out_special_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_mov_status),  MP_ROM_PTR(&state_machine_set_mov_status_obj) },

    { MP_ROM_QSTR(MP_QSTR_reset),           MP_ROM_PTR(&state_machine_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_enabled),     MP_ROM_PTR(&state_machine_set_enabled_obj) },
    { MP_ROM_QSTR(MP_QSTR_exec),            MP_ROM_PTR(&state_machine_exec_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_pc),          MP_ROM_PTR(&state_machine_get_pc_obj) },

    { MP_ROM_QSTR(MP_QSTR_drain),           MP_ROM_PTR(&state_machine_drain_obj) },

    { MP_ROM_QSTR(MP_QSTR_OUT),             MP_ROM_INT(OUT_PIN) },
    { MP_ROM_QSTR(MP_QSTR_SET),             MP_ROM_INT(SET_PIN) },
    { MP_ROM_QSTR(MP_QSTR_IN),              MP_ROM_INT(IN_PIN) },
    { MP_ROM_QSTR(MP_QSTR_SIDESET),         MP_ROM_INT(SIDESET_PIN) },
    { MP_ROM_QSTR(MP_QSTR_JMP),             MP_ROM_INT(JMP_PIN) },
    { MP_ROM_QSTR(MP_QSTR_set_pin_values),  MP_ROM_PTR(&state_machine_set_pin_values_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pindirs),     MP_ROM_PTR(&state_machine_set_pindirs_obj) },

    { MP_ROM_QSTR(MP_QSTR_DMA_SIZE_8),      MP_ROM_INT(DMA_SIZE_8) },
    { MP_ROM_QSTR(MP_QSTR_DMA_SIZE_16),     MP_ROM_INT(DMA_SIZE_16) },
    { MP_ROM_QSTR(MP_QSTR_DMA_SIZE_32),     MP_ROM_INT(DMA_SIZE_32) },

    { MP_ROM_QSTR(MP_QSTR_PIO_FIFO_JOIN_NONE), MP_ROM_INT(PIO_FIFO_JOIN_NONE) },
    { MP_ROM_QSTR(MP_QSTR_PIO_FIFO_JOIN_TX), MP_ROM_INT(PIO_FIFO_JOIN_TX) },
    { MP_ROM_QSTR(MP_QSTR_PIO_FIFO_JOIN_RX), MP_ROM_INT(PIO_FIFO_JOIN_RX) },

    { MP_ROM_QSTR(MP_QSTR_STATUS_TX_LESSTHAN), MP_ROM_INT(STATUS_TX_LESSTHAN) },
    { MP_ROM_QSTR(MP_QSTR_STATUS_RX_LESSTHAN), MP_ROM_INT(STATUS_RX_LESSTHAN) },

    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_debug),           MP_ROM_PTR(&state_machine_debug_obj) },
    #endif
};
static MP_DEFINE_CONST_DICT(state_machine_locals_dict, state_machine_locals_dict_table);

extern const mp_stream_p_t mp_io_file_stream_p;

MP_DEFINE_CONST_OBJ_TYPE(
    state_machine_type,
    MP_QSTR_PioStateMachine,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT | MP_TYPE_FLAG_TRUE_SELF,
    make_new, state_machine_make_new,
    attr, state_machine_attr,
    iter, &mp_io_base_iternext,
    protocol, &mp_io_file_stream_p,
    locals_dict, &state_machine_locals_dict,
    parent, &mp_type_io_file
    );
