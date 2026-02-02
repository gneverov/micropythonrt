function(add_micropy_extension_library target lib)
    if(MICROPY_DYNLINK)
        add_dynamic_library(${target} ${ARGN})
        target_link_libraries(${target} micropython_import ${lib})
        target_link_options(${target} PRIVATE
            LINKER:--script=${RP2_LIB_LD_SCRIPT}
            # LINKER:--no-warn-rwx-segments
            LINKER:-Map=$<TARGET_FILE:${target}>.map
        )
        set_target_properties(${target} PROPERTIES NEWLIB_DYNLINK_OPTIONS "--entry;0x6000000E;mp_extmod_module")
        set(genhdr_dir ${CMAKE_BINARY_DIR}/genhdr)
        get_target_property(srcs ${lib} INTERFACE_SOURCES)
        target_compile_definitions(${lib} INTERFACE
            MICROPY_PY_EXTENSION
            NO_QSTR
            MP_QSTRDEFSFILE="${genhdr_dir}/${target}.qstrdefs.h"
        )
        add_custom_command(
            OUTPUT ${genhdr_dir}/${target}.qstrdefs.h ${genhdr_dir}/${target}.qstrdefs.c
            COMMAND ${Python3_EXECUTABLE} ${MICROPY_PY_DIR}/makeqstrdefs_ext.py ${target} ${srcs}
            DEPENDS ${MICROPY_PY_DIR}/makeqstrdefs_ext.py ${srcs}
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
        target_sources(${target} PRIVATE
            ${genhdr_dir}/${target}.qstrdefs.c
        )
        add_import_library(${target}_import ${target})
        add_dependencies(${target} ${MICROPY_TARGET})
    else()
        target_link_libraries(${MICROPY_TARGET} ${lib})
    endif()
endfunction()

function(set_target_visibility_hidden TARGET)
    get_target_property(SRCS ${TARGET} INTERFACE_SOURCES)
    set_source_files_properties(${SRCS} PROPERTIES COMPILE_OPTIONS -fvisibility=hidden)
endfunction()

# Include component cmake fragments
include(${MICROPY_DIR}/py/py.cmake)
include(${MICROPY_DIR}/extmod/extmod.cmake)
include(${MICROPY_DIR}/py/usermod.cmake)

add_subdirectory(${MICROPY_PORT_DIR}/lwip)

set(TINYUF2_DIR "${MICROPY_DIR}/lib/tinyuf2")
add_subdirectory(${MICROPY_PORT_DIR}/tinyuf2)
set_target_visibility_hidden(tinyuf2)

set_target_visibility_hidden(tinyusb_device_base)
set_target_visibility_hidden(tinyusb_common_base)
set_target_visibility_hidden(tinyusb_device_base)

if(MICROPY_DYNLINK)
    add_dynamic_executable(${MICROPY_TARGET})
else()
    add_executable(${MICROPY_TARGET})
endif()

# Provide a C-level definitions of PICO_ARM.
# (The pico-sdk already defines PICO_RISCV when it's enabled.)
if(PICO_ARM)
    target_compile_definitions(pico_platform_headers INTERFACE
        PICO_ARM=1
    )
endif()

set(MICROPY_QSTRDEFS_PORT
    ${MICROPY_PORT_DIR}/qstrdefsport.h
)

set(MICROPY_SOURCE_LIB
    # ${MICROPY_DIR}/shared/netutils/dhcpserver.c
    ${MICROPY_DIR}/shared/netutils/trace.c
    ${MICROPY_DIR}/shared/readline/readline.c
    ${MICROPY_DIR}/shared/runtime/gchelper_native.c
    ${MICROPY_DIR}/shared/runtime/interrupt_char.c
    ${MICROPY_DIR}/shared/runtime/pyexec.c
    ${MICROPY_DIR}/shared/runtime/stdout_helpers.c
    ${MICROPY_DIR}/shared/timeutils/timeutils.c
)

if(PICO_ARM)
    list(APPEND MICROPY_SOURCE_LIB
        ${MICROPY_DIR}/shared/runtime/gchelper_thumb1.s
    )
elseif(PICO_RISCV)
    list(APPEND MICROPY_SOURCE_LIB
        ${MICROPY_DIR}/shared/runtime/gchelper_rv32i.s
    )
endif()

set(MICROPY_SOURCE_DRIVERS
    ${MICROPY_DIR}/drivers/bus/softspi.c
    ${MICROPY_DIR}/drivers/dht/dht.c
)

set(MICROPY_SOURCE_PORT
    clocks_extra.c
    help.c
    machine_i2c.c
    machine_pin.c
    machine_rtc.c
    machine_spi.c
    machine_timer.c
    machine/audio_out_pwm.c
    machine/pin.c
    machine/pio_sm.c
    # machine/uart.c
    main.c
    modrp2.c
    mphalport.c
    mpthreadport.c
    newlib_drv.c
    tinyusb/tusb_default_config.c
    usbd.c
    ${CMAKE_BINARY_DIR}/pins_${MICROPY_BOARD}.c
)

set(MICROPY_SOURCE_QSTR
    ${MICROPY_SOURCE_PY}
    ${MICROPY_DIR}/shared/readline/readline.c
    ${MICROPY_PORT_DIR}/machine_adc.c
    ${MICROPY_PORT_DIR}/machine_i2c.c
    ${MICROPY_PORT_DIR}/machine_pin.c
    ${MICROPY_PORT_DIR}/machine_rtc.c
    ${MICROPY_PORT_DIR}/machine_spi.c
    ${MICROPY_PORT_DIR}/machine_timer.c
    ${MICROPY_PORT_DIR}/machine_wdt.c
    ${MICROPY_PORT_DIR}/machine/audio_out_pwm.c
    ${MICROPY_PORT_DIR}/machine/pin.c
    ${MICROPY_PORT_DIR}/machine/pio_sm.c
    ${MICROPY_PORT_DIR}/machine/uart.c
    ${MICROPY_PORT_DIR}/modmachine.c
    ${MICROPY_PORT_DIR}/modrp2.c
    ${MICROPY_PORT_DIR}/mpthreadport.c
    ${CMAKE_BINARY_DIR}/pins_${MICROPY_BOARD}.c
)

set(PICO_SDK_COMPONENTS
    hardware_adc
    hardware_base
    hardware_clocks
    hardware_dma
    hardware_flash
    hardware_gpio
    hardware_i2c
    hardware_irq
    hardware_pio
    hardware_pll
    hardware_pwm
    hardware_regs
    hardware_spi
    hardware_structs
    hardware_sync
    hardware_timer
    hardware_uart
    hardware_watchdog
    hardware_xosc
    pico_base_headers
    pico_binary_info
    pico_bootrom
    pico_float
    pico_platform
    pico_rand
    pico_stdlib
    pico_unique_id
    pico_util
    tinyusb_common
    tinyusb_device
)

if(PICO_ARM)
    list(APPEND PICO_SDK_COMPONENTS
        cmsis_core
    )
elseif(PICO_RISCV)
    list(APPEND PICO_SDK_COMPONENTS
        hardware_hazard3
        hardware_riscv
    )
endif()

if(MICROPY_EXTMOD_EXAMPLE)
    include(${MICROPY_DIR}/extmod/example/example.cmake)
    add_micropy_extension_library(libexample extmod_example)    
endif()

if(MICROPY_LVGL)
    include(${MICROPY_DIR}/extmod/lvgl/lvgl.cmake)
    add_micropy_extension_library(liblvgl mp_lvgl)    
endif()

if(MICROPY_PY_LWIP)
    # Patch the tinyusb_device project to support networking
    target_include_directories(tinyusb_device INTERFACE 
        ${PICO_TINYUSB_PATH}/lib/networking
    )
    target_sources(tinyusb_device INTERFACE 
        ${PICO_TINYUSB_PATH}/lib/networking/rndis_reports.c
    )

    target_link_libraries(${MICROPY_TARGET} lwip_helper morelib_lwip)

    list(APPEND PICO_SDK_COMPONENTS
        pico_lwip_core
        pico_lwip_core4
        pico_lwip_core6
        pico_lwip_contrib_freertos
        # pico_lwip_freertos
        pico_lwip_mdns
        pico_lwip_snmp
        pico_lwip_sntp
    )
    set_source_files_properties(
        ${PICO_LWIP_PATH}/src/apps/snmp/snmp_traps.c
        PROPERTIES COMPILE_OPTIONS -Wno-error=dangling-pointer
    )
    target_sources(${MICROPY_TARGET} PRIVATE
        ${PICO_LWIP_PATH}/src/api/err.c
        ${PICO_LWIP_PATH}/src/api/tcpip.c
        ${PICO_LWIP_PATH}/src/netif/ethernet.c
    )

    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        MICROPY_PY_LWIP=1
    )

    if(CMAKE_BUILD_TYPE MATCHES Debug)
        target_compile_definitions(${MICROPY_TARGET} PRIVATE
            LWIP_DEBUG=1
        )
    endif()
