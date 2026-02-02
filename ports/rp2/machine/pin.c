// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "morelib/gpio.h"
#include "rp2/gpio.h"

#include "extmod/modos_newlib.h"
#include "./pin.h"
#include "py/parseargs.h"
#include "py/runtime.h"


enum gpio_irq_pulse {
    GPIO_IRQ_PULSE_DOWN = 0x10u,
    GPIO_IRQ_PULSE_UP = 0x20u,
};

static bool pin_inited(pin_obj_t *self) {
    return self->pin != -1u;
}

static pin_obj_t *pin_get_raise(mp_obj_t self_in) {
    pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!pin_inited(self)) {
        mp_raise_OSError(EBADF);
    }
    return self;
}
static void pin_reconfigure(pin_obj_t *self, struct gpio_v2_line_values *values) {
    struct gpio_v2_line_config config = { .flags = self->flags };
    if (values) {
        size_t i = config.num_attrs++;
        config.attrs[i].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
        config.attrs[i].attr.values = values->bits;
        config.attrs[i].mask = values->mask;
    }
    if (self->debounce_time_us) {
        size_t i = config.num_attrs++;
        config.attrs[i].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
        config.attrs[i].attr.debounce_period_us = self->debounce_time_us;
        config.attrs[i].mask = 1;
    }
    int ret = ioctl(self->fd, GPIO_V2_LINE_SET_CONFIG_IOCTL, &config);
    mp_os_check_ret(ret);
}

static mp_obj_t pin_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_pin, 0 };
    mp_hal_pin_obj_t pin;
    parse_args_and_kw(n_args, n_kw, args, "O&", kws, mp_hal_get_pin_obj, &pin);

    int fd = open("/dev/gpiochip0", O_RDWR);
    mp_os_check_ret(fd);

    struct gpio_v2_line_request req = { 0 };
    req.offsets[0] = pin;
    req.config.flags = GPIO_V2_LINE_FLAG_INPUT;
    req.num_lines = 1;
    int ret = ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req);
    close(fd);
    mp_os_check_ret(ret);

    pin_obj_t *self = mp_obj_malloc_with_finaliser(pin_obj_t, type);
    self->pin = pin;
    self->fd = req.fd;
    self->flags = req.config.flags;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t pin_del(mp_obj_t self_in) {
    pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->fd >= 0) {
        close(self->fd);
        self->fd = -1;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(pin_del_obj, pin_del);

static mp_obj_t pin_fileno(mp_obj_t self_in) {
    pin_obj_t *self = pin_get_raise(self_in);
    return MP_OBJ_NEW_SMALL_INT(self->fd);
}
static MP_DEFINE_CONST_FUN_OBJ_1(pin_fileno_obj, pin_fileno);

static mp_obj_t pin_set_pulls(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_pull_up, MP_QSTR_pull_down, 0 };
    mp_obj_t self_in;
    mp_int_t pull_up = 0, pull_down = 0;
    parse_args_and_kw(n_args, 0, args, "O|pp", kws, &self_in, &pull_up, &pull_down);

    pin_obj_t *self = pin_get_raise(self_in);
    self->flags &= ~(GPIO_V2_LINE_FLAG_BIAS_PULL_UP | GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN);
    if (pull_up) {
        self->flags |= GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
    }
    if (pull_down) {
        self->flags |= GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN;
    }
    pin_reconfigure(self, NULL);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pin_set_pulls_obj, 1, 3, pin_set_pulls);

static mp_obj_t pin_set_edges(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_rising, MP_QSTR_falling, 0 };
    mp_obj_t self_in;
    mp_int_t rising = 0, falling = 0;
    parse_args_and_kw(n_args, 0, args, "O|pp", kws, &self_in, &rising, &falling);

    pin_obj_t *self = pin_get_raise(self_in);
    self->flags &= ~(GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING);
    if (rising) {
        self->flags |= GPIO_V2_LINE_FLAG_EDGE_RISING;
    }
    if (falling) {
        self->flags |= GPIO_V2_LINE_FLAG_EDGE_FALLING;
    }
    pin_reconfigure(self, NULL);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pin_set_edges_obj, 1, 3, pin_set_edges);

static mp_obj_t pin_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    pin_obj_t *self = pin_get_raise(self_in);
    if (op == MP_UNARY_OP_INT_MAYBE) {
        return MP_OBJ_NEW_SMALL_INT(self->pin);
    }
    return mp_const_none;
}

