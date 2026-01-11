#include <py/runtime.h>

#if !MICROPY_FREERTOS
MP_NORETURN void abort_(void);

MP_NORETURN void abort_(void) {
    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("abort() called"));
}
#endif
