if(MICROPY_SSL_MBEDTLS AND MICROPY_PY_LWIP)
    list(APPEND MICROPY_SOURCE_EXTMOD
        ${MICROPY_EXTMOD_DIR}/ssl/certificate.c
        ${MICROPY_EXTMOD_DIR}/ssl/modssl.c
        ${MICROPY_EXTMOD_DIR}/ssl/sslcontext.c
        ${MICROPY_EXTMOD_DIR}/ssl/sslsocket.c
    )
endif()
