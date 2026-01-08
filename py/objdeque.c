// SPDX-FileCopyrightText: 2018 Paul Sokolovsky
// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <memory.h>

#include "py/runtime.h"

#if MICROPY_PY_COLLECTIONS_DEQUE

typedef struct _mp_obj_deque_t {
    mp_obj_base_t base;
    size_t alloc;
    mp_obj_t *items;
    size_t off;
    size_t len;
    int mutation;
} mp_obj_deque_t;

static inline mp_obj_t *mp_obj_deque_get(mp_obj_deque_t *self, size_t index) {
    index += self->off;
    if (index >= self->alloc) {
        index -= self->alloc;
    }
    return &self->items[index];
}

static void mp_obj_deque_grow(mp_obj_deque_t *self) {
    if (self->len + 1 > self->alloc) {
        size_t new_alloc = 3 * (self->alloc / 2);
        self->items = m_realloc(self->items, sizeof(mp_obj_t) * new_alloc);
        if (self->off + self->len > self->alloc) {
            size_t rem = self->alloc - self->off;
            memmove(&self->items[new_alloc - rem], &self->items[self->off], sizeof(mp_obj_t) * rem);
            self->off = new_alloc - rem;
        }
        self->alloc = new_alloc;
    }
    self->len++;
    self->mutation++;
}

static mp_int_t mp_obj_deque_contains(mp_obj_deque_t *self, mp_obj_t value, mp_int_t start, mp_int_t stop) {
    int mut = self->mutation;
    for (size_t i = start; i < stop; i++) {
        bool eq = mp_obj_equal(*mp_obj_deque_get(self, i), value);
        if (self->mutation != mut) {
            mp_raise_type(&mp_type_RuntimeError);
        }
        if (eq) {
            return i;
        }
    }
    return -1;
}

static mp_int_t mp_obj_deque_equals(mp_obj_deque_t *self, mp_obj_deque_t *other) {
    if (self == other) {
        return true;
    }
    if (self->len != other->len) {
        return false;
    }
    // int mut = self->mutation;
    for (size_t i = 0; i < self->len; i++) {
        bool eq = mp_obj_equal(*mp_obj_deque_get(self, i), *mp_obj_deque_get(other, i));
        // if (self->mutation != mut) {
        //     mp_raise_type(&mp_type_RuntimeError);
        // }
        if (!eq) {
            return false;
        }
    }
    return true;
}

static void mp_obj_deque_remove_at(mp_obj_deque_t *self, mp_int_t index) {
    for (size_t i = index; i > 0; i--) {
        *mp_obj_deque_get(self, i) = *mp_obj_deque_get(self, i - 1);
    }
    self->items[self->off++] = MP_OBJ_NULL;
    if (self->off >= self->alloc) {
        self->off -= self->alloc;
    }
    self->len--;
    self->mutation++;
}

static mp_obj_t mp_obj_deque_extend(mp_obj_t self_in, mp_obj_t iterable);

static mp_obj_t mp_obj_deque_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);

    mp_obj_deque_t *o = mp_obj_malloc(mp_obj_deque_t, type);
    o->alloc = 4;
    o->items = m_malloc(sizeof(mp_obj_t) * o->alloc);
    o->off = o->len = 0;

    if (n_args > 0) {
        mp_obj_deque_extend(MP_OBJ_FROM_PTR(o), args[0]);
    }
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t mp_obj_deque_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    switch (op) {
        case MP_UNARY_OP_BOOL:
            return mp_obj_new_bool(self->len);
        case MP_UNARY_OP_LEN:
            return MP_OBJ_NEW_SMALL_INT(self->len);

        #if MICROPY_PY_SYS_GETSIZEOF
        case MP_UNARY_OP_SIZEOF: {
            size_t sz = sizeof(*self) + sizeof(mp_obj_t) * self->alloc;
            return MP_OBJ_NEW_SMALL_INT(sz);
        }
        #endif
        default:
            return MP_OBJ_NULL; // op not supported
    }
}
static mp_obj_t mp_obj_deque_binary_op(mp_binary_op_t op, mp_obj_t self_in, mp_obj_t arg) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    switch (op) {
        case MP_BINARY_OP_EQUAL: {
            if (mp_obj_get_type(arg) != &mp_type_deque) {
                return false;
            }
            return mp_obj_new_bool(mp_obj_deque_equals(self, MP_OBJ_TO_PTR(arg)));
        }
        case MP_BINARY_OP_CONTAINS:
            return mp_obj_new_bool(mp_obj_deque_contains(self, arg, 0, self->len) >= 0);
        default:
            return MP_OBJ_NULL;
    }
}

