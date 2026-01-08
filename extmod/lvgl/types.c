// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./anim.h"
#include "./color.h"
#include "./draw/buffer.h"
#include "./font.h"
#include "./misc.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./style.h"
#include "./super.h"
#include "./types.h"


static const lvgl_ptr_type_t *lvgl_type_is_ptr(lv_type_code_t type_code) {
    switch (type_code) {
        case LV_TYPE_ANIM: {
            return &lvgl_anim_type;
        }
        case LV_TYPE_DRAW_BUFFER: {
            return &lvgl_draw_buf_type;
        }
        case LV_TYPE_GRAD_DSC: {
            return &lvgl_grad_dsc_type;
        }        
        case LV_TYPE_STYLE_TRANSITION_DSC: {
            return &lvgl_style_transition_dsc_type;
        }
        default: {
            return NULL;
        }
    }
}

static const lvgl_static_ptr_type_t *lvgl_type_is_static_ptr(lv_type_code_t type_code) {
    switch (type_code) {
        case LV_TYPE_ANIM_PATH:
            return &lvgl_anim_path_type;
        case LV_TYPE_COLOR_FILTER:
            return &lvgl_color_filter_type;
        case LV_TYPE_FONT:
            return &lvgl_font_type;
        default:
            return NULL;
    }
}

size_t lvgl_type_sizeof(lv_type_code_t type_code) {
    switch (type_code) {
        case LV_TYPE_INT8:
            return sizeof(int8_t);
        case LV_TYPE_INT16:
            return sizeof(int16_t);
        case LV_TYPE_INT32:
            return sizeof(int32_t);
        case LV_TYPE_COLOR:
            return sizeof(lv_color_t);
        case LV_TYPE_STR:
            return sizeof(char *);
        case LV_TYPE_AREA:
            return sizeof(lv_area_t);            
        case LV_TYPE_POINT:
            return sizeof(lv_point_t);
        case LV_TYPE_POINT_PRECISE:
            return sizeof(lv_point_precise_t);
        default:
            return sizeof(void *);
    }
}

static void lvgl_type_free_ptr(const lvgl_ptr_type_t *ptr_type, void **plv_ptr) {
        lvgl_ptr_handle_t *handle = lvgl_ptr_from_lv(ptr_type, *plv_ptr);
        lvgl_ptr_delete(handle);
        *plv_ptr = NULL;
}

void lvgl_type_free(lv_type_code_t type_code, void *value) {
    switch (type_code) {
        case LV_TYPE_STR: {
            char **pvalue = value;
            lv_free(*pvalue);
            *pvalue = NULL;
            return;
        }
        case LV_TYPE_IMAGE_SRC: {
            lv_image_src_t src_type = lv_image_src_get_type(*(const void **)value);
            if ((src_type == LV_IMAGE_SRC_FILE) || (src_type == LV_IMAGE_SRC_SYMBOL)) {
                lvgl_type_free(LV_TYPE_STR, value);
            }
            else if (src_type == LV_IMAGE_SRC_VARIABLE) {
                lvgl_type_free(LV_TYPE_DRAW_BUFFER, value);
            }
            else {
                void **pvalue = value;
                *pvalue = NULL;
            }
            return;
        }
        case LV_TYPE_OBJ_HANDLE: {
            lvgl_obj_handle_t **pvalue = value;
            lvgl_ptr_delete(&(*pvalue)->base);
            *pvalue = NULL;
            return;
        }
        case LV_TYPE_PROP_LIST: {
            lv_style_prop_t **pprops = value;
            free(*pprops);
            *pprops = NULL;
            return;
        }
        case LV_TYPE_GC_HANDLE: {
            gc_handle_t **pgc_handle = value;
            if (*pgc_handle) {
                gc_handle_free(*pgc_handle);
            }
            *pgc_handle = NULL;
        }
        default: {
            break;
        }
    }
    const lvgl_ptr_type_t *ptr_type = lvgl_type_is_ptr(type_code);
    if (ptr_type) {
        lvgl_type_free_ptr(ptr_type, value);
        return;
    }
}

