// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <string.h>

#include "tusb.h"
#include "tinyusb/tusb_config.h"

#include "extmod/modos_newlib.h"
#include "extmod/usb/usb_config.h"
#include "py/runtime.h"

#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) | _PID_MAP(ECM_RNDIS, 5) | _PID_MAP(NCM, 5) )


typedef struct {
    mp_obj_base_t base;
    tusb_config_t tusb_config;
    tusb_desc_configuration_t *cfg_desc;
    size_t ep_idx;
    size_t str_idx;
    tusb_desc_string_t *str_desc;
} usb_config_obj_t;

static uint8_t usb_config_str_desc(usb_config_obj_t *self, const uint16_t *desc) {
    uint16_t *p = (uint16_t *)self->str_desc;
    size_t maxlen = (&self->tusb_config.strings[TUSB_CONFIG_STR_SIZE] - p) * sizeof(uint16_t);
    uint length = ((const tusb_desc_string_t *)desc)->bLength;
    if (length > maxlen) {
        mp_raise_ValueError(NULL);
    }
    memcpy(p, desc, length);
    self->str_desc = (tusb_desc_string_t *)(p + length / sizeof(uint16_t));
    return self->str_idx++;    
}

static uint8_t usb_config_str(usb_config_obj_t *self, mp_obj_t py_str, const char *c_str) {
    if ((py_str != MP_OBJ_NULL) && (py_str != mp_const_none)) {
        c_str = mp_obj_str_get_str(py_str);
    }
    const byte *b_str = (const byte *)c_str;
    size_t str_len = strlen(c_str);
    if ((c_str == NULL) || (strlen(c_str) == 0)) {
        return 0;
    }
    
    uint16_t *p = (uint16_t *)self->str_desc;
    size_t maxlen = (&self->tusb_config.strings[TUSB_CONFIG_STR_SIZE] - p) * sizeof(uint16_t);
    uint desc_len = sizeof(uint16_t) * utf8_charlen(b_str, str_len) + sizeof(tusb_desc_string_t);
    if (desc_len > maxlen) {
        mp_raise_ValueError(NULL);
    }
    tusb_desc_string_t *desc = self->str_desc;
    desc->bLength = desc_len;
    desc->bDescriptorType = TUSB_DESC_STRING;
    size_t i = 0;
    for (const byte *c = b_str; (c < b_str + str_len) && (i < 126); c = utf8_next_char(c)) {
        unichar u = utf8_get_char(c);
        desc->unicode_string[i++] = u < 0x10000 ? u : 0xFFFD;
        
    }
    self->str_desc = (tusb_desc_string_t *)(p + desc->bLength / sizeof(uint16_t));
    return self->str_idx++;
}

static mp_obj_t usb_config_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    usb_config_obj_t *self = m_new_obj(usb_config_obj_t);
    memset(self, 0, sizeof(usb_config_obj_t));
    self->base.type = type;
    return MP_OBJ_FROM_PTR(self);
}

