#include <string.h>
#include "pico/stdlib.h"
#include "bsp_i2c.h"

static void bsp_i2c_recover_bus(void)
{
    i2c_deinit(BSP_I2C_NUM);

    gpio_init(BSP_I2C_SDA_PIN);
    gpio_init(BSP_I2C_SCL_PIN);
    gpio_pull_up(BSP_I2C_SDA_PIN);
    gpio_pull_up(BSP_I2C_SCL_PIN);

    gpio_set_dir(BSP_I2C_SDA_PIN, GPIO_IN);
    gpio_set_dir(BSP_I2C_SCL_PIN, GPIO_OUT);
    gpio_put(BSP_I2C_SCL_PIN, 1);
    sleep_us(10);

    for (int i = 0; i < 9; ++i) {
        gpio_put(BSP_I2C_SCL_PIN, 0);
        sleep_us(5);
        gpio_put(BSP_I2C_SCL_PIN, 1);
        sleep_us(5);
    }
}

bool bsp_i2c_probe(uint8_t device_addr)
{
    uint8_t dummy = 0;
    return i2c_read_blocking(BSP_I2C_NUM, device_addr, &dummy, 1, false) >= 0;
}

size_t bsp_i2c_scan(uint8_t *addresses, size_t max_addresses)
{
    size_t count = 0;

    for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
        if (bsp_i2c_probe(addr)) {
            if (addresses != NULL && count < max_addresses) {
                addresses[count] = addr;
            }
            count++;
        }
    }

    return count;
}

void bsp_i2c_write(uint8_t device_addr, uint8_t *buffer, size_t len)
{
    i2c_write_blocking(BSP_I2C_NUM, device_addr, buffer, len, false);
}

void bsp_i2c_write_reg8(uint8_t device_addr, uint8_t reg_addr, uint8_t *buffer, size_t len)
{
    uint8_t write_buffer[len + 1];
    write_buffer[0] = reg_addr;
    memcpy(write_buffer + 1, buffer, len);
    i2c_write_blocking(BSP_I2C_NUM, device_addr, write_buffer, len + 1, false);
}

void bsp_i2c_read_reg8(uint8_t device_addr, uint8_t reg_addr, uint8_t *buffer, size_t len)
{
    i2c_write_blocking(BSP_I2C_NUM, device_addr, &reg_addr, 1, true);
    i2c_read_blocking(BSP_I2C_NUM, device_addr, buffer, len, false);
}

void bsp_i2c_write_reg16(uint8_t device_addr, uint16_t reg_addr, uint8_t *buffer, size_t len)
{
    uint8_t write_buffer[len + 2];
    write_buffer[0] = (uint8_t)(reg_addr >> 8);
    write_buffer[1] = (uint8_t)reg_addr;
    memcpy(write_buffer + 2, buffer, len);
    i2c_write_blocking(BSP_I2C_NUM, device_addr, write_buffer, len + 2, false);
}

void bsp_i2c_read_reg16(uint8_t device_addr, uint16_t reg_addr, uint8_t *buffer, size_t len)
{
    uint8_t write_buffer[2];
    write_buffer[0] = (uint8_t)(reg_addr >> 8);
    write_buffer[1] = (uint8_t)reg_addr;
    i2c_write_blocking(BSP_I2C_NUM, device_addr, write_buffer, 2, true);
    i2c_read_blocking(BSP_I2C_NUM, device_addr, buffer, len, false);
}

void bsp_i2c_init(void)
{
    bsp_i2c_recover_bus();
    i2c_init(BSP_I2C_NUM, 100 * 1000);
    gpio_set_function(BSP_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(BSP_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(BSP_I2C_SDA_PIN);
    gpio_pull_up(BSP_I2C_SCL_PIN);
}