endif()

if (MICROPY_PY_AUDIO_MP3)
    add_micropy_extension_library(libaudio_mp3 micropy_lib_audio_mp3)
endif()

if(MICROPY_PY_BLUETOOTH)
    list(APPEND MICROPY_SOURCE_PORT mpbthciport.c)
    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        MICROPY_PY_BLUETOOTH=1
        MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS=1
        MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE=1
    )
endif()

if (MICROPY_PY_BLUETOOTH_CYW43)
    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        CYW43_ENABLE_BLUETOOTH=1
        MICROPY_PY_BLUETOOTH_CYW43=1
    )

    if (MICROPY_BLUETOOTH_BTSTACK)
        target_link_libraries(${MICROPY_TARGET}
            pico_btstack_hci_transport_cyw43
        )
    endif()
endif()

if (MICROPY_BLUETOOTH_BTSTACK)
    string(CONCAT GIT_SUBMODULES "${GIT_SUBMODULES} " lib/btstack)

    list(APPEND MICROPY_SOURCE_PORT mpbtstackport.c)

    include(${MICROPY_DIR}/extmod/btstack/btstack.cmake)
    target_link_libraries(${MICROPY_TARGET} micropy_extmod_btstack)

    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        MICROPY_BLUETOOTH_BTSTACK=1
        MICROPY_BLUETOOTH_BTSTACK_CONFIG_FILE=\"btstack_inc/btstack_config.h\"
    )

    # For modbluetooth_btstack.c includes
    get_target_property(BTSTACK_INCLUDE micropy_extmod_btstack INTERFACE_INCLUDE_DIRECTORIES)
    list(APPEND MICROPY_INC_CORE ${BTSTACK_INCLUDE})