#if CFG_TUH_ENABLED
static mp_obj_t usb_config_host(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    // self->tusb_config.host = true;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_host_obj, 1, usb_config_host);
#endif

#if CFG_TUD_ENABLED
static mp_obj_t usb_config_device(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    self->cfg_desc = NULL;
    self->str_desc = (tusb_desc_string_t *)self->tusb_config.strings;
    self->str_idx = 0;
    usb_config_str_desc(self, tusb_default_config.strings);

    const tusb_desc_device_t *default_desc = &tusb_default_config.device;
    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_vid), MP_MAP_LOOKUP);
    uint16_t vid = elem ? mp_obj_get_int(elem->value) : default_desc->idVendor;

    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_pid), MP_MAP_LOOKUP);
    uint16_t pid = elem ? mp_obj_get_int(elem->value) : USB_PID;

    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_device), MP_MAP_LOOKUP);
    uint16_t device = elem ? mp_obj_get_int(elem->value) : default_desc->bcdDevice;

    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_manufacturer), MP_MAP_LOOKUP);
    uint8_t manufacturer_idx;
    if (elem) {
        manufacturer_idx = usb_config_str(self, elem->value, NULL);
    } else {
        manufacturer_idx = usb_config_str_desc(self, tud_descriptor_string_cb(default_desc->iManufacturer, 0));
    }

    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_product), MP_MAP_LOOKUP);
    uint8_t product_idx;
    if (elem) {
        product_idx = usb_config_str(self, elem->value, NULL);
    } else {
        product_idx = usb_config_str_desc(self, tud_descriptor_string_cb(default_desc->iProduct, 0));
    }

    void mp_usbd_port_get_serial_number(char *serial_buf);
    char serial[MICROPY_HW_USB_DESC_STR_MAX];
    mp_usbd_port_get_serial_number(serial);
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_serial), MP_MAP_LOOKUP);
    uint8_t serial_idx = usb_config_str(self, elem ? elem->value : MP_OBJ_NULL, serial);  

    tusb_desc_device_t *desc = &self->tusb_config.device;
    desc->bLength = sizeof(tusb_desc_device_t);
    desc->bDescriptorType = TUSB_DESC_DEVICE;
    desc->bcdUSB = 0x0200;
    desc->bDeviceClass = TUSB_CLASS_MISC;
    desc->bDeviceSubClass = MISC_SUBCLASS_COMMON;
    desc->bDeviceProtocol = MISC_PROTOCOL_IAD;
    desc->bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE;
    desc->idVendor = vid;
    desc->idProduct = pid;
    desc->bcdDevice = device;
    desc->iManufacturer = manufacturer_idx;
    desc->iProduct = product_idx;
    desc->iSerialNumber = serial_idx;
    desc->bNumConfigurations = 0;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_device_obj, 1, usb_config_device);

static mp_obj_t usb_config_configuration(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device.bLength) {
        mp_raise_ValueError(NULL);
    }
    size_t idx = self->tusb_config.device.bNumConfigurations++;
    uint8_t *p = self->cfg_desc ? (uint8_t *)self->cfg_desc + self->cfg_desc->wTotalLength : self->tusb_config.configs;
    tusb_desc_configuration_t *desc = self->cfg_desc = (tusb_desc_configuration_t *)p;

    mp_map_elem_t *elem;
    uint8_t str_idx = 0;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    if (elem) {
        str_idx = usb_config_str(self, elem->value, NULL);
    }

    uint8_t attribute = 0;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_attribute), MP_MAP_LOOKUP);
    if (elem) {
        attribute = mp_obj_get_int(elem->value);
    }

    uint8_t power = USBD_MAX_POWER_MA;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_power_ma), MP_MAP_LOOKUP);
    if (elem) {
        power = MIN((mp_uint_t)mp_obj_get_int(elem->value) / 2, USBD_MAX_POWER_MA);
    }

    desc->bLength = TUD_CONFIG_DESC_LEN;
    desc->bDescriptorType = TUSB_DESC_CONFIGURATION;
    desc->wTotalLength = desc->bLength;
    desc->bNumInterfaces = 0;
    desc->bConfigurationValue = idx + 1;
    desc->iConfiguration = str_idx;
    desc->bmAttributes = 0x80 | (attribute & 0x60);
    desc->bMaxPower = power;
    self->ep_idx = 1;
    return MP_OBJ_NEW_SMALL_INT(idx);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_configuration_obj, 1, usb_config_configuration);

static void usb_config_cfg_append(usb_config_obj_t *self, uint8_t *buf, size_t len) {
    assert(self->cfg_desc);
    assert(self->cfg_desc->wTotalLength + len <= 0xffff);

    tusb_desc_configuration_t *desc = self->cfg_desc ;
    uint8_t *p = (uint8_t *)desc + desc->wTotalLength;
    if (p + len > &self->tusb_config.configs[TUSB_CONFIG_CFG_SIZE]) {
        mp_raise_ValueError(NULL);
    }
    memcpy(p, buf, len);
    desc->wTotalLength += len;
}

