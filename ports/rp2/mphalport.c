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

#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

#include "py/runtime.h"
#include "py/mphal.h"
#include "shared/timeutils/timeutils.h"
#include "pico/time.h"
#include "pico/unique_id.h"
#include "pico/aon_timer.h"

#if MICROPY_PY_NETWORK_CYW43
#include "cyw43.h"
#endif

uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags) {
    panic("mp_hal_stdio_poll not implemented");
}

// Receive single character
int mp_hal_stdin_rx_chr(void) {
    int ch;
    do {
        MP_THREAD_GIL_EXIT();
        ch = getchar();
        MP_THREAD_GIL_ENTER();
        mp_handle_pending(false);
    }
    while ((ch == -1) && (errno == EINTR));

    return ch;
}

// Send string of given length
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    size_t size = fwrite(str, sizeof(char), len, stdout);
    fflush(stdout);
    return size;
}

#if PICO_RISCV
__attribute__((naked)) mp_uint_t mp_hal_ticks_cpu(void) {
    __asm volatile (
        "li a0, 4\n" // mask value to uninhibit mcycle counter
        "csrw mcountinhibit, a0\n" // uninhibit mcycle counter
        "csrr a0, mcycle\n" // get mcycle counter
        "ret\n"
        );
}
#endif

void mp_hal_delay_us(mp_uint_t us) {
    // Avoid calling sleep_us() and invoking the alarm pool by splitting long
    // sleeps into an optional longer sleep and a shorter busy-wait
    uint64_t end = time_us_64() + us;
    if (us > 1000) {
        mp_hal_delay_ms(us / 1000);
    }
    while (time_us_64() < end) {
        // Tight loop busy-wait for accurate timing
    }
}

void mp_hal_delay_ms(mp_uint_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void mp_hal_time_ns_set_from_rtc(void) {
}

uint64_t mp_hal_time_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000000 + t.tv_nsec;
}

// Generate a random locally administered MAC address (LAA)
void mp_hal_generate_laa_mac(int idx, uint8_t buf[6]) {
    #ifndef NDEBUG
    printf("Warning: No MAC in OTP, generating MAC from board id\n");
    #endif
    pico_unique_board_id_t pid;
    pico_get_unique_board_id(&pid);
    buf[0] = 0x02; // LAA range
    buf[1] = (pid.id[7] << 4) | (pid.id[6] & 0xf);
    buf[2] = (pid.id[5] << 4) | (pid.id[4] & 0xf);
    buf[3] = (pid.id[3] << 4) | (pid.id[2] & 0xf);
    buf[4] = pid.id[1];
    buf[5] = (pid.id[0] << 2) | idx;
}

// A board can override this if needed
MP_WEAK void mp_hal_get_mac(int idx, uint8_t buf[6]) {
    #if MICROPY_PY_NETWORK_CYW43
    // The mac should come from cyw43 otp when CYW43_USE_OTP_MAC is defined
    // This is loaded into the state after the driver is initialised
    // cyw43_hal_generate_laa_mac is only called by the driver to generate a mac if otp is not set
    if (idx == MP_HAL_MAC_WLAN0) {
        memcpy(buf, cyw43_state.mac, 6);
        return;
    }
    #endif
    mp_hal_generate_laa_mac(idx, buf);
}

void mp_hal_get_mac_ascii(int idx, size_t chr_off, size_t chr_len, char *dest) {
    static const char hexchr[16] = "0123456789ABCDEF";
    uint8_t mac[6];
    mp_hal_get_mac(idx, mac);
    for (; chr_len; ++chr_off, --chr_len) {
        *dest++ = hexchr[mac[chr_off >> 1] >> (4 * (1 - (chr_off & 1))) & 0xf];
    }
}