endif()

if(MICROPY_BLUETOOTH_NIMBLE)
    string(CONCAT GIT_SUBMODULES "${GIT_SUBMODULES} " lib/mynewt-nimble)
    if(NOT (${ECHO_SUBMODULES}) AND NOT EXISTS ${MICROPY_DIR}/lib/mynewt-nimble/nimble/host/include/host/ble_hs.h)
        message(FATAL_ERROR " mynewt-nimble not initialized.\n Run 'make BOARD=${MICROPY_BOARD} submodules'")
    endif()

    list(APPEND MICROPY_SOURCE_PORT mpnimbleport.c)
    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        MICROPY_BLUETOOTH_NIMBLE=1
        MICROPY_BLUETOOTH_NIMBLE_BINDINGS_ONLY=0
        MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING=1
        MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS=1
    )
    target_compile_options(${MICROPY_TARGET} PRIVATE
    # TODO: This flag is currently needed to make nimble build.
    -Wno-unused-but-set-variable
    )
    include(${MICROPY_DIR}/extmod/nimble/nimble.cmake)
    target_link_libraries(${MICROPY_TARGET} micropy_extmod_nimble)
    get_target_property(NIMBLE_INCLUDE micropy_extmod_nimble INTERFACE_INCLUDE_DIRECTORIES)
    list(APPEND MICROPY_INC_CORE ${NIMBLE_INCLUDE})
endif()

if (MICROPY_PY_NETWORK_CYW43)
    add_library(network_cyw43 INTERFACE)

    target_sources(network_cyw43 INTERFACE 
        network_cyw43.c
        modnetwork_cyw43.c
        machine_pin_cyw43.c
    )

    set_source_files_properties(
        network_cyw43.c
        modnetwork_cyw43.c 
        PROPERTIES MICROPY_SOURCE_QSTR ON)

    target_compile_definitions(network_cyw43 INTERFACE
        MICROPY_PY_NETWORK_CYW43=1
        CYW43_CONFIG_FILE="${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/include/cyw43_configport.h"
    )
    if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(network_cyw43 INTERFACE
        CYW43_USE_STATS=1
    )
    endif()

    target_link_libraries(network_cyw43 INTERFACE 
        cyw43_helper
        cyw43_driver_picow
    )
    set_target_visibility_hidden(cyw43_helper)
    set_target_visibility_hidden(cyw43_driver)
    set_target_visibility_hidden(cyw43_driver_picow)

    target_remove_property_value(cyw43_driver INTERFACE_SOURCES ${PICO_CYW43_DRIVER_PATH}/src/cyw43_lwip.c)
    target_remove_property_value(cyw43_driver_picow INTERFACE_LINK_LIBRARIES cybt_shared_bus)

    add_micropy_extension_library(libcyw43 network_cyw43)    
