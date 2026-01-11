/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <malloc.h>
#include <memory.h>

#include "py/mphal.h"
#include "py/runtime.h"
#include "rp2/flash_lockout.h"
#include "modrp2.h"
#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#if !PICO_RP2040
#include "pico/bootrom.h"
#endif

#define CS_PIN_INDEX 1

#if PICO_RP2040
#define CS_BIT (1u << CS_PIN_INDEX)
#else
#define CS_BIT SIO_GPIO_HI_IN_QSPI_CSN_BITS
#endif

// Improved version of
// https://github.com/raspberrypi/pico-examples/blob/master/picoboard/button/button.c
static bool __no_inline_not_in_flash_func(bootsel_button)(void) {
    // Disable interrupts and the other core since they might be
    // executing code from flash and we are about to temporarily
    // disable flash access.
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();

    // Set the CS pin to high impedance.
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        (GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB),
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Delay without calling any functions in flash.
    uint32_t start = timer_hw->timerawl;
    while ((uint32_t)(timer_hw->timerawl - start) <= MICROPY_HW_BOOTSEL_DELAY_US) {
        ;
    }

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // The button pulls the QSPI_SS pin *low* when pressed.
    bool button_state = !(sio_hw->gpio_hi_in & CS_BIT);

    // Restore the QSPI_SS pin so we can use flash again.
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        (GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB),
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    MICROPY_END_ATOMIC_SECTION(atomic_state);

    return button_state;
}

static mp_obj_t rp2_bootsel_button(void) {
    return MP_OBJ_NEW_SMALL_INT(bootsel_button());
}
MP_DEFINE_CONST_FUN_OBJ_0(rp2_bootsel_button_obj, rp2_bootsel_button);

static mp_obj_t rp2_get_core_num(void) {
    return MP_OBJ_NEW_SMALL_INT(get_core_num());
}
MP_DEFINE_CONST_FUN_OBJ_0(rp2_get_core_num_obj, rp2_get_core_num);

static mp_obj_t rp2_flash_do_cmd(mp_obj_t txbuf) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(txbuf, &bufinfo, MP_BUFFER_READ);
    vstr_t rxbuf;
    vstr_init_len(&rxbuf, bufinfo.len);

    flash_lockout_start();
    flash_do_cmd(bufinfo.buf, (uint8_t *)rxbuf.buf, bufinfo.len);
    flash_lockout_end();

    return mp_obj_new_bytes_from_vstr(&rxbuf);
}
MP_DEFINE_CONST_FUN_OBJ_1(rp2_flash_do_cmd_obj, rp2_flash_do_cmd);

#if !PICO_RP2040
static mp_obj_t rp2_get_sys_info(mp_obj_t flag_in) {
    uint32_t flag = mp_obj_get_int(flag_in);
    rom_get_sys_info_fn get_sys_info = rom_func_lookup(ROM_FUNC_GET_SYS_INFO);
    uint32_t buffer[5];
    int result = get_sys_info(buffer, 5, 1ul << flag);
    if (result <= 0) {
        mp_raise_ValueError(NULL);
    }
    return mp_obj_new_bytes((void *)(buffer + 1), (result - 1) * sizeof(uint32_t));
}
MP_DEFINE_CONST_FUN_OBJ_1(rp2_get_sys_info_obj, rp2_get_sys_info);
#endif

static const mp_rom_map_elem_t rp2_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_ROM_QSTR(MP_QSTR_rp2) },
    { MP_ROM_QSTR(MP_QSTR_bootsel_button),      MP_ROM_PTR(&rp2_bootsel_button_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_core_num),        MP_ROM_PTR(&rp2_get_core_num_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_do_cmd),        MP_ROM_PTR(&rp2_flash_do_cmd_obj) },
    #if !PICO_RP2040
    { MP_ROM_QSTR(MP_QSTR_get_sys_info),        MP_ROM_PTR(&rp2_get_sys_info_obj) },
    #endif
};
static MP_DEFINE_CONST_DICT(rp2_module_globals, rp2_module_globals_table);

const mp_obj_module_t mp_module_rp2 = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&rp2_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_rp2, mp_module_rp2);
