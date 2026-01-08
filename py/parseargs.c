// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdarg.h>
#include <string.h>

#include "py/extras.h"
#include "py/parseargs.h"
#include "py/runtime.h"


static const char *parse_arg(const mp_obj_t arg, const char *format, qstr name, va_list *vals) {
    switch (*format) {
        case '(': {
            format++;
            if (arg != MP_OBJ_NULL) {
                mp_obj_t *items;
                size_t len = mp_obj_tuple_get_checked(arg, &items);
                for (size_t i = 0; i < len; i++) {
                    format = parse_arg(items[i], format, MP_QSTR_, vals);
                }
            } else {
                while (*format != ')') {
                    format = parse_arg(MP_OBJ_NULL, format, MP_QSTR_, vals);
                }
            }
            if (*format == ')') {
                format++;
                break;
            }
        }
        case ')': {
            mp_raise_TypeError(NULL);
            break;
        }
        case 's':
        case 'w':
        case 'y':
        case 'z': {
            char code = *format;
            format++;
            if ((code == 'y') && (arg != MP_OBJ_NULL) && mp_obj_is_str(arg)) {
                mp_raise_TypeError(MP_ERROR_TEXT("expected bytes-like object, not str"));
            }
            if (*format == '*') {
                format++;
                mp_buffer_info_t *val = va_arg(*vals, mp_buffer_info_t *);

                if (arg != MP_OBJ_NULL) {
                    mp_buffer_info_t bufinfo;
                    mp_get_buffer_raise(arg, &bufinfo, (code == 'w') ? MP_BUFFER_RW : MP_BUFFER_READ);
                    if (val) {
                        *val = bufinfo;
                    }
                }
            } else {
                assert(code != 'w');
                const char **val = va_arg(*vals, const char **);
                size_t *len = NULL;
                if (*format == '#') {
                    format++;
                    len = va_arg(*vals, size_t *);
                }

                if (arg != MP_OBJ_NULL) {
                    const char *s = NULL;
                    size_t n = 0;
                    if ((code != 'z') || (arg != mp_const_none)) {
                        s = mp_obj_str_get_data(arg, &n);
                    }
                    if (val) {
                        *val = s;
                    }
                    if (len) {
                        *len = n;
                    } else if (s && strlen(s) != n) {
                        mp_raise_ValueError(MP_ERROR_TEXT("embedded null byte"));
                    }
                }
            }
            break;
        }
        case 'q': {
            format++;
            qstr *val = va_arg(*vals, qstr *);

            if (arg != MP_OBJ_NULL) {
                if (!mp_obj_is_qstr(arg)) {
                    mp_raise_TypeError(MP_ERROR_TEXT("expected qstr"));
                }
                if (val) {
                    *val = MP_OBJ_QSTR_VALUE(arg);
                }
            }
            break;
        }
        case 'i': {
            format++;
            mp_int_t *val = va_arg(*vals, mp_int_t *);

            if (arg != MP_OBJ_NULL) {
                mp_int_t i = mp_obj_get_int(arg);
                if (val) {
                    *val = i;
                }
            }
            break;
        }
        case 'p': {
            format++;
            mp_int_t *val = va_arg(*vals, mp_int_t *);

            if (arg != MP_OBJ_NULL) {
                mp_int_t i = mp_obj_is_true(arg);
                if (val) {
                    *val = i;
                }
            }
            break;
        }
        case 'f': {
            format++;
            mp_float_t *val = va_arg(*vals, mp_float_t *);

            if (arg != MP_OBJ_NULL) {
                mp_float_t f = mp_obj_get_float(arg);
                if (val) {
                    *val = f;
                }
            }
            break;
        }
        case 'O': {
            format++;
            if (*format == '!') {
                format++;
                const mp_obj_type_t *expected_type = va_arg(*vals, const mp_obj_type_t *);
                mp_obj_t *val = va_arg(*vals, mp_obj_t *);

                if (arg != MP_OBJ_NULL) {
                    const mp_obj_type_t *actual_type = mp_obj_get_type(arg);
                    if (!mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(actual_type), MP_OBJ_FROM_PTR(expected_type))) {
                        mp_raise_msg_varg(&mp_type_TypeError, "%q: must be %q, not %q", name, actual_type->name, expected_type->name);
                    }
                    if (val) {
                        *val = arg;
                    }
                }
            } else if (*format == '&') {
                format++;
                void *(*converter)(mp_obj_t) = va_arg(*vals, void *(*)(mp_obj_t));
                void **val = va_arg(*vals, void **);

                if (arg != MP_OBJ_NULL) {
                    if (val) {
                        *val = converter(arg);
                    }
                }
            } else {
                mp_obj_t *val = va_arg(*vals, mp_obj_t *);
                if (val && arg != MP_OBJ_NULL) {
                    *val = arg;
                }
            }
            break;
        }
        default: {
            format++;
            assert(0); // invalid format specifier
        }
    }
    return format;
}

void vparse_args_and_kw(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args, const char *format, const qstr keywords[], va_list vals) {
    bool required = true;
    bool pos_allowed = true;
    size_t pos = 0;
    uint32_t used_kws = 0;

    while (*format) {
        qstr name = *keywords;
        if (*format == '|') {
            format++;
            required = false;
            continue;
        }
        if (*format == '$') {
            format++;
            required = false;
            pos_allowed = false;
            continue;
        }

        bool found = false;
        if (pos_allowed && pos < n_args) {
            format = parse_arg(args[pos++], format, name, &vals);
            found = true;
        }

        bool kw_allowed = name > MP_QSTR_;
        if (kw_allowed) {
            mp_map_elem_t *elem = mp_map_lookup(kw_args, MP_OBJ_NEW_QSTR(name), MP_MAP_LOOKUP);
            if (elem) {
                format = parse_arg(elem->value, format, name, &vals);
                used_kws |= 1 << (elem - kw_args->table);
                found = true;
            }
        }

        if (!found) {
            if (required) {
                mp_raise_msg_varg(&mp_type_TypeError, "%q: missing", name);
            } else {
                format = parse_arg(MP_OBJ_NULL, format, name, &vals);
            }
        }
        keywords += (name > 0) ? 1 : 0;
    }
    if (pos < n_args) {
        mp_raise_msg_varg(&mp_type_TypeError, "function: too many args");
    }

    if ((used_kws + 1) != (1 << kw_args->used)) {
        mp_raise_msg_varg(&mp_type_TypeError, "function: too many keywords");
    }
}

void parse_args_and_kw_map(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args, const char *format, const qstr keywords[], ...) {
    va_list vals;
    va_start(vals, keywords);
    mp_map_t empty;
    mp_map_init(&empty, 0);
    vparse_args_and_kw(n_args, args, kw_args ? kw_args : &empty, format, keywords, vals);
    va_end(vals);
}

void parse_args_and_kw(size_t n_args, size_t n_kw, const mp_obj_t *args, const char *format, const qstr keywords[], ...) {
    va_list vals;
    va_start(vals, keywords);
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    vparse_args_and_kw(n_args, args, &kw_args, format, keywords, vals);
    va_end(vals);
}

void parse_args(size_t n_args, const mp_obj_t *args, const char *format, ...) {
    va_list vals;
    va_start(vals, format);
    const qstr keywords[] = { 0 };
    vparse_args_and_kw(n_args, args, NULL, format, keywords, vals);
    va_end(vals);
}