static void lvgl_type_from_mp_ptr(const lvgl_ptr_type_t *ptr_type, mp_obj_t obj, void **plv_ptr) {
    lvgl_ptr_handle_t *handle = (obj != mp_const_none) ? lvgl_ptr_from_mp(ptr_type, obj) : NULL;
    *plv_ptr = lvgl_ptr_to_lv(handle);
    lvgl_ptr_copy(handle);
}

void lvgl_type_from_mp_prop_list(mp_obj_t obj, lv_style_prop_t **pprops) {
    if (obj == mp_const_none) {
        *pprops = NULL;
        return;
    }
        
    size_t props_len;
    mp_obj_t *props_items;
    if (mp_obj_is_type(obj, MP_OBJ_FROM_PTR(&mp_type_list))) {
        mp_obj_list_get(obj, &props_len, &props_items);
    }
    else if (mp_obj_is_type(obj, MP_OBJ_FROM_PTR(&mp_type_tuple))) {
        mp_obj_tuple_get(obj, &props_len, &props_items);
    }
    else {
        mp_raise_TypeError(NULL);
    }

    lv_style_prop_t *props = malloc(sizeof(lv_style_prop_t) * (props_len + 1));
    for (size_t i = 0; i < props_len; i++) {
        const char* prop_str = mp_obj_str_get_str(props_items[i]);
        qstr prop_qstr = qstr_find_strn(prop_str, strlen(prop_str));
        lv_type_code_t type_code;
        lv_style_prop_t prop = lvgl_style_lookup(prop_qstr, &type_code);
        if (!prop) {
            free(props);
            mp_raise_msg_varg(&mp_type_AttributeError, MP_ERROR_TEXT("no style attribute '%s'"), prop_str);
        }
        props[i] = prop;
    }
    props[props_len] = 0;

    *pprops = props;
}

void lvgl_type_from_mp_array(lv_type_code_t type_code, mp_obj_t obj, size_t *array_size, void **parray) {
    if (obj == mp_const_none) {
        *parray = NULL;
        return;
    }

    size_t len;
    mp_obj_t *items;
    if (mp_obj_is_type(obj, MP_OBJ_FROM_PTR(&mp_type_list))) {
        mp_obj_list_get(obj, &len, &items);
    }
    else if (mp_obj_is_type(obj, MP_OBJ_FROM_PTR(&mp_type_tuple))) {
        mp_obj_tuple_get(obj, &len, &items);
    }
    else {
        mp_raise_TypeError(NULL);
    }

    size_t elem_size = lvgl_type_sizeof(type_code);
    void *gc_array = m_malloc(len * elem_size);
    for (size_t i = 0; i < len; i++) {
        lvgl_type_from_mp(type_code, items[i], gc_array + i * elem_size);
    }
    
    void *array = lv_malloc(len * elem_size);
    lv_memcpy(array, gc_array, len * elem_size);
    *array_size = len;
    *parray = array;
}

