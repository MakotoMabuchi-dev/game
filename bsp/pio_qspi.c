#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pio_qspi.h"
#include "pio_qspi.pio.h"

static uint pio_qspi_sm;
static int pio_qspi_dma_chan;

static inline void qspi_program_init(PIO pio, uint sm, uint offset, uint sclk_pin, uint d0_pin, float div)
{
    pio_sm_config c = qspi_program_get_default_config(offset);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_sideset_pins(&c, sclk_pin);
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(pio, sclk_pin);
    pio_gpio_init(pio, d0_pin + 0);
    pio_gpio_init(pio, d0_pin + 1);
    pio_gpio_init(pio, d0_pin + 2);
    pio_gpio_init(pio, d0_pin + 3);

    gpio_pull_up(sclk_pin);
    gpio_pull_up(d0_pin + 0);
    gpio_pull_up(d0_pin + 1);
    gpio_pull_up(d0_pin + 2);
    gpio_pull_up(d0_pin + 3);

    pio_sm_set_consecutive_pindirs(pio, sm, sclk_pin, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, d0_pin, 4, true);
    sm_config_set_out_pins(&c, d0_pin, 4);
    sm_config_set_out_shift(&c, false, true, 8);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static void pio_qspi_dma_init(void)
{
    pio_qspi_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(pio_qspi_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(QSPI_PIO, pio_qspi_sm, true));
    dma_channel_configure(
        pio_qspi_dma_chan,
        &c,
        &QSPI_PIO->txf[pio_qspi_sm],
        NULL,
        0,
        false);
}

void pio_qspi_1bit_write_data_blocking(uint8_t *buf, size_t len)
{
    uint8_t cmd_buf[4 * len];

    for (size_t j = 0; j < len; j++) {
        for (int i = 0; i < 4; i++) {
            uint8_t bit1 = (buf[j] & (1u << (2 * i))) ? 1u : 0u;
            uint8_t bit2 = (buf[j] & (1u << (2 * i + 1))) ? 1u : 0u;
            cmd_buf[j * 4 + 3 - i] = bit1 | (uint8_t)(bit2 << 4);
        }
    }

    for (size_t i = 0; i < 4 * len; i++) {
        while (pio_sm_is_tx_fifo_full(QSPI_PIO, pio_qspi_sm)) {
            tight_loop_contents();
        }
        *(volatile uint8_t *)&QSPI_PIO->txf[pio_qspi_sm] = cmd_buf[i];
    }

    while (pio_sm_is_tx_fifo_full(QSPI_PIO, pio_qspi_sm)) {
        tight_loop_contents();
    }
}

void pio_qspi_4bit_write_data_blocking(uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        while (pio_sm_is_tx_fifo_full(QSPI_PIO, pio_qspi_sm)) {
            tight_loop_contents();
        }
        *(volatile uint8_t *)&QSPI_PIO->txf[pio_qspi_sm] = buf[i];
    }
}

void pio_qspi_4bit_write_data(uint8_t *buf, size_t len)
{
    dma_channel_set_trans_count(pio_qspi_dma_chan, len, false);
    dma_channel_set_read_addr(pio_qspi_dma_chan, buf, true);
}

void pio_qspi_init(uint sclk_pin, uint d0_pin, uint32_t baudrate, channel_irq_callback_t irq_cb)
{
    float div = (float)clock_get_hz(clk_sys) / (float)baudrate / 2.0f;
    if (div < 1.0f) {
        div = 1.0f;
    }

    pio_qspi_sm = pio_claim_unused_sm(QSPI_PIO, true);
    uint pio_qspi_offset = pio_add_program(QSPI_PIO, &qspi_program);
    qspi_program_init(QSPI_PIO, pio_qspi_sm, pio_qspi_offset, sclk_pin, d0_pin, div);
    pio_qspi_dma_init();

    if (irq_cb != NULL) {
        bsp_dma_channel_irq_add(1, pio_qspi_dma_chan, irq_cb);
    }
}
