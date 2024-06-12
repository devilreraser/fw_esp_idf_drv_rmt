/* *****************************************************************************
 * File:   drv_rmt.c
 * Author: XX
 *
 * Created on YYYY MM DD
 * 
 * Description: ...
 * 
 **************************************************************************** */

/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include "drv_rmt.h"

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define FORCE_LEGACY_FOR_RMT_ESP_IDF_VERSION_5    1
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && FORCE_LEGACY_FOR_RMT_ESP_IDF_VERSION_5 == 0
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#else
//#pragma GCC diagnostic ignored "-Wcpp"
#include "driver/rmt.h"
//#pragma GCC diagnostic pop
#endif


/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */
#define TAG "drv_rmt"

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Function-Like Macros
 **************************************************************************** */

/* *****************************************************************************
 * Variables Definitions
 **************************************************************************** */
//static rmt_channel_t tx_channel = RMT_CHANNEL_6;
static rmt_channel_t rx_channel = RMT_CHANNEL_7;

static RingbufHandle_t ring_buffer = NULL;

/* *****************************************************************************
 * Prototype of functions definitions
 **************************************************************************** */

/* *****************************************************************************
 * Functions
 **************************************************************************** */
void drv_rmt_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
}

void drv_rmt_init_rx(void)
{
    rmt_config_t rmt_rx_config = RMT_DEFAULT_CONFIG_RX(CONFIG_DRV_RMT_TEST_RX_GPIO, rx_channel);
    //rmt_rx_config.flags = RMT_CHANNEL_FLAGS_INVERT_SIG;
    rmt_rx_config.rx_config.idle_threshold = 80 * 120;
    rmt_config(&rmt_rx_config);
    rmt_driver_install(rx_channel, 512, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED);

    //get RMT RX ringbuffer
    rmt_get_ringbuf_handle(rx_channel, &ring_buffer);

}

void drv_rmt_start_rx(void)
{
    bool b_reset_rx_pointer = true;
    rmt_rx_start(rx_channel, b_reset_rx_pointer);
}

void drv_rmt_reset_rx(void)
{
    rmt_rx_memory_reset(rx_channel);
    //rmt_memory_rw_rst(rx_channel);

}

void drv_rmt_stop_rx(void)
{
    rmt_rx_stop(rx_channel);
}

void drv_rmt_deinit_rx(void)
{
    rmt_driver_uninstall(rx_channel);
}


size_t drv_rmt_read_rx(uint8_t* pu8_data, size_t max_size, TickType_t timeout)
{
    size_t data_length = 0;
    size_t length = 0;
    rmt_item32_t *items = NULL;

    // //if (timeout) vTaskDelay(timeout);
    // #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
    // if (timeout) esp_rom_delay_us(pdTICKS_TO_MS(timeout)*1000); //v.5.0.4
    // #else
    // if (timeout) ets_delay_us(pdTICKS_TO_MS(timeout)*1000);   //v.4.4.3
    // #endif

    items = (rmt_item32_t *) xRingbufferReceive(ring_buffer, &length, timeout);

    if (items) 
    {
        ESP_LOGD(TAG, "xRingbufferReceive %d bytes", length);
        if (timeout)
        {
            length /= 4; // one RMT = 4 Bytes
            ESP_LOGD(TAG, "xRingbufferReceive %d rmt items", length);
            for (int index = 0; index < length; index++)
            {
                ESP_LOGD(TAG, "[%2d] | (%d)=%3d (%d)=%3d", index, items[index].level0, items[index].duration0, items[index].level1, items[index].duration1);
            }

            if(pu8_data != NULL)
            {
                if ((length - 3) == 40)
                {

                    for (int index = 0; index < length; index++)
                    {
                        if(items[index+2].duration0 >= 50)   //28 for 0 | 70 for 1
                        {
                            /* Bit received was a 1 */
                            pu8_data[index/8] |= (1 << (7-(index%8)));
                        }
                    }
                    data_length = 40/8;



                }
            }
        }

        
        // if (ir_parser->input(ir_parser, items, length) == ESP_OK) {
        //     if (ir_parser->get_scan_code(ir_parser, &addr, &cmd, &repeat) == ESP_OK) {
        //         ESP_LOGI(TAG, "Scan Code %s --- addr: 0x%04x cmd: 0x%04x", repeat ? "(repeat)" : "", addr, cmd);
        //     }
        // }

        //after parsing the data, return spaces to ringbuffer.
        vRingbufferReturnItem(ring_buffer, (void *) items);
    }

    return data_length;
}


