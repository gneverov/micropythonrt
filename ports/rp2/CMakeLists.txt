cmake_minimum_required(VERSION 3.27)
cmake_policy(SET CMP0126 OLD)

# Set build type to reduce firmware size
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

# Set main target and component locations
set(MICROPY_TARGET firmware)
get_filename_component(MICROPY_DIR "../.." ABSOLUTE)

# Set the location of this port's directory.
set(MICROPY_PORT_DIR ${CMAKE_CURRENT_LIST_DIR})

# Set the board if it's not already set.
if(NOT MICROPY_BOARD)
    set(MICROPY_BOARD RPI_PICO)
endif()

# Set the board directory and check that it exists.
if(NOT MICROPY_BOARD_DIR)
    set(MICROPY_BOARD_DIR ${MICROPY_PORT_DIR}/boards/${MICROPY_BOARD})
endif()
get_filename_component(MICROPY_BOARD_DIR ${MICROPY_BOARD_DIR} ABSOLUTE)
if(NOT EXISTS ${MICROPY_BOARD_DIR}/mpconfigboard.cmake)
    message(FATAL_ERROR "Invalid MICROPY_BOARD specified: ${MICROPY_BOARD}")
endif()

# Include board config, it may override MICROPY_FROZEN_MANIFEST
include(${MICROPY_BOARD_DIR}/mpconfigboard.cmake)

# Set the PICO_BOARD if it's not already set (allow a board to override it).
if(NOT PICO_BOARD)
    string(TOLOWER ${MICROPY_BOARD} PICO_BOARD)
endif()


include(${MICROPY_DIR}/lib/pico-sdk/pico_sdk_init.cmake)

find_package(Python3 COMPONENTS Interpreter)

# Define the top-level project
project(${MICROPY_TARGET})

# Calls pico_sdk_init()
set(FREERTOS_DIR ${MICROPY_DIR}/lib/freertos)
set(MORELIBC_DIR ${MICROPY_DIR}/lib/morelibc)
include(${MORELIBC_DIR}/rp2/pico-sdk.cmake)

include(${MORELIBC_DIR}/cmake/picolibc.cmake)

add_subdirectory(${FREERTOS_DIR} freertos)

set(FATFS_DIR ${MICROPY_DIR}/lib/fatfs)
set(LITTLEFS_DIR ${MICROPY_DIR}/lib/littlefs)
add_subdirectory(${MORELIBC_DIR} morelibc)

set(MICROPY_FREERTOS ON)
set(MICROPY_DYNLINK ON)
set(MICROPY_EXTMOD_EXAMPLE OFF)
set(MICROPY_PY_AUDIO_MP3 OFF)
set(MICROPY_LVGL ON)
set(MICROPY_SSL_MBEDTLS ON)
set(MICROPY_PY_LWIP ON)
set(MICROPY_PY_NETWORK_CYW43 ${PICO_CYW43_SUPPORTED})
include(cmake/micropy.cmake)