#if CFG_TUD_CDC
static mp_obj_t usb_config_cdc(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device.bLength || !self->cfg_desc) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
    
    size_t itf = self->cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_CDC_DESCRIPTOR(itf, str_idx, 0x80 | ep_idx, 8, ep_idx + 1, 0x80 | (ep_idx + 1), 64)
    };
    usb_config_cfg_append(self, desc, sizeof(desc));
    self->cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_cdc_obj, 1, usb_config_cdc);
#endif

#if CFG_TUD_MSC
static mp_obj_t usb_config_msc(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device.bLength || !self->cfg_desc) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
    
    const char *vendor_id = (const char *)tusb_default_config.msc_vendor_id;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_vendor), MP_MAP_LOOKUP);
    if (elem) {
        vendor_id = mp_obj_str_get_str(elem->value);
    }  

    const char *product_id = (const char *)tusb_default_config.msc_product_id;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_product), MP_MAP_LOOKUP);
    if (elem) {
        product_id = mp_obj_str_get_str(elem->value);
    }  

    const char *product_rev = (const char *)tusb_default_config.msc_product_rev;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_revision), MP_MAP_LOOKUP);
    if (elem) {
        product_rev = mp_obj_str_get_str(elem->value);
    }

    size_t itf = self->cfg_desc->bNumInterfaces;
    assert(itf < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_MSC_DESCRIPTOR(itf, str_idx, ep_idx, 0x80 | ep_idx, 64)
    };
    usb_config_cfg_append(self, desc, sizeof(desc));
    self->cfg_desc->bNumInterfaces += 1;
    self->ep_idx += 1;

    strncpy((char *)self->tusb_config.msc_vendor_id, vendor_id, 8);
    strncpy((char *)self->tusb_config.msc_product_id, product_id, 16);
    strncpy((char *)self->tusb_config.msc_product_rev, product_rev, 4);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_msc_obj, 1, usb_config_msc);
#endif

#if CFG_TUD_AUDIO && CFG_TUD_AUDIO_ENABLE_EP_OUT
static mp_obj_t usb_config_audio_speaker(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device.bLength || !self->cfg_desc) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
    
    size_t itf = self->cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t cdc_desc[] = {
        TUD_AUDIO_SPEAKER_MONO_FB_DESCRIPTOR(itf, str_idx, 2 /*_nBytesPerSample*/, 16 /*_nBitsUsedPerSample*/, ep_idx, 64, 0x80 | (ep_idx + 1))
    };
    usb_config_cfg_append(self, cdc_desc, sizeof(cdc_desc));
    self->cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_audio_speaker_obj, 1, usb_config_audio_speaker);
#endif

#if CFG_TUD_AUDIO && CFG_TUD_AUDIO_ENABLE_EP_IN
static mp_obj_t usb_config_audio_mic(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device.bLength || !self->cfg_desc) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);

    size_t itf = self->cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(itf, str_idx, /*_nBytesPerSample*/ CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, /*_nBitsUsedPerSample*/ CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX*8, /*_epin*/ 0x80 | ep_idx, /*_epsize*/ CFG_TUD_AUDIO_EP_SZ_IN)
    };
    usb_config_cfg_append(self, desc, sizeof(desc));
    self->cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 1;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_audio_mic_obj, 1, usb_config_audio_mic);
#endif

#if CFG_TUD_ECM_RNDIS
static mp_obj_t usb_config_net_ecm(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device.bLength || !self->cfg_desc) {
        mp_raise_ValueError(NULL);
    }

    uint8_t mac_idx = usb_config_str(self, args[1], NULL);

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
  
    size_t itf = self->cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_CDC_ECM_DESCRIPTOR(itf, str_idx, mac_idx, 0x80 | ep_idx, 64, ep_idx + 1, 0x80 | (ep_idx + 1), CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU)
    };
    usb_config_cfg_append(self, desc, sizeof(desc));
    self->cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_net_ecm_obj, 2, usb_config_net_ecm);