void lvgl_type_from_mp(lv_type_code_t type_code, mp_obj_t obj, void *value) {
    switch (type_code) {
        case LV_TYPE_INT8: {
            *(int8_t *)value = mp_obj_get_int(obj);
            return;
        }
        case LV_TYPE_INT16: {
            *(int16_t *)value = mp_obj_get_int(obj);
            return;
        }                
        case LV_TYPE_INT32: {
            *(int32_t *)value = mp_obj_get_int(obj);
            return;
        }
        case LV_TYPE_COLOR: {
            *(lv_color_t *)value = lv_color_hex(mp_obj_get_int(obj));
            return;
        }        
        case LV_TYPE_STR: {
            if (obj == mp_const_none) {
                *(char **)value = NULL;
            }
            else {
                const char *str = mp_obj_str_get_str(obj);
                *(char **)value = lv_strdup(str);
            }
            return;
        }
        case LV_TYPE_AREA: {
            lvgl_area_from_mp(obj, value);
            return;
        }        
        case LV_TYPE_POINT: {
            lvgl_point_from_mp(obj, value);
            return;
        }
        case LV_TYPE_POINT_PRECISE: {
            lvgl_point_precise_from_mp(obj, value);
            return;
        }
        case LV_TYPE_IMAGE_SRC: {
            if (obj == mp_const_none) {
                *(void **)value = NULL;
            }
            else if (mp_obj_is_str(obj)) {
                const char *str = mp_obj_str_get_str(obj);
                lv_image_src_t src_type = lv_image_src_get_type(str);
                if (src_type == LV_IMAGE_SRC_FILE) {
                    size_t len = lv_strlen(str);
                    char **pvalue = value;                    
                    *pvalue = lv_malloc(len + 3);
                    (*pvalue)[0] = LV_FS_POSIX_LETTER;
                    (*pvalue)[1] = ':';
                    lv_strcpy(*pvalue + 2, str);
                }
                else if (src_type == LV_IMAGE_SRC_SYMBOL) {
                    lvgl_type_from_mp(LV_TYPE_STR, obj, value);
                }
                else {
                    mp_raise_ValueError(NULL);
                }
            }
            else {
                lvgl_type_from_mp(LV_TYPE_DRAW_BUFFER, obj, value);
            }
            return;
        }        
        case LV_TYPE_OBJ_HANDLE: {
            lvgl_obj_handle_t *handle = lvgl_obj_from_mp_checked(obj);
            *(lvgl_obj_handle_t **)value = lvgl_obj_copy(handle);
            return;
        }
        case LV_TYPE_PROP_LIST: {
            lvgl_type_from_mp_prop_list(obj, value);
            return;
        }
        case LV_TYPE_GC_HANDLE: {
            if (obj == mp_const_none) {
                *(gc_handle_t **)value = NULL;
            }            
            else if (!mp_obj_is_obj(obj)) {
                mp_raise_TypeError(NULL);
            }
            else {
                *(gc_handle_t **)value = gc_handle_alloc(MP_OBJ_TO_PTR(obj));
            }
            return;
        }
        default: {
            break;
        }
    }
    const lvgl_ptr_type_t *ptr_type = lvgl_type_is_ptr(type_code);
    if (ptr_type) {
        lvgl_type_from_mp_ptr(ptr_type, obj, value);
        return;
    }    
    const lvgl_static_ptr_type_t *static_ptr_type = lvgl_type_is_static_ptr(type_code);
    if (static_ptr_type) {
        *(const void **)value = lvgl_static_ptr_from_mp(static_ptr_type, obj);
        return;
    }
    assert(0);
}

static mp_obj_t lvgl_type_to_mp_ptr(const lvgl_ptr_type_t *ptr_type, const void *lv_ptr) {
    lvgl_ptr_handle_t *handle = lvgl_ptr_from_lv(ptr_type, lv_ptr);
    return lvgl_ptr_to_mp(handle);
}

static mp_obj_t lvgl_type_to_mp_prop_list(const lv_style_prop_t *props) {
    if (!props) {
        return mp_const_none;
    }
    mp_obj_t obj = mp_obj_new_list(0, NULL);
    while (*props) {
        assert(0);
        props++;
    }
    return obj;
}

mp_obj_t lvgl_type_to_mp_array(lv_type_code_t type_code, size_t array_size, const void *array) {
    size_t elem_size = lvgl_type_sizeof(type_code);
    mp_obj_t list = mp_obj_new_list(0, NULL);
    for (size_t i = 0; i < array_size; i++) {
        mp_obj_t elem = lvgl_type_to_mp(type_code, array + i * elem_size);
        mp_obj_list_append(list, elem);
    }
    return list;
}