endif()

if (MICROPY_PY_NETWORK_NINAW10)
    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        MICROPY_PY_NETWORK_NINAW10=1
    )

    target_include_directories(${MICROPY_TARGET} PRIVATE
        ${MICROPY_DIR}/drivers/ninaw10/
    )

    # Enable NINA-W10 WiFi and Bluetooth drivers.
    list(APPEND MICROPY_SOURCE_DRIVERS
        ${MICROPY_DIR}/drivers/ninaw10/nina_bt_hci.c
        ${MICROPY_DIR}/drivers/ninaw10/nina_wifi_drv.c
        ${MICROPY_DIR}/drivers/ninaw10/nina_wifi_bsp.c
        ${MICROPY_DIR}/drivers/ninaw10/machine_pin_nina.c
    )
endif()

if (MICROPY_PY_NETWORK_WIZNET5K)
    string(CONCAT GIT_SUBMODULES "${GIT_SUBMODULES} " lib/wiznet5k)
    if((NOT (${ECHO_SUBMODULES})) AND NOT EXISTS ${MICROPY_DIR}/lib/wiznet5k/README.md)
        message(FATAL_ERROR " wiznet5k not initialized.\n Run 'make BOARD=${MICROPY_BOARD} submodules'")
    endif()

    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        MICROPY_PY_NETWORK_WIZNET5K=1
        WIZCHIP_PREFIXED_EXPORTS=1
        _WIZCHIP_=${MICROPY_PY_NETWORK_WIZNET5K}
        WIZCHIP_YIELD=mpy_wiznet_yield
    )

    if (MICROPY_PY_LWIP)
        target_compile_definitions(${MICROPY_TARGET} PRIVATE
            # When using MACRAW mode (with lwIP), maximum buffer space must be used for the raw socket
            WIZCHIP_USE_MAX_BUFFER=1
        )
    endif()

    target_include_directories(${MICROPY_TARGET} PRIVATE
        ${MICROPY_DIR}/lib/wiznet5k/
        ${MICROPY_DIR}/lib/wiznet5k/Ethernet/
    )

    list(APPEND MICROPY_SOURCE_LIB
        ${MICROPY_DIR}/lib/wiznet5k/Ethernet/W5100/w5100.c
        ${MICROPY_DIR}/lib/wiznet5k/Ethernet/W5100S/w5100s.c
        ${MICROPY_DIR}/lib/wiznet5k/Ethernet/W5200/w5200.c
        ${MICROPY_DIR}/lib/wiznet5k/Ethernet/W5300/w5300.c
        ${MICROPY_DIR}/lib/wiznet5k/Ethernet/W5500/w5500.c
        ${MICROPY_DIR}/lib/wiznet5k/Ethernet/socket.c
        ${MICROPY_DIR}/lib/wiznet5k/Ethernet/wizchip_conf.c
        ${MICROPY_DIR}/lib/wiznet5k/Internet/DNS/dns.c
        ${MICROPY_DIR}/lib/wiznet5k/Internet/DHCP/dhcp.c
    )
endif()

# Add qstr sources for extmod and usermod, in case they are modified by components above.
list(APPEND MICROPY_SOURCE_QSTR
    ${MICROPY_SOURCE_EXTMOD}
    ${MICROPY_SOURCE_USERMOD}
    ${MICROPY_SOURCE_BOARD}
)

# Define mpy-cross flags
set(MICROPY_CROSS_FLAGS -march=armv6m)

# Set the frozen manifest file
if(NOT MICROPY_FROZEN_MANIFEST)
    # set(MICROPY_FROZEN_MANIFEST ${MICROPY_PORT_DIR}/boards/manifest.py)
endif()

