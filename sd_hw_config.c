#include "hw_config.h"

static spi_t g_sd_spi = {
    .hw_inst = spi0,
    .sck_gpio = 18,
    .mosi_gpio = 19,
    .miso_gpio = 20,
    .baud_rate = 25 * 1000 * 1000u,
};

static sd_spi_if_t g_sd_spi_if = {
    .spi = &g_sd_spi,
    .ss_gpio = 23,
};

static sd_card_t g_sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &g_sd_spi_if,
};

size_t sd_get_num(void)
{
    return 1;
}

sd_card_t *sd_get_by_num(size_t num)
{
    return num == 0 ? &g_sd_card : NULL;
}
