#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "bsp_cst816.h"
#include "bsp_i2c.h"

static bsp_touch_info_t *g_touch_info;

static void bsp_cst816_reg_read_byte(uint8_t reg_addr, uint8_t *data, size_t len)
{
    bsp_i2c_read_reg8(CST816_DEVICE_ADDR, reg_addr, data, len);
}

static void bsp_cst816_reg_write_byte(uint8_t reg_addr, uint8_t *data, size_t len)
{
    bsp_i2c_write_reg8(CST816_DEVICE_ADDR, reg_addr, data, len);
}

static void bsp_cst816_reset(void)
{
    gpio_put(BSP_CST816_RST_PIN, 0);
    sleep_ms(10);
    gpio_put(BSP_CST816_RST_PIN, 1);
    sleep_ms(100);
}

static void bsp_cst816_read(void)
{
    typedef struct {
        uint8_t num;
        uint8_t x_h : 4;
        uint8_t : 4;
        uint8_t x_l;
        uint8_t y_h : 4;
        uint8_t : 4;
        uint8_t y_l;
    } data_t;

    data_t point;
    bsp_cst816_reg_read_byte(CST816_REG_DATA_START, (uint8_t *)&point, sizeof(point));
    if (point.num > 0 && point.num <= CST816_LCD_TOUCH_MAX_POINTS) {
        g_touch_info->data.coords[0].x = (uint16_t)(point.x_h << 8) | point.x_l;
        g_touch_info->data.coords[0].y = (uint16_t)(point.y_h << 8) | point.y_l;
        g_touch_info->data.points = point.num;
    } else {
        g_touch_info->data.points = 0;
    }
}

static bool bsp_cst816_get_touch_data(bsp_touch_data_t *data)
{
    memcpy(data, &g_touch_info->data, sizeof(*data));
    g_touch_info->data.points = 0;
    return data->points > 0;
}

static void bsp_cst816_set_rotation(uint16_t rotation)
{
    g_touch_info->rotation = rotation;
}

static void bsp_cst816_get_rotation(uint16_t *rotation)
{
    *rotation = g_touch_info->rotation;
}

static void bsp_cst816_init(void)
{
    uint8_t id = 0;
    uint8_t sleep = 0xFF;

    gpio_init(BSP_CST816_RST_PIN);
    gpio_set_dir(BSP_CST816_RST_PIN, GPIO_OUT);
    bsp_cst816_reset();

    gpio_init(BSP_CST816_INT_PIN);
    gpio_set_dir(BSP_CST816_INT_PIN, GPIO_IN);
    gpio_pull_up(BSP_CST816_INT_PIN);

    sleep_ms(100);
    bsp_cst816_reg_read_byte(CST816_REG_CHIP_ID, &id, 1);
    bsp_cst816_reg_write_byte(CST816_REG_SLEEP, &sleep, 1);
    printf("touch id: 0x%02x\n", id);
}

bool bsp_touch_new_cst816(bsp_touch_interface_t **interface, bsp_touch_info_t *info)
{
    static bsp_touch_interface_t touch_if;
    static bsp_touch_info_t touch_info;

    if (info == NULL) {
        return false;
    }

    memcpy(&touch_info, info, sizeof(touch_info));
    touch_if.init = bsp_cst816_init;
    touch_if.reset = bsp_cst816_reset;
    touch_if.set_rotation = bsp_cst816_set_rotation;
    touch_if.get_rotation = bsp_cst816_get_rotation;
    touch_if.read = bsp_cst816_read;
    touch_if.get_data = bsp_cst816_get_touch_data;

    *interface = &touch_if;
    g_touch_info = &touch_info;
    return true;
}
