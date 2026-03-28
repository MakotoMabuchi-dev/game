#ifndef BSP_ST77916_H
#define BSP_ST77916_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_display.h"

#define BSP_LCD_SCLK_PIN 10
#define BSP_LCD_D0_PIN 11
#define BSP_LCD_D1_PIN 12
#define BSP_LCD_D2_PIN 13
#define BSP_LCD_D3_PIN 14
#define BSP_LCD_CS_PIN 15
#define BSP_LCD_RST_PIN 16
#define BSP_LCD_TE_PIN 17
#define BSP_LCD_BL_PIN 24

#define PWM_FREQ 10000
#define PWM_WRAP 1000

bool bsp_display_new_st77916(bsp_display_interface_t **interface, bsp_display_info_t *info);

#endif