target_sources(${MICROPY_TARGET} PRIVATE
    ${MICROPY_SOURCE_PY}
    ${MICROPY_SOURCE_EXTMOD}
    ${MICROPY_SOURCE_LIB}
    ${MICROPY_SOURCE_DRIVERS}
    ${MICROPY_SOURCE_PORT}
    ${MICROPY_SOURCE_BOARD}
)

if(MICROPY_SSL_MBEDTLS)
    target_link_libraries(${MICROPY_TARGET} pico_mbedtls)
    target_include_directories(${MICROPY_TARGET} PRIVATE ${MICROPY_PORT_DIR}/mbedtls)
endif()

target_link_libraries(${MICROPY_TARGET} usermod)

target_include_directories(${MICROPY_TARGET} PRIVATE
    ${MICROPY_INC_CORE}
    ${MICROPY_INC_USERMOD}
    ${MICROPY_BOARD_DIR}
    "${MICROPY_PORT_DIR}"
    "${CMAKE_BINARY_DIR}"
)

target_compile_options(${MICROPY_TARGET} PRIVATE
    -Wall
    -Werror
    -g  # always include debug information in the ELF
)

target_link_options(${MICROPY_TARGET} PRIVATE
    -Wl,--wrap=runtime_init_clocks
)

# Apply optimisations to performance-critical source code.
set_source_files_properties(
    ${MICROPY_PY_DIR}/map.c
    ${MICROPY_PY_DIR}/mpz.c
    ${MICROPY_PY_DIR}/vm.c
    PROPERTIES
    COMPILE_OPTIONS "-O2"
)

set_source_files_properties(
    ${PICO_SDK_PATH}/src/rp2_common/pico_double/double_math.c
    ${PICO_SDK_PATH}/src/rp2_common/pico_float/float_math.c
    PROPERTIES
    COMPILE_OPTIONS "-Wno-error=uninitialized"
)

target_compile_definitions(${MICROPY_TARGET} PRIVATE
    ${MICROPY_DEF_BOARD}
    PICO_FLOAT_PROPAGATE_NANS=1
    PICO_HEAP_SIZE=0
    PICO_STACK_SIZE=4096
    PICO_CORE1_STACK_SIZE=4096
    PICO_FLASH_SIZE_BYTES=16777216
    PICO_MAX_SHARED_IRQ_HANDLERS=8 # we need more than the default
    PICO_PROGRAM_NAME="MicroPythonRT"
    PICO_NO_PROGRAM_VERSION_STRING=1 # do it ourselves in main.c
    MICROPY_BUILD_TYPE="${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} ${CMAKE_BUILD_TYPE}"
    PICO_NO_BI_STDIO_UART=1 # we call it UART REPL
    PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
    PICO_INT64_OPS_IN_RAM=1
    $<$<NOT:$<STREQUAL:${PICO_CHIP},rp2040>>:PSRAM_BASE=0x11000000u>
)

if(PICO_RP2040)
    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
    )
elseif(PICO_RP2350)
    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        PICO_EMBED_XIP_SETUP=1 # to put flash into continuous read mode
    )
endif()

target_link_libraries(${MICROPY_TARGET}
    morelib_rp2
    rp2_pioasm
    morelib_fatfs
    morelib_littlefs
    morelib_tinyusb
    tinyuf2
    ${PICO_SDK_COMPONENTS}
)
target_include_directories(morelib_tinyusb INTERFACE tinyusb/include)

if (MICROPY_HW_ENABLE_DOUBLE_TAP)
# Enable double tap reset into bootrom.
target_link_libraries(${MICROPY_TARGET}
    pico_bootsel_via_double_reset
)
endif()

# todo this is a bit brittle, but we want to move a few source files into RAM (which requires
#  a linker script modification) until we explicitly add  macro calls around the function
#  defs to move them into RAM.
if (PICO_ON_DEVICE AND NOT PICO_NO_FLASH AND NOT PICO_COPY_TO_RAM)
    pico_set_linker_script(${MICROPY_TARGET} ${RP2_EXE_LD_SCRIPT})
endif()

pico_add_extra_outputs(${MICROPY_TARGET})

pico_find_compiler_with_triples(PICO_COMPILER_SIZE "${PICO_GCC_TRIPLE}" size)

add_custom_command(TARGET ${MICROPY_TARGET}
    POST_BUILD
    COMMAND ${PICO_COMPILER_SIZE} --format=berkeley ${PROJECT_BINARY_DIR}/${MICROPY_TARGET}.elf
    VERBATIM
)

