#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "bsp_i2c.h"
#include "bsp_qmi8658.h"

#define QMI8658_CHIP_ID 0x05
#define QMI8658_REG_WHO_AM_I 0x00
#define QMI8658_REG_CTRL2 0x03
#define QMI8658_REG_CTRL5 0x06
#define QMI8658_REG_CTRL7 0x08
#define QMI8658_REG_AX_L 0x35
#define QMI8658_I2C_TIMEOUT_US 2000
#define QMI8658_AUDIO_PA_CTRL_PIN 0
#define QMI8658_AUDIO_DOUT_PIN 1
#define QMI8658_AUDIO_DIN_PIN 2
#define QMI8658_AUDIO_MCLK_PIN 3
#define QMI8658_AUDIO_BCLK_PIN 4
#define QMI8658_AUDIO_LRCLK_PIN 5

typedef struct {
    i2c_inst_t *inst;
    uint8_t sda_pin;
    uint8_t scl_pin;
    const char *label;
} qmi8658_bus_candidate_t;

#define QMI8658_ALT_SDA_PIN 4
#define QMI8658_ALT_SCL_PIN 5

static uint8_t qmi8658_addr = 0;
static i2c_inst_t *qmi8658_bus = NULL;
static uint8_t qmi8658_sda_pin = 0;
static uint8_t qmi8658_scl_pin = 0;
static bool qmi8658_ready = false;