mp_obj_t lvgl_type_to_mp(lv_type_code_t type_code, const void *value) {
    switch (type_code) {
        case LV_TYPE_NONE: {
            return mp_const_none;
        }
        case LV_TYPE_INT8: {
            return mp_obj_new_int(*(int8_t *)value);
        }
        case LV_TYPE_INT16: {
            return mp_obj_new_int(*(int16_t *)value);
        }        
        case LV_TYPE_INT32: {
            return mp_obj_new_int(*(int32_t *)value);
        }        
        case LV_TYPE_COLOR: {
            return mp_obj_new_int(lv_color_to_int(*(lv_color_t *)value));
        }        
        case LV_TYPE_STR: {
            const char *str = *(char **)value;
            return str ? mp_obj_new_str(str, lv_strlen(str)) : mp_const_none;
        }
        case LV_TYPE_AREA: {
            return lvgl_area_to_mp(value);
        }
        case LV_TYPE_POINT: {
            return lvgl_point_to_mp(value);
        }
        case LV_TYPE_POINT_PRECISE: {
            return lvgl_point_precise_to_mp(value);
        }        
        case LV_TYPE_IMAGE_SRC: {
            lv_image_src_t src_type = lv_image_src_get_type(*(const void **)value);
            if (src_type == LV_IMAGE_SRC_FILE) {
                const char *str = *(const char **)value + 2;
                return lvgl_type_to_mp(LV_TYPE_STR, &str);
            }
            else if (src_type == LV_IMAGE_SRC_SYMBOL) {
                return lvgl_type_to_mp(LV_TYPE_STR, value);
            }            
            else if (src_type == LV_IMAGE_SRC_VARIABLE) {
                return lvgl_type_to_mp(LV_TYPE_DRAW_BUFFER, value);
            }
            else {
                return mp_const_none;
            }
        }
        case LV_TYPE_OBJ_HANDLE: {
            return lvgl_obj_to_mp(*(lvgl_obj_handle_t **)value);
        }
        case LV_TYPE_PROP_LIST: {
            return lvgl_type_to_mp_prop_list(*(const lv_style_prop_t **)value);
        }
        case LV_TYPE_GC_HANDLE: {
            const gc_handle_t *gc_handle = *(const gc_handle_t **)value;
            void *gc_ptr = NULL;
            if (gc_handle) {
                gc_ptr = gc_handle_get(gc_handle);
            }
            return gc_ptr ? MP_OBJ_FROM_PTR(gc_ptr) : mp_const_none;
        }
        default: {
            break;
        }
    }
    const lvgl_ptr_type_t *ptr_type = lvgl_type_is_ptr(type_code);
    if (ptr_type) {
        return lvgl_type_to_mp_ptr(ptr_type, *(void **)value);
    }      
    const lvgl_static_ptr_type_t *static_ptr_type = lvgl_type_is_static_ptr(type_code);
    if (static_ptr_type) {
        return lvgl_static_ptr_to_mp(static_ptr_type, *(const void **)value);
    }
    assert(0);
    return MP_OBJ_NULL;
}

void lvgl_type_clone_ptr(const lvgl_ptr_type_t *ptr_type, void **plv_dst, const void *lv_src) {
    lvgl_ptr_handle_t *handle = lvgl_ptr_from_lv(ptr_type, lv_src);
    *plv_dst = lvgl_ptr_to_lv(handle);
    lvgl_ptr_copy(handle);    
}

static void lvgl_type_clone_prop_list(lv_style_prop_t **dst, const lv_style_prop_t *src) {
    if (src) {
        size_t len = 0;
        for(const lv_style_prop_t *props = src; *props; props++) {
            len++;
        }
        size_t size = sizeof(lv_style_prop_t) * (len + 1);
        *dst = lv_malloc(size);
        lv_memcpy(*dst, src, size);
    }
    else {
        *dst = NULL;
    }
}

void lvgl_type_clone_array(lv_type_code_t type_code, size_t array_size, void **dst, const void *src) {
    size_t elem_size = lvgl_type_sizeof(type_code);
    *dst = lv_malloc(array_size * elem_size);
    lv_memcpy(*dst, src, array_size * elem_size);
}