# Include the main MicroPython cmake rules.
include(${MICROPY_DIR}/py/mkrules.cmake)

if(NOT PICO_NUM_GPIOS)
    set(PICO_NUM_GPIOS 30)
endif()

if(NOT PICO_NUM_EXT_GPIOS)
    set(PICO_NUM_EXT_GPIOS 10)
endif()

if(PICO_RP2040)
    set(GEN_PINS_AF_CSV "${MICROPY_PORT_DIR}/boards/rp2040_af.csv")
elseif(PICO_RP2350)
    if(PICO_NUM_GPIOS EQUAL 48)
        set(GEN_PINS_AF_CSV "${MICROPY_PORT_DIR}/boards/rp2350b_af.csv")
    else()
        set(GEN_PINS_AF_CSV "${MICROPY_PORT_DIR}/boards/rp2350_af.csv")
    endif()
endif()

set(GEN_PINS_PREFIX "${MICROPY_PORT_DIR}/boards/rp2_prefix.c")
set(GEN_PINS_MKPINS "${MICROPY_PORT_DIR}/boards/make-pins.py")
set(GEN_PINS_SRC "${CMAKE_BINARY_DIR}/pins_${MICROPY_BOARD}.c")
set(GEN_PINS_HDR "${MICROPY_GENHDR_DIR}/pins.h")

if(NOT MICROPY_BOARD_PINS)
    set(MICROPY_BOARD_PINS "${MICROPY_BOARD_DIR}/pins.csv")
endif()

if(EXISTS "${MICROPY_BOARD_PINS}")
    set(GEN_PINS_BOARD_CSV "${MICROPY_BOARD_PINS}")
    set(GEN_PINS_CSV_ARG --board-csv "${MICROPY_BOARD_PINS}")
endif()

target_sources(${MICROPY_TARGET} PRIVATE
    ${GEN_PINS_HDR}
)

# Generate pins
add_custom_command(
    OUTPUT ${GEN_PINS_HDR} ${GEN_PINS_SRC}
    COMMAND ${Python3_EXECUTABLE} ${GEN_PINS_MKPINS} ${GEN_PINS_CSV_ARG} --af-csv ${GEN_PINS_AF_CSV} --prefix ${GEN_PINS_PREFIX} --output-source ${GEN_PINS_SRC} --num-gpios ${PICO_NUM_GPIOS} --num-ext-gpios ${PICO_NUM_EXT_GPIOS} --output-header ${GEN_PINS_HDR}
    DEPENDS
        ${GEN_PINS_AF_CSV}
        ${GEN_PINS_BOARD_CSV}
        ${GEN_PINS_MKPINS}
        ${GEN_PINS_PREFIX}
        ${MICROPY_MPVERSION}
    VERBATIM
    COMMAND_EXPAND_LISTS
)

if(MICROPY_DYNLINK)
    add_import_library(micropython_import ${MICROPY_TARGET})

    # Execute "ar t" to list objects in libc archive
    execute_process(COMMAND ${CMAKE_AR} t ${PICOLIBC_SYSROOT}/lib/${PICOLIBC_BUILDTYPE}/libc.a OUTPUT_VARIABLE LIBC_OBJS )
    separate_arguments(LIBC_OBJS UNIX_COMMAND ${LIBC_OBJS})

    set(LIBC_OBJS_DIR ${CMAKE_BINARY_DIR}/extract_libc/)
    list(TRANSFORM LIBC_OBJS PREPEND ${LIBC_OBJS_DIR})

    # Extract object files ("ar x") into output directory
    add_custom_target(extract_libc
        COMMAND mkdir -p ${LIBC_OBJS_DIR}
        COMMAND ${CMAKE_AR} x ${PICOLIBC_SYSROOT}/lib/${PICOLIBC_BUILDTYPE}/libc.a --output ${LIBC_OBJS_DIR}
        BYPRODUCTS ${LIBC_OBJS}
        VERBATIM
    )

    # Explicitly add certain objects from libc.a to support dynamic linking
    include(${MICROPY_PORT_DIR}/cmake/picolibc_objs.cmake)
    list(TRANSFORM LIBC_OBJECTS PREPEND ${LIBC_OBJS_DIR}/)
    target_sources(${MICROPY_TARGET} PRIVATE
        ${LIBC_OBJECTS}
    )
    add_dependencies(${MICROPY_TARGET} extract_libc)
endif()