static void qmi8658_configure_bus(const qmi8658_bus_candidate_t *candidate)
{
    i2c_deinit(candidate->inst);
    i2c_init(candidate->inst, 100 * 1000);
    gpio_set_function(candidate->sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(candidate->scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(candidate->sda_pin);
    gpio_pull_up(candidate->scl_pin);
    sleep_ms(2);
}

static void qmi8658_restore_audio_pins(void)
{
    gpio_init(QMI8658_AUDIO_PA_CTRL_PIN);
    gpio_set_dir(QMI8658_AUDIO_PA_CTRL_PIN, GPIO_OUT);
    gpio_put(QMI8658_AUDIO_PA_CTRL_PIN, 1);

    gpio_set_function(QMI8658_AUDIO_DOUT_PIN, GPIO_FUNC_PIO1);
    gpio_set_function(QMI8658_AUDIO_DIN_PIN, GPIO_FUNC_PIO1);
    gpio_set_function(QMI8658_AUDIO_MCLK_PIN, GPIO_FUNC_PIO1);
    gpio_set_function(QMI8658_AUDIO_BCLK_PIN, GPIO_FUNC_PIO1);
    gpio_set_function(QMI8658_AUDIO_LRCLK_PIN, GPIO_FUNC_PIO1);
}

static void qmi8658_release_candidate(const qmi8658_bus_candidate_t *candidate)
{
    i2c_deinit(candidate->inst);

    if (candidate->sda_pin <= QMI8658_AUDIO_LRCLK_PIN ||
        candidate->scl_pin <= QMI8658_AUDIO_LRCLK_PIN) {
        qmi8658_restore_audio_pins();
        return;
    }

    if (!(candidate->sda_pin == BSP_I2C_SDA_PIN && candidate->scl_pin == BSP_I2C_SCL_PIN)) {
        gpio_deinit(candidate->sda_pin);
        gpio_deinit(candidate->scl_pin);
    }
}

static bool qmi8658_read_register(i2c_inst_t *inst,
                                  uint8_t device_addr,
                                  uint8_t reg_addr,
                                  uint8_t *buffer,
                                  size_t len)
{
    if (i2c_write_timeout_us(inst, device_addr, &reg_addr, 1, true, QMI8658_I2C_TIMEOUT_US) < 0) {
        return false;
    }
    if (i2c_read_timeout_us(inst, device_addr, buffer, len, false, QMI8658_I2C_TIMEOUT_US) < 0) {
        return false;
    }
    return true;
}

static bool qmi8658_write_register(uint8_t reg_addr, uint8_t value)
{
    uint8_t write_buffer[2] = {reg_addr, value};
    if (qmi8658_bus == NULL) {
        return false;
    }
    return i2c_write_timeout_us(qmi8658_bus,
                                qmi8658_addr,
                                write_buffer,
                                2,
                                false,
                                QMI8658_I2C_TIMEOUT_US) >= 0;
}

static bool qmi8658_probe_address(i2c_inst_t *inst, uint8_t device_addr)
{
    uint8_t chip_id = 0;

    if (!qmi8658_read_register(inst, device_addr, QMI8658_REG_WHO_AM_I, &chip_id, 1)) {
        printf("qmi8658 probe 0x%02x read failed\n", device_addr);
        return false;
    }
    printf("qmi8658 probe 0x%02x who=0x%02x\n", device_addr, chip_id);
    return chip_id == QMI8658_CHIP_ID;
}

bool bsp_qmi8658_init(void)
{
    static const qmi8658_bus_candidate_t bus_candidates[] = {
        {i2c1, 6, 7, "i2c1 gp6/gp7"},
        {i2c0, QMI8658_ALT_SDA_PIN, QMI8658_ALT_SCL_PIN, "i2c0 gp4/gp5"},
        {i2c1, 18, 19, "i2c1 gp18/gp19"},
        {i2c0, 20, 21, "i2c0 gp20/gp21"},
        {i2c1, 22, 23, "i2c1 gp22/gp23"},
        {i2c1, 26, 27, "i2c1 gp26/gp27"},
        {i2c0, 28, 29, "i2c0 gp28/gp29"},
        {i2c0, 0, 1, "i2c0 gp0/gp1 risky"},
        {i2c1, 2, 3, "i2c1 gp2/gp3 risky"},
        {i2c0, 8, 9, "i2c0 gp8/gp9 risky"},
        {i2c1, 10, 11, "i2c1 gp10/gp11 risky"},
        {i2c0, 12, 13, "i2c0 gp12/gp13 risky"},
        {i2c1, 14, 15, "i2c1 gp14/gp15 risky"},
        {i2c0, 16, 17, "i2c0 gp16/gp17 risky"},
        {i2c0, 24, 25, "i2c0 gp24/gp25 risky"},
    };
    static const uint8_t candidate_addrs[] = {0x6A, 0x6B};
    uint8_t chip_id = 0;

    if (qmi8658_ready) {
        return true;
    }

    for (size_t bus_index = 0; bus_index < sizeof(bus_candidates) / sizeof(bus_candidates[0]); ++bus_index) {
        const qmi8658_bus_candidate_t *candidate = &bus_candidates[bus_index];
        uint8_t found_addr = 0;

        printf("qmi8658 probing %s\n", candidate->label);
        qmi8658_configure_bus(candidate);

        for (size_t addr_index = 0; addr_index < sizeof(candidate_addrs) / sizeof(candidate_addrs[0]); ++addr_index) {
            if (qmi8658_probe_address(candidate->inst, candidate_addrs[addr_index])) {
                found_addr = candidate_addrs[addr_index];
                break;
            }
        }

        if (found_addr != 0) {
            qmi8658_bus = candidate->inst;
            qmi8658_addr = found_addr;
            qmi8658_sda_pin = candidate->sda_pin;
            qmi8658_scl_pin = candidate->scl_pin;
            printf("qmi8658 detected on %s at 0x%02x\n", candidate->label, qmi8658_addr);
            break;
        }

        printf("qmi8658 missing on %s\n", candidate->label);
        qmi8658_release_candidate(candidate);
    }

    if (qmi8658_addr == 0 || qmi8658_bus == NULL) {
        bsp_i2c_init();
        qmi8658_restore_audio_pins();
        printf("qmi8658 not found on known i2c buses\n");
        return false;
    }

    if (!qmi8658_read_register(qmi8658_bus, qmi8658_addr, QMI8658_REG_WHO_AM_I, &chip_id, 1)) {
        return false;
    }
    if (!qmi8658_write_register(QMI8658_REG_CTRL7, 0x00)) {
        return false;
    }
    if (!qmi8658_write_register(QMI8658_REG_CTRL2, 0x06)) {
        return false;
    }
    if (!qmi8658_write_register(QMI8658_REG_CTRL5, 0x05)) {
        return false;
    }
    if (!qmi8658_write_register(QMI8658_REG_CTRL7, 0x01)) {
        return false;
    }

    sleep_ms(30);
    printf("qmi8658 ready at 0x%02x, id=0x%02x\n", qmi8658_addr, chip_id);
    qmi8658_ready = true;
    return true;
}

bool bsp_qmi8658_read_data(qmi8658_data_t *data)
{
    uint8_t buffer[6];

    if (data == NULL) {
        return false;
    }
    if (!qmi8658_ready && !bsp_qmi8658_init()) {
        return false;
    }

    for (int retry = 0; retry < 3; ++retry) {
        if (qmi8658_read_register(qmi8658_bus, qmi8658_addr, QMI8658_REG_AX_L, buffer, sizeof(buffer))) {
            data->acc_x = (int16_t)(((uint16_t)buffer[1] << 8) | buffer[0]);
            data->acc_y = (int16_t)(((uint16_t)buffer[3] << 8) | buffer[2]);
            data->acc_z = (int16_t)(((uint16_t)buffer[5] << 8) | buffer[4]);
            data->gyr_x = 0;
            data->gyr_y = 0;
            data->gyr_z = 0;
            return true;
        }
        sleep_ms(5);
    }

    return false;
}

void bsp_qmi8658_deinit(void)
{
    if (qmi8658_bus != NULL) {
        i2c_deinit(qmi8658_bus);
    }

    if (qmi8658_bus == i2c0 ||
        qmi8658_sda_pin == QMI8658_ALT_SDA_PIN ||
        qmi8658_scl_pin == QMI8658_ALT_SCL_PIN) {
        qmi8658_restore_audio_pins();
    }

    qmi8658_addr = 0;
    qmi8658_bus = NULL;
    qmi8658_sda_pin = 0;
    qmi8658_scl_pin = 0;
    qmi8658_ready = false;
    bsp_i2c_init();
}