static mp_obj_t mp_obj_deque_append(mp_obj_t self_in, mp_obj_t arg) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);

    mp_obj_deque_grow(self);
    *mp_obj_deque_get(self, self->len - 1) = arg;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_obj_deque_append_obj, mp_obj_deque_append);

static mp_obj_t mp_obj_deque_appendleft(mp_obj_t self_in, mp_obj_t arg) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);

    mp_obj_deque_grow(self);
    if (self->off == 0) {
        self->off += self->alloc;
    }
    self->items[--self->off] = arg;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_obj_deque_appendleft_obj, mp_obj_deque_appendleft);

static mp_obj_t mp_obj_deque_clear(mp_obj_t self_in) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    self->off = self->len = 0;
    memset(self->items, 0, sizeof(mp_obj_t) * self->alloc);
    self->mutation++;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_obj_deque_clear_obj, mp_obj_deque_clear);

static mp_obj_t mp_obj_deque_copy(mp_obj_t self_in) {
    return mp_obj_deque_make_new(&mp_type_deque, 1, 0, &self_in);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_obj_deque_copy_obj, mp_obj_deque_copy);

static mp_obj_t mp_obj_deque_count(mp_obj_t self_in, mp_obj_t value) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    mp_int_t count = 0;
    int mut = self->mutation;
    for (size_t i = 0; i < self->len; i++) {
        bool eq = mp_obj_equal(*mp_obj_deque_get(self, i), value);
        if (self->mutation != mut) {
            mp_raise_type(&mp_type_RuntimeError);
        }
        if (eq) {
            count++;
        }
    }
    return MP_OBJ_NEW_SMALL_INT(count);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_obj_deque_count_obj, mp_obj_deque_count);

