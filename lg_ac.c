#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <driver/rmt.h>
#include <driver/timer.h>
#include <esp_log.h>
#include <esp_types.h>

#include <soc/timer_group_struct.h>

#include "lg_ac_ir_config.h"
#include "lg_ac.h"

#define DEFERRED_OFF_INTEVAL   60

#define CODE_OFF    0x88C0051
const uint32_t CODE_TEMPERATURE_ON[] = {
    0x8800347,      /* 18 */
    0x8800448,      /* 19 */
    0x8800549,      /* 20 */
    0x880064A,      /* 21 */
    0x880074B,      /* 22 */
    0x880084C,      /* 23 */
    0x880094D,      /* 24 */
    0x8800A4E,      /* 25 */
    0x8800B4F,      /* 26 */
    0x8800C50,      /* 27 */
    0x8800D51,      /* 28 */
    0x8800E52,      /* 29 */
    0x8800F53,      /* 30 */
};
const uint32_t CODE_TEMPERATURE_SET[] = {
    0x880834F,      /* 18 */
    0x8808400,      /* 19 */
    0x8808541,      /* 20 */
    0x8808642,      /* 21 */
    0x8808743,      /* 22 */
    0x8808844,      /* 23 */
    0x8808945,      /* 24 */
    0x8808A46,      /* 25 */
    0x8808B47,      /* 26 */
    0x8808C48,      /* 27 */
    0x8808D49,      /* 28 */
    0x8808E4A,      /* 29 */
    0x8808F4B,      /* 30 */
};

static xQueueHandle deferred_queue;

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
    gpio_pad_select_gpio(gpio);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);

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

static void IRAM_ATTR _deferred_timer_isr(void *para)
{
    TIMERG0.int_clr_timers.t0 = 1;
    int off = true;
    xQueueSendFromISR(deferred_queue, &off, NULL);
}

static void _deferred_off_timer_stop(void)
{
    timer_pause(TIMER_GROUP_0, TIMER_0);
}

static void _deferred_off_timer_start(void)
{
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_start(TIMER_GROUP_0, TIMER_0);
}

static void _deferred_off_task(void *arg)
{
    while (1) {
        int off;
        xQueueReceive(deferred_queue, &off, portMAX_DELAY);
        if (off) {
            printf("[LG_AC] DEFERRED OFF\n");
            _code_send(CODE_OFF);
        }
    }
}

static void _deferred_timer_init(void)
{
#define TIMER_DIVIDER         16
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = false;
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    timer_isr_register(TIMER_GROUP_0, TIMER_0, _deferred_timer_isr, 
            (void *)TIMER_0, ESP_INTR_FLAG_IRAM, NULL);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, DEFERRED_OFF_INTEVAL * TIMER_SCALE);
}

static void _deferred_off_init(void)
{
    deferred_queue = xQueueCreate(5, sizeof(int));
    _deferred_timer_init();
    xTaskCreate(_deferred_off_task, "lg_ac_deferred_ac_off_task", 2048, NULL, 5, NULL);
}

int lg_ac_temperature_set(int temperature)
{
    printf("[LG_AC] TEMPERATURE SET:%d\n", temperature);

    if (temperature < LG_AC_TEMPERATURE_MIN || temperature > LG_AC_TEMPERATURE_MAX) {
        return -1;
    }

    _deferred_off_timer_stop();
    _code_send(CODE_TEMPERATURE_SET[temperature - LG_AC_TEMPERATURE_MIN]);
    return 0;
}

void lg_ac_off(void)
{
    lg_ac_temperature_set(30);
    _deferred_off_timer_start();
}

void lg_ac_on(void)
{
    _deferred_off_timer_stop();
    _code_send(CODE_TEMPERATURE_ON[0]);
}

void lg_ac_init(int gpio)
{
    _tx_init(gpio, RMT_TX_CHANNEL);
    _deferred_off_init();
}
