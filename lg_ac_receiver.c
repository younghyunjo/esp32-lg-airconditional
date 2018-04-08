#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/rmt.h>

#include "lg_ac_ir_config.h"

static const char* TAG = "LG_AC_RECEIVER";

inline bool _check_in_range(int duration_ticks, int target_us, int margin_us)
{
    if(( LG_ITEM_DURATION(duration_ticks) < (target_us + margin_us))
        && ( LG_ITEM_DURATION(duration_ticks) > (target_us - margin_us))) {
        return true;
    } else {
        return false;
    }
}

static bool _is_header(rmt_item32_t* item)
{
    if((item->level0 == RMT_RX_ACTIVE_LEVEL && item->level1 != RMT_RX_ACTIVE_LEVEL)
        && _check_in_range(item->duration0, LG_HEADER_HIGH_US, LG_BIT_MARGIN)
        && _check_in_range(item->duration1, LG_HEADER_LOW_US, LG_BIT_MARGIN)) {
        return true;
    }
    return false;
}

static bool _is_bit_one(rmt_item32_t* item)
{
    if((item->level0 == RMT_RX_ACTIVE_LEVEL && item->level1 != RMT_RX_ACTIVE_LEVEL)
        && _check_in_range(item->duration0, LG_BIT_ONE_HIGH_US, LG_BIT_MARGIN)
        && _check_in_range(item->duration1, LG_BIT_ONE_LOW_US, LG_BIT_MARGIN)) {
        return true;
    }
    return false;
}

static bool _is_bit_zero(rmt_item32_t* item)
{
    if((item->level0 == RMT_RX_ACTIVE_LEVEL && item->level1 != RMT_RX_ACTIVE_LEVEL)
        && _check_in_range(item->duration0, LG_BIT_ZERO_HIGH_US, LG_BIT_MARGIN)
        && _check_in_range(item->duration1, LG_BIT_ZERO_LOW_US, LG_BIT_MARGIN)) {
        return true;
    }
    return false;
}


static int _ir_decode(rmt_item32_t* item, int item_num, uint32_t* code)
{
    int w_len = item_num;
    if(w_len < LG_DATA_ITEM_NUM) {
        return -1;
    }

    int i = 0, j = 0;
    if(!_is_header(item++)) {
        ESP_LOGE(TAG, "invalid header");
        return -1;
    }

    uint32_t data = 0;
    for(j = 0; j < 28; j++) {
        if(_is_bit_one(item)) {
            data = (data << 1) | 1;
        }
        else if(_is_bit_zero(item)) {
            data = (data << 1) | 0;
        }
        else
            return -1;
        item++;
        i++;
    }

    *code = data;
    return i;
}

static void _rx_init(int gpio, int channel)
{
    rmt_config_t rmt_rx;
    rmt_rx.channel = channel;
    rmt_rx.gpio_num = gpio;
    rmt_rx.clk_div = RMT_CLK_DIV;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = true;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = RMT_ITEM32_TIMEOUT_US / 10 * (RMT_TICK_10_US);
    rmt_config(&rmt_rx);
    rmt_driver_install(rmt_rx.channel, 1000, 0);
}

static void _rx_task(void* arg)
{
    int rx_gpio = (int)arg;

    ESP_LOGI(TAG, "RX TASK STARTED\n");
    int channel = RMT_RX_CHANNEL;
    _rx_init(rx_gpio, channel);
    RingbufHandle_t rb = NULL;

    rmt_get_ringbuf_handle(channel, &rb);
    xRingbufferPrintInfo(rb);
    rmt_rx_start(channel, 1);

    while(rb) {
        size_t rx_size = 0;
        rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, 1000);
        if(item) {
            int offset = 0;

            while (offset < rx_size/4) {
                uint32_t code = 0;
                int res = _ir_decode(item + offset, rx_size / 4 - offset, &code);
                if(res > 0) {
                    offset += (res + 2); // add 'start' and 'end' bit
                    ESP_LOGI(TAG, "RMT RCV --- code: 0x%x res:%d", code, res);
                } else {
                    ESP_LOGE(TAG, "decoding failed");
                    break;
                }
            }

            vRingbufferReturnItem(rb, (void*) item);
        }
    }
    vTaskDelete(NULL);
}

void lg_ac_receiver_start(int gpio)
{
    xTaskCreate(_rx_task, "lgac_rx_task", 2048, (void*)gpio, 10, NULL);
}