static mp_obj_t mp_obj_deque_extend(mp_obj_t self_in, mp_obj_t iterable) {
    if (iterable == self_in) {
        iterable = mp_obj_deque_copy(self_in);
    }
    mp_obj_iter_buf_t iter_buf;
    mp_obj_t iter = mp_getiter(iterable, &iter_buf);
    mp_obj_t item;
    while ((item = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
        mp_obj_deque_append(self_in, item);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_obj_deque_extend_obj, mp_obj_deque_extend);

static mp_obj_t mp_obj_deque_extendleft(mp_obj_t self_in, mp_obj_t iterable) {
    if (iterable == self_in) {
        iterable = mp_obj_deque_copy(self_in);
    }
    mp_obj_iter_buf_t iter_buf;
    mp_obj_t iter = mp_getiter(iterable, &iter_buf);
    mp_obj_t item;
    while ((item = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
        mp_obj_deque_appendleft(self_in, item);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_obj_deque_extendleft_obj, mp_obj_deque_extendleft);

static mp_int_t mp_obj_deque_get_index(mp_obj_deque_t *self, mp_obj_t index_in) {
    mp_int_t index = mp_obj_get_int(index_in);
    if (index < 0) {
        index += self->len;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > self->len) {
        index = self->len;
    }
    return index;
}

static mp_obj_t mp_obj_deque_index(size_t n_args, const mp_obj_t *args) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t value = args[1];
    mp_int_t start = (n_args > 2) ? mp_obj_deque_get_index(self, args[2]) : 0;
    mp_int_t stop = (n_args > 3) ? mp_obj_deque_get_index(self, args[3]) : self->len;

    mp_int_t index = mp_obj_deque_contains(self, value, start, stop);
    if (index < 0) {
        mp_raise_ValueError(NULL);
    }
    return MP_OBJ_NEW_SMALL_INT(index);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_obj_deque_index_obj, 2, 4, mp_obj_deque_index);

static mp_obj_t mp_obj_deque_insert(mp_obj_t self_in, mp_obj_t index_in, mp_obj_t value) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    mp_int_t index = mp_obj_deque_get_index(self, index_in);

    mp_obj_deque_grow(self);
    for (size_t i = self->len - 1; i > index; i--) {
        *mp_obj_deque_get(self, i) = *mp_obj_deque_get(self, i - 1);
    }
    *mp_obj_deque_get(self, index) = value;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(mp_obj_deque_insert_obj, mp_obj_deque_insert);

static mp_obj_t mp_obj_deque_pop(mp_obj_t self_in) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->len == 0) {
        mp_raise_msg(&mp_type_IndexError, MP_ERROR_TEXT("empty"));
    }

    mp_obj_t *item = mp_obj_deque_get(self, --self->len);
    mp_obj_t ret = *item;
    *item = MP_OBJ_NULL;
    self->mutation++;
    return ret;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_obj_deque_pop_obj, mp_obj_deque_pop);

static mp_obj_t mp_obj_deque_popleft(mp_obj_t self_in) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->len == 0) {
        mp_raise_msg(&mp_type_IndexError, MP_ERROR_TEXT("empty"));
    }

    mp_obj_t ret = self->items[self->off];
    mp_obj_deque_remove_at(self, 0);
    return ret;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_obj_deque_popleft_obj, mp_obj_deque_popleft);

static mp_obj_t mp_obj_deque_remove(mp_obj_t self_in, mp_obj_t value) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);

    mp_int_t index = mp_obj_deque_contains(self, value, 0, self->len);
    if (index < 0) {
        mp_raise_ValueError(NULL);
    }
    // if (index >= self->len) {
    //     mp_raise_type(&mp_type_IndexError);
    // }
    mp_obj_deque_remove_at(self, index);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_obj_deque_remove_obj, mp_obj_deque_remove);

static void mp_obj_deque_reverse_range(mp_obj_deque_t *self, size_t start, size_t stop) {
    for (size_t i = 0; i < (stop - start) / 2; i++) {
        mp_obj_t tmp = *mp_obj_deque_get(self, start + i);
        *mp_obj_deque_get(self, start + i) = *mp_obj_deque_get(self, stop - 1 - i);
        *mp_obj_deque_get(self, stop - 1 - i) = tmp;
    }
}

static mp_obj_t mp_obj_deque_reverse(mp_obj_t self_in) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_deque_reverse_range(self, 0, self->len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_obj_deque_reverse_obj, mp_obj_deque_reverse);

static mp_obj_t mp_obj_deque_rotate(size_t n_args, const mp_obj_t *args) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_int_t n = (n_args > 1) ? mp_obj_get_int(args[1]) : 1;
    mp_int_t len = self->len;
    if (len) {
        n = (n % len + len) % len;
        mp_obj_deque_reverse_range(self, 0, len);
        mp_obj_deque_reverse_range(self, 0, n);
        mp_obj_deque_reverse_range(self, n, len);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_obj_deque_rotate_obj, 1, 2, mp_obj_deque_rotate);

static mp_obj_t mp_obj_deque_subscr(mp_obj_t self_in, mp_obj_t index_in, mp_obj_t value) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    mp_int_t index = mp_obj_get_int(index_in);
    if (index < 0) {
        index += self->len;
    }
    if ((index < 0) || (index >= self->len)) {
        mp_raise_type(&mp_type_IndexError);
    }

    mp_obj_t *item = mp_obj_deque_get(self, index);
    if (value == MP_OBJ_SENTINEL) {
        // load
        return *item;
    } else if (value != MP_OBJ_NULL) {
        // store into deque
        *item = value;
        return mp_const_none;
    } else {
        mp_obj_deque_remove_at(self, index);
        return mp_const_none;
    }
}