static mp_obj_t usb_config_net_rndis(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device.bLength || !self->cfg_desc) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);

    size_t itf = self->cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_RNDIS_DESCRIPTOR(itf, str_idx, 0x80 | ep_idx, 8, ep_idx + 1, 0x80 | (ep_idx + 1), CFG_TUD_NET_ENDPOINT_SIZE)
    };
    usb_config_cfg_append(self, desc, sizeof(desc));
    self->cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_net_rndis_obj, 1, usb_config_net_rndis);
#endif

#if CFG_TUD_NCM
static mp_obj_t usb_config_net_ncm(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device.bLength || !self->cfg_desc) {
        mp_raise_ValueError(NULL);
    }

    uint8_t mac_idx = usb_config_str(self, args[1], NULL);

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
  
    size_t itf = cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_CDC_NCM_DESCRIPTOR(itf, str_idx, mac_idx, 0x80 | ep_idx, 64, ep_idx + 1, 0x80 | (ep_idx + 1), CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU)
    };
    usb_config_cfg_append(self, desc, sizeof(desc));
    self->cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_net_ncm_obj, 2, usb_config_net_ncm);
#endif
#endif

#ifndef NDEBUG
static void usb_config_print_tusb_config(const mp_print_t *print, const tusb_config_t *tusb_config) {
    mp_printf(print, "device = { ");
    const uint8_t *p8 = (const uint8_t *)&tusb_config->device;
    for (size_t i = 0; i < sizeof(tusb_desc_device_t); i++) {
        mp_printf(print, "0x%02x, ", (uint)p8[i]);
    }
    mp_printf(print, "}\n");
    
    mp_printf(print, "configs = {\n");
    p8 = tusb_config->configs;
    while (*p8) {
        const tusb_desc_configuration_t *cfg = (const tusb_desc_configuration_t *)p8;
        mp_printf(print, "\t");
        for (size_t i = 0; i < cfg->wTotalLength; i++) {
            mp_printf(print, "0x%02x, ", (uint)p8[i]);
        }
        mp_printf(print, "\n");
        p8 += cfg->wTotalLength;
    }
    mp_printf(print, "\t0,\n}\n");

    mp_printf(print, "strings = {\n");
    const uint16_t *p16 = tusb_config->strings;
    while (*p16) {
        const tusb_desc_string_t *str = (const tusb_desc_string_t *)p16;
        mp_printf(print, "\t");
        for (size_t i = 0; i < str->bLength / 2; i++) {
            mp_printf(print, "0x%04x, ", (uint)p16[i]);
        }
        mp_printf(print, "\n");
        p16 += str->bLength / 2;
    }
    mp_printf(print, "\t0,\n}\n");

    #if CFG_TUD_MSC
    mp_printf(print, "msc_vendor_id = '%.8s'\n", tusb_config->msc_vendor_id);
    mp_printf(print, "msc_product_id = '%.16s'\n", tusb_config->msc_product_id);
    mp_printf(print, "msc_product_rev = '%.4s'\n", tusb_config->msc_product_rev);
    #endif
    
    mp_printf(print, "crc = 0x%08lx\n", tusb_config->crc);
}

static void usb_config_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (kind == PRINT_REPR) {
        const mp_obj_type_t *type = mp_obj_get_type(self_in);
        mp_printf(print, "<%q>", type->name);
        return;
    }

    if (!self->tusb_config.device.bLength) {
        return;
    }

    usb_config_print_tusb_config(print, &self->tusb_config);
}
#endif