static void pin_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    pin_obj_t *self = pin_get_raise(self_in);
    switch (attr) {
        case MP_QSTR_value: {
            if (dest[0] == MP_OBJ_SENTINEL) {
                // store attr
                if (dest[1] == mp_const_none) {
                    if (!(self->flags & GPIO_V2_LINE_FLAG_INPUT)) {
                        self->flags &= ~GPIO_V2_LINE_FLAG_OUTPUT;
                        self->flags |= GPIO_V2_LINE_FLAG_INPUT;
                        pin_reconfigure(self, NULL);
                    }
                    dest[0] = MP_OBJ_NULL;
                } else if (dest[1] != MP_OBJ_NULL) {
                    bool value = mp_obj_is_true(dest[1]);
                    struct gpio_v2_line_values values = { .bits = value ? 1 : 0, .mask = 1 };
                    if (!(self->flags & GPIO_V2_LINE_FLAG_OUTPUT)) {
                        self->flags &= ~GPIO_V2_LINE_FLAG_INPUT;
                        self->flags |= GPIO_V2_LINE_FLAG_OUTPUT;
                        pin_reconfigure(self, &values);
                    } else {
                        int ret = ioctl(self->fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
                        mp_os_check_ret(ret);
                    }
                    dest[0] = MP_OBJ_NULL;
                }
            } else {
                // load attr
                struct gpio_v2_line_values values = { .mask = 1 };
                int ret = ioctl(self->fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values);
                mp_os_check_ret(ret);
                dest[0] = mp_obj_new_bool(values.bits & 1);
            }
            break;
        }
        case MP_QSTR_debounce_time_us: {
            if (dest[0] == MP_OBJ_SENTINEL) {
                if (dest[1] != MP_OBJ_NULL) {
                    // store attr
                    self->debounce_time_us = mp_obj_get_int(dest[1]);
                    pin_reconfigure(self, NULL);
                    dest[0] = MP_OBJ_NULL;
                }
            } else {
                // load attr
                dest[0] = mp_obj_new_int(self->debounce_time_us);
            }
            break;
        }
        default:
            dest[1] = MP_OBJ_SENTINEL;
    }
}

static mp_obj_t pin_wait(mp_obj_t self_in) {
    pin_obj_t *self = pin_get_raise(self_in);

    struct gpio_v2_line_event event = { 0 };
    vstr_t vstr;
    vstr_init_fixed_buf(&vstr, sizeof(event), (char *)&event);
    int ret = mp_os_read_vstr(self->fd, &vstr, sizeof(event));
    mp_os_check_ret(ret);

    return mp_obj_new_int_from_ull(event.timestamp_ns);
}
static MP_DEFINE_CONST_FUN_OBJ_1(pin_wait_obj, pin_wait);

#ifndef NDEBUG
static mp_obj_t pin_debug(mp_obj_t self_in) {
    pin_obj_t *self = pin_get_raise(self_in);
    rp2_gpio_debug(self->pin);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(pin_debug_obj, pin_debug);
#endif

static const mp_rom_map_elem_t pin_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&pin_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pulls),       MP_ROM_PTR(&pin_set_pulls_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_edges),       MP_ROM_PTR(&pin_set_edges_obj) },

    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&pin_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno),          MP_ROM_PTR(&pin_fileno_obj) },

    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_debug),           MP_ROM_PTR(&pin_debug_obj) },
    #endif

    { MP_ROM_QSTR(MP_QSTR_wait),            MP_ROM_PTR(&pin_wait_obj) },
    { MP_ROM_QSTR(MP_QSTR_EDGE_FALL),       MP_ROM_INT(GPIO_IRQ_EDGE_FALL) },
    { MP_ROM_QSTR(MP_QSTR_EDGE_RISE),       MP_ROM_INT(GPIO_IRQ_EDGE_RISE) },
    { MP_ROM_QSTR(MP_QSTR_PULSE_DOWN),      MP_ROM_INT(GPIO_IRQ_PULSE_DOWN) },
    { MP_ROM_QSTR(MP_QSTR_PULSE_UP),        MP_ROM_INT(GPIO_IRQ_PULSE_UP) },
};
static MP_DEFINE_CONST_DICT(pin_locals_dict, pin_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    pin_type,
    MP_QSTR_Pin,
    MP_TYPE_FLAG_NONE,
    make_new, pin_make_new,
    unary_op, pin_unary_op,
    attr, pin_attr,
    locals_dict, &pin_locals_dict
    );