/******************************************************************************/
/* deque iterator                                                             */

typedef struct _mp_obj_deque_it_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    mp_obj_deque_t *deque;
    size_t cur;
    int mutation;
} mp_obj_deque_it_t;

static mp_obj_t mp_obj_deque_it_iternext(mp_obj_t self_in) {
    mp_obj_deque_it_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->mutation != self->deque->mutation) {
        mp_raise_type(&mp_type_RuntimeError);
    }
    if (self->cur < self->deque->len) {
        return *mp_obj_deque_get(self->deque, self->cur++);
    } else {
        return MP_OBJ_STOP_ITERATION;
    }
}

static mp_obj_t mp_obj_deque_iter(mp_obj_t self_in, mp_obj_iter_buf_t *iter_buf) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_deque_it_t *it = mp_obj_malloc(mp_obj_deque_it_t, &mp_type_polymorph_iter);
    it->iternext = mp_obj_deque_it_iternext;
    it->deque = self;
    it->cur = 0;
    it->mutation = self->mutation;
    return MP_OBJ_FROM_PTR(it);
}

static void mp_obj_deque_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_obj_deque_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "%q([", self->base.type->name);
    for (size_t i = 0; i < self->len; i++) {
        if (i > 0) {
            mp_print_str(print, ", ");
        }
        mp_obj_t item = *mp_obj_deque_get(self, i);
        mp_obj_print_helper(print, item, kind);
    }
    mp_print_str(print, "])");
}

static const mp_rom_map_elem_t mp_obj_deque_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_append),          MP_ROM_PTR(&mp_obj_deque_append_obj) },
    { MP_ROM_QSTR(MP_QSTR_appendleft),      MP_ROM_PTR(&mp_obj_deque_appendleft_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear),           MP_ROM_PTR(&mp_obj_deque_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_copy),            MP_ROM_PTR(&mp_obj_deque_copy_obj) },
    { MP_ROM_QSTR(MP_QSTR_count),           MP_ROM_PTR(&mp_obj_deque_count_obj) },
    { MP_ROM_QSTR(MP_QSTR_extend),          MP_ROM_PTR(&mp_obj_deque_extend_obj) },
    { MP_ROM_QSTR(MP_QSTR_extendleft),      MP_ROM_PTR(&mp_obj_deque_extendleft_obj) },
    { MP_ROM_QSTR(MP_QSTR_index),           MP_ROM_PTR(&mp_obj_deque_index_obj) },
    { MP_ROM_QSTR(MP_QSTR_insert),          MP_ROM_PTR(&mp_obj_deque_insert_obj) },
    { MP_ROM_QSTR(MP_QSTR_pop),             MP_ROM_PTR(&mp_obj_deque_pop_obj) },
    { MP_ROM_QSTR(MP_QSTR_popleft),         MP_ROM_PTR(&mp_obj_deque_popleft_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove),          MP_ROM_PTR(&mp_obj_deque_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_reverse),         MP_ROM_PTR(&mp_obj_deque_reverse_obj) },
    { MP_ROM_QSTR(MP_QSTR_rotate),          MP_ROM_PTR(&mp_obj_deque_rotate_obj) },
};
static MP_DEFINE_CONST_DICT(mp_obj_deque_locals_dict, mp_obj_deque_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_deque,
    MP_QSTR_deque,
    MP_TYPE_FLAG_ITER_IS_GETITER,
    make_new, mp_obj_deque_make_new,
    unary_op, mp_obj_deque_unary_op,
    binary_op, mp_obj_deque_binary_op,
    subscr, mp_obj_deque_subscr,
    iter, mp_obj_deque_iter,
    locals_dict, &mp_obj_deque_locals_dict,
    print, mp_obj_deque_print
    );

#endif // MICROPY_PY_COLLECTIONS_DEQUE
