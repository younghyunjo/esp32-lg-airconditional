#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/rmt.h>

#include "lg_ac_ir_config.h"

#define CODE_OFF    0x88C0051
#define CODE_ON_18  0x8800347
#define CODE_ON_19  0x8800448
#define CODE_ON_20  0x8800549
#define CODE_ON_21  0x880064A
#define CODE_ON_22  0x880074B
#define CODE_ON_23  0x880084C
#define CODE_ON_24  0x880094D
#define CODE_ON_25  0x8800A4E
#define CODE_ON_26  0x8800B4F
#define CODE_ON_27  0x8800C50
#define CODE_ON_28  0x8800D51
#define CODE_ON_29  0x8800E52
#define CODE_ON_30  0x8800F53

#define TEMPERATURE_MIN 18
#define TEMPERATURE_MAX 30

const uint32_t TEMPERATURE_CODE[] = {
    0x880834F
    0x8808400
    0x8808541
    0x8808642
    0x8808743
    0x8808844
    0x8808945
    0x8808A46
    0x8808B47
    0x8808C48
    0x8808D49
    0x8808E4A
    0x8808F4B
};

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

void lg_ac_temperature_set(int temperature)
{
    if (temperature < TEMPERATURE_MIN || temperature > TEMPERATURE_MAX) {
        return;
    }

    uint32_t code = TEMPERATURE_CODE(temperature - TEMPERATURE_MIN);
    _code_send(code);
}

void lg_ac_off(void)
{
    _code_send(CODE_OFF);
}

void lg_ac_on(void)
{
    _code_send(CODE_ON_18);
}

void lg_ac_init(int gpio)
{
    _tx_init(gpio, RMT_TX_CHANNEL);
}
