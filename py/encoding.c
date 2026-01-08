// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <ctype.h>
#include <stdio.h>

#include "py/encoding.h"
#include "py/runtime.h"


qstr mp_get_encoding(mp_obj_t encoding) {
    size_t len;
    const char *bytes = mp_obj_str_get_data(encoding, &len);
    vstr_t vstr;
    vstr_init(&vstr, len);
    for (size_t i = 0; i < len; i++) {
        if (!isascii(bytes[i]) || iscntrl(bytes[i])) {
            goto fail;
        }
        if (isalnum(bytes[i])) {
            vstr_add_byte(&vstr, tolower(bytes[i]));
        }
    }
    encoding = mp_obj_new_str_from_vstr(&vstr);
    if (mp_obj_is_qstr(encoding)) {
        return mp_obj_str_get_qstr(encoding);
    }
fail:
    mp_raise_msg(&mp_type_LookupError, MP_ERROR_TEXT("unknown encoding"));
}

static int mp_decode_utf8(wchar_t *wc, const char *bytes, size_t len) {
    *wc = 0;
    if (len == 0) {
        return 0;
    }

    size_t n = 0;  // parsing an n-byte code point sequence
    // at the start of a new sequence
    if (bytes[0] < 0x80) {
        // ASCII code point (1 byte sequence)
        n = 1;
        *wc = bytes[0] & 0x7F;
    } else if (bytes[0] < 0xC2) {
        // error
    } else if (bytes[0] < 0xE0) {
        // 2 byte sequence
        n = 2;
        *wc = bytes[0] & 0x1F;
    } else if (bytes[0] < 0xF0) {
        // 3 byte sequence
        n = 3;
        *wc = bytes[0] & 0x0F;
    } else if (bytes[0] < 0xF5) {
        // 4 byte sequence
        n = 4;
        *wc = bytes[0] & 0x07;
    } else {
        // error;
    }
    if (len < n) {
        // not enough bytes
        return 0;
    }

    size_t i = 1;  // at the ith byte of an n-byte code point sequence
    while (i < n) {
        // in the middle of a sequence
        if (bytes[i] < 0x80) {
            // error
            n = 0;
        } else if (bytes[i] < 0xC0) {
            // continuation byte
            *wc <<= 6;
            *wc |= bytes[i] & 0x3F;
        } else {
            // error
            n = 0;
        }
        i++;
    }

    return n;
}

mp_obj_t mp_decode(const char *bytes, size_t len, int encoding, int errors) {
    vstr_t vstr;
    vstr_init(&vstr, len);
    while (len) {
        char ch = *bytes;
        switch (encoding) {
            case MP_QSTR_utf8: {
                wchar_t wc;
                int ret = mp_decode_utf8(&wc, bytes, len);
                if (ret > 0) {
                    if ((wc & ~0x7FF) != 0xD800) {
                        vstr_add_char(&vstr, wc);
                        bytes += ret;
                        len -= ret;
                        continue;
                    }
                }
                break;
            }
            case MP_QSTR_ascii: {
                if (ch <= 0x7F) {
                    vstr_add_byte(&vstr, ch);
                    bytes++;
                    len--;
                    continue;
                }
                break;
            }
            case MP_QSTR_latin1:
            case MP_QSTR_iso88591: {
                vstr_add_char(&vstr, ch);
                bytes++;
                len--;
                continue;
            }
            default:
                mp_raise_msg(&mp_type_LookupError, MP_ERROR_TEXT("unknown encoding"));
                break;
        }

        switch (errors) {
            case MP_QSTR_strict:
                mp_raise_type(&mp_type_UnicodeError);
                break;
            case MP_QSTR_ignore:
                break;
            case MP_QSTR_replace:
                vstr_add_char(&vstr, 0xFFFD);
                break;
            case MP_QSTR_backslashreplace:
                sprintf(vstr_add_len(&vstr, 4), "\\x%02x", ch);
                break;
            case MP_QSTR_surrogateescape:
                vstr_add_char(&vstr, 0xDC00 + ch);
                break;
            default:
                mp_raise_msg(&mp_type_LookupError, MP_ERROR_TEXT("unknown error handler name"));
                break;
        }
        bytes++;
        len--;
    }
    return mp_obj_new_str_from_utf8_vstr(&vstr);
}

mp_obj_t mp_encode(const char *bytes, size_t len, int encoding, int errors) {
    vstr_t vstr;
    vstr_init(&vstr, len);
    while (len) {
        wchar_t wc;
        int ret = mp_decode_utf8(&wc, bytes, len);
        if (ret == 0) {
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("invalid utf-8"));
        }
        bytes += ret;
        len -= ret;
        switch (encoding) {
            case MP_QSTR_utf8:
                if ((wc & ~0x7FF) != 0xD800) {
                    vstr_add_char(&vstr, wc);
                    continue;
                }
                // code point is a surrogate
                break;
            case MP_QSTR_idna:
                errors = MP_QSTR_strict;
            // idna not properly implemented
            case MP_QSTR_ascii:
                if (wc <= 0x7F) {
                    vstr_add_byte(&vstr, wc);
                    continue;
                }
                // code point out of range
                break;
            case MP_QSTR_latin1:
            case MP_QSTR_iso88591:
                if (wc <= 0xFF) {
                    vstr_add_byte(&vstr, wc);
                    continue;
                }
                // code point out of range
                break;
            default:
                mp_raise_msg(&mp_type_LookupError, MP_ERROR_TEXT("unknown encoding"));
                break;
        }
        switch (errors) {
            case MP_QSTR_strict:
                mp_raise_type(&mp_type_UnicodeError);
                break;
            case MP_QSTR_ignore:
                break;
            case MP_QSTR_replace:
                vstr_add_byte(&vstr, '?');
                break;
            case MP_QSTR_backslashreplace:
                sprintf(vstr_add_len(&vstr, 6), (wc <= 0xFF) ? "\\x%02x" : "\\u%04x", wc);
                break;
            case MP_QSTR_surrogateescape:
                if ((wc & ~0x7f) == 0xDC80) {
                    vstr_add_byte(&vstr, wc & 0xff);
                } else {
                    mp_raise_type(&mp_type_UnicodeError);
                }
                break;
            default:
                mp_raise_msg(&mp_type_LookupError, MP_ERROR_TEXT("unknown error handler name"));
                break;
        }
    }
    return mp_obj_new_bytes_from_vstr(&vstr);
}