static mp_obj_t usb_config_exists(void) {
    return mp_obj_new_bool(tusb_config_get() != &tusb_default_config);
}
static MP_DEFINE_CONST_FUN_OBJ_0(usb_config_exists_fun, usb_config_exists);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(usb_config_exists_obj, MP_ROM_PTR(&usb_config_exists_fun));

static mp_obj_t usb_config_delete(void) {
    int ret = tusb_config_clear();
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(usb_config_delete_fun, usb_config_delete);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(usb_config_delete_obj, MP_ROM_PTR(&usb_config_delete_fun));

static mp_obj_t usb_config_save(mp_obj_t self_in) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int ret = tusb_config_set(&self->tusb_config);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(usb_config_save_obj, usb_config_save);

static mp_obj_t usb_config_load(void) {
    mp_obj_t self_in = usb_config_make_new(&usb_config_type, 0, 0, NULL);
    usb_config_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->tusb_config = *tusb_config_get();
    return self_in;
}
static MP_DEFINE_CONST_FUN_OBJ_0(usb_config_load_fun, usb_config_load);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(usb_config_load_obj, MP_ROM_PTR(&usb_config_load_fun));

static const mp_rom_map_elem_t usb_config_locals_dict_table[] = {
#if CFG_TUH_ENABLED    
    { MP_ROM_QSTR(MP_QSTR_host),          MP_ROM_PTR(&usb_config_host_obj) },
#endif
#if CFG_TUD_ENABLED    
    { MP_ROM_QSTR(MP_QSTR_device),          MP_ROM_PTR(&usb_config_device_obj) },
    { MP_ROM_QSTR(MP_QSTR_configuration),   MP_ROM_PTR(&usb_config_configuration_obj) },
    #if CFG_TUD_CDC
    { MP_ROM_QSTR(MP_QSTR_cdc),             MP_ROM_PTR(&usb_config_cdc_obj) },
    #endif
    #if CFG_TUD_MSC
    { MP_ROM_QSTR(MP_QSTR_msc),             MP_ROM_PTR(&usb_config_msc_obj) },
    #endif
    #if CFG_TUD_AUDIO && CFG_TUD_AUDIO_ENABLE_EP_OUT
    { MP_ROM_QSTR(MP_QSTR_audio_speaker),   MP_ROM_PTR(&usb_config_audio_speaker_obj) },
    #endif    
    #if CFG_TUD_AUDIO && CFG_TUD_AUDIO_ENABLE_EP_IN
    { MP_ROM_QSTR(MP_QSTR_audio_mic),       MP_ROM_PTR(&usb_config_audio_mic_obj) },
    #endif
    #if CFG_TUD_ECM_RNDIS
    { MP_ROM_QSTR(MP_QSTR_net_ecm),         MP_ROM_PTR(&usb_config_net_ecm_obj) },
    { MP_ROM_QSTR(MP_QSTR_net_rndis),       MP_ROM_PTR(&usb_config_net_rndis_obj) },
    #endif
    #if CFG_TUD_NCM
    { MP_ROM_QSTR(MP_QSTR_net_ncm),         MP_ROM_PTR(&usb_config_net_ncm_obj) },
    #endif
#endif
    { MP_ROM_QSTR(MP_QSTR_exists),          MP_ROM_PTR(&usb_config_exists_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&usb_config_delete_obj) },
    { MP_ROM_QSTR(MP_QSTR_save),            MP_ROM_PTR(&usb_config_save_obj) },
    { MP_ROM_QSTR(MP_QSTR_load),            MP_ROM_PTR(&usb_config_load_obj) },
};
static MP_DEFINE_CONST_DICT(usb_config_locals_dict, usb_config_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    usb_config_type,
    MP_QSTR_UsbConfig,
    MP_TYPE_FLAG_NONE,
    make_new, usb_config_make_new,
    #ifndef NDEBUG
    print, usb_config_print,
    #endif
    locals_dict, &usb_config_locals_dict
    );
