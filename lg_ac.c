#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/rmt.h>

#include "lg_ac_ir_config.h"

static inline void _fill_item_level(rmt_item32_t* item, int high_us, int low_us)
{
    item->level0 = 1;
    item->duration0 = (high_us) / 10 * RMT_TICK_10_US;
    item->level1 = 0;
    item->duration1 = (low_us) / 10 * RMT_TICK_10_US;
}

static void _fill_item_header(rmt_item32_t* item)
{
    _fill_item_level(item, LG_HEADER_HIGH_US, LG_HEADER_LOW_US);
}

static void _fill_item_bit_one(rmt_item32_t* item)
{
    _fill_item_level(item, LG_BIT_ONE_HIGH_US, LG_BIT_ONE_LOW_US);
}

static void _fill_item_bit_zero(rmt_item32_t* item)
{
    _fill_item_level(item, LG_BIT_ZERO_HIGH_US, LG_BIT_ZERO_LOW_US);
}

static void _fill_item_end(rmt_item32_t* item)
{
    _fill_item_level(item, LG_BIT_END, 0x7fff);
}


static void _ir_encode(rmt_item32_t* item, uint32_t code) 
{
    _fill_item_header(item++);

    for (unsigned long  mask = 1UL << (LG_CODE_NBITS - 1);  mask;  mask >>= 1) {
        if (code & mask)
            _fill_item_bit_one(item);
        else
            _fill_item_bit_zero(item);
        item++;
    }

    _fill_item_end(item);
}

static void _code_send(uint32_t code)
{
    rmt_item32_t item[LG_DATA_ITEM_NUM];
    memset(item, 0, sizeof(item));
    _ir_encode(item, code);
    rmt_write_items(RMT_TX_CHANNEL, item, LG_DATA_ITEM_NUM, true);
    rmt_wait_tx_done(RMT_TX_CHANNEL, portMAX_DELAY);
}

static void _tx_init(int gpio, int channel)
{
    rmt_config_t rmt_tx;
    rmt_tx.channel = channel;
    rmt_tx.gpio_num = gpio;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_duty_percent = 50;
    rmt_tx.tx_config.carrier_freq_hz = 38000;
    rmt_tx.tx_config.carrier_level = 1;
    rmt_tx.tx_config.carrier_en = RMT_TX_CARRIER_EN;
    rmt_tx.tx_config.idle_level = 0;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = 0;
    rmt_config(&rmt_tx);
    rmt_driver_install(rmt_tx.channel, 0, 0);
}

void lg_ac_init(int gpio)
{
    _tx_init(gpio, RMT_TX_CHANNEL);
}
