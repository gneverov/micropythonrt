// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#ifndef LV_ILI9341_DISP_H
#define LV_ILI9341_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "FreeRTOS.h"
#include "task.h"

#include "rp2/spi.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    rp2_spi_t *spi;
    uint8_t cs;
    uint8_t dc;
    uint8_t dma;
    uint baudrate;
    int int_count;
    volatile TaskHandle_t task;    
} lv_ili9341_disp_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
int lv_ili9341_disp_init(lv_ili9341_disp_t *drv, rp2_spi_t *spi, uint cs, uint dc, uint baudrate, lv_display_t **disp);

void lv_ili9341_disp_deinit(lv_display_t *disp);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_ILI9341_DISP_H*/
