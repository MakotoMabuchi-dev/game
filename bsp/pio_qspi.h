#ifndef PIO_QSPI_H
#define PIO_QSPI_H

#include <stddef.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "bsp_dma_channel_irq.h"

#define QSPI_PIO pio2

void pio_qspi_init(uint sclk_pin, uint d0_pin, uint32_t baudrate, channel_irq_callback_t irq_cb);
void pio_qspi_1bit_write_data_blocking(uint8_t *buf, size_t len);
void pio_qspi_4bit_write_data_blocking(uint8_t *buf, size_t len);
void pio_qspi_4bit_write_data(uint8_t *buf, size_t len);

#endif
