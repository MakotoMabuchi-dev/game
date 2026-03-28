#ifndef BSP_CST816_H
#define BSP_CST816_H

#include "pico/stdlib.h"
#include "bsp_touch.h"

#define BSP_CST816_RST_PIN 9
#define BSP_CST816_INT_PIN 8
#define CST816_LCD_TOUCH_MAX_POINTS 1
#define CST816_DEVICE_ADDR 0x15

typedef enum {
    CST816_REG_DATA_START = 0x02,
    CST816_REG_CHIP_ID = 0xA7,
    CST816_REG_SLEEP = 0xFE,
} cst816_reg_t;

bool bsp_touch_new_cst816(bsp_touch_interface_t **interface, bsp_touch_info_t *info);

#endif