void lvgl_type_clone(lv_type_code_t type_code, void *dst, const void *src) {
    switch (type_code) {
        case LV_TYPE_INT8: {
            *(int8_t *)dst = *(int8_t *)src;
            return;
        }
        case LV_TYPE_INT16: {
            *(int16_t *)dst = *(int16_t *)src;
            return;
        }        
        case LV_TYPE_INT32: {
            *(int32_t *)dst = *(int32_t *)src;
            return;
        }        
        case LV_TYPE_COLOR: {
            *(lv_color_t *)dst = *(lv_color_t *)src;
            return;
        }        
        case LV_TYPE_STR: {
            *(char **)dst = lv_strdup(*(const char **)src);
            return;
        }
        case LV_TYPE_IMAGE_SRC: {
            lv_image_src_t src_type = lv_image_src_get_type(*(const void **)src);
            if ((src_type == LV_IMAGE_SRC_FILE) || (src_type == LV_IMAGE_SRC_SYMBOL)) {
                lvgl_type_clone(LV_TYPE_STR, dst, src);
            }
            else if (src_type == LV_IMAGE_SRC_VARIABLE) {
                lvgl_type_clone(LV_TYPE_DRAW_BUFFER, dst, src);
            }
            else {
                *(const void **)dst = *(const void **)src;
            }
            return;
        }  
        case LV_TYPE_OBJ_HANDLE: {
            lvgl_obj_handle_t *handle = *(lvgl_obj_handle_t **)src;
            lvgl_ptr_copy(&handle->base);
            *(lvgl_obj_handle_t **)dst = handle;
            return;
        }
        case LV_TYPE_PROP_LIST: {
            lvgl_type_clone_prop_list(dst, *(const lv_style_prop_t **)src);
            return;
        }
        case LV_TYPE_GC_HANDLE: {
            gc_handle_t *gc_handle = *(gc_handle_t **)src;
            *(gc_handle_t **)dst = gc_handle ? gc_handle_copy(gc_handle) : NULL;
            return;
        }
        default: {
            break;
        }
    }
    const lvgl_ptr_type_t *ptr_type = lvgl_type_is_ptr(type_code);
    if (ptr_type) {
        lvgl_type_clone_ptr(ptr_type, dst, *(const void **)src);
        return;
    }       
    const lvgl_static_ptr_type_t *static_ptr_type = lvgl_type_is_static_ptr(type_code);
    if (static_ptr_type) {
        *(const void **)dst = *(const void **)src;
        return;
    }
    assert(0);
}


void lvgl_attrs_free(const lvgl_type_attr_t *attrs, void *value) {
    for (; attrs->qstr; attrs++) {
        lvgl_type_free(attrs->type_code, value + attrs->offset);
    }
}

void lvgl_type_attr(qstr attr, mp_obj_t *dest, lv_type_code_t type_code, void *value) {
    lvgl_super_attr_check(attr, true, true, false, dest);

    if (dest[0] != MP_OBJ_SENTINEL) {
        dest[0] = lvgl_type_to_mp(type_code, value);
    }
    else if (dest[1] != MP_OBJ_NULL) {
        lvgl_type_free(type_code, value);
        lvgl_type_from_mp(type_code, dest[1], value);
        dest[0] = MP_OBJ_NULL;
    }
}

bool lvgl_attrs_attr(qstr attr, mp_obj_t *dest, const lvgl_type_attr_t *attrs, void *value) {
    for (; attrs->qstr; attrs++) {
        if (attrs->qstr == attr) {
            lvgl_type_attr(attr, dest, attrs->type_code, value + attrs->offset);
            return true;
        }
    }
    return false;
}

uint lvgl_bitfield_attr_bool(qstr attr, mp_obj_t *dest, uint value) {
    lvgl_super_attr_check(attr, true, true, false, dest);
    if (dest[0] != MP_OBJ_SENTINEL) {
        dest[0] = mp_obj_new_bool(value);
    }
    else if (dest[1] != MP_OBJ_NULL) {
        value = mp_obj_is_true(dest[1]);
        dest[0] = MP_OBJ_NULL;
    }
    return value;
}

uint lvgl_bitfield_attr_int(qstr attr, mp_obj_t *dest, uint value) {
    lvgl_super_attr_check(attr, true, true, false, dest);
    if (dest[0] != MP_OBJ_SENTINEL) {
        dest[0] = mp_obj_new_int(value);
    }
    else if (dest[1] != MP_OBJ_NULL) {
        value = mp_obj_get_int(dest[1]);
        dest[0] = MP_OBJ_NULL;
    }
    return value;
}
