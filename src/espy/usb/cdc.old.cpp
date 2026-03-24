#include "espy/usb/cdc.hpp"

#include "espy/usb/tusb_config.h"
#include "sdkconfig.h"

#include <stdint.h>

#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_cdc_acm.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace espy::usb::cdc
{
    static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

    static QueueHandle_t rx_queue;
    typedef struct {
        uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];     // Data buffer
        size_t buf_len;                                     // Number of bytes received
        uint8_t itf;                                        // Index of CDC device interface
    } rx_message_t;

    auto rx_callback(int itf, cdcacm_event_t*) -> void;
    auto line_state_changed_callback(int, cdcacm_event_t*) -> void;

    template <uint32_t BufSize = 256>
    int printf_to_cdc(const char* fmt, va_list args);

    auto task_echo_incoming(void* tparams) -> void;
}

auto espy::usb::cdc::init(const init_settings& settings) -> void
{
    rx_queue = xQueueCreate(5, sizeof(rx_message_t));
    assert(rx_queue);

    tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = &rx_callback, // the first way to register a callback
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };

    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));
    /* the second way to register a callback */
    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
                        TINYUSB_CDC_ACM_0,
                        CDC_EVENT_LINE_STATE_CHANGED,
                        &line_state_changed_callback));


    ESP_LOGI("espy::usb::cdc::init", "CDC initialization DONE");

    if(settings.forward_log_to_cdc) { esp_log_set_vprintf(printf_to_cdc); }

    if(settings.echo_incoming) {
        xTaskCreate(
            espy::usb::cdc::task_echo_incoming,
            "espy::usb::cdc::task_echo_incoming",
            2048,            // Stack size in words
            NULL,            // Parameter passed into the task
            1,               // Priority
            NULL             // Task handle
        );
    }
}

auto espy::usb::cdc::rx_callback(int itf, cdcacm_event_t *event) -> void
{
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret = tinyusb_cdcacm_read((tinyusb_cdcacm_itf_t)itf, rx_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {

        rx_message_t rx_msg = {
            .buf_len = rx_size,
            .itf = (tinyusb_cdcacm_itf_t)itf,
        };

        memcpy(rx_msg.buf, rx_buf, rx_size);
        xQueueSend(rx_queue, &rx_msg, 0);
    } else {
        ESP_LOGE("espy::usb::cdc::rx_callback", "Read Error");
    }
}

auto espy::usb::cdc::line_state_changed_callback(int itf, cdcacm_event_t *event) -> void
{
    int dtr = event->line_state_changed_data.dtr;
    int rts = event->line_state_changed_data.rts;
    ESP_LOGI("espy::usb::cdc::line_state_changed_callback", "Line state changed on channel %d: DTR:%d, RTS:%d", itf, dtr, rts);
}

auto espy::usb::logging::logf(const char *fmt, va_list args) -> int
{
    /// TODO: bufsize configurable by macro.
    char buf[256];
    int len = vsnprintf(buf, BufSize, fmt, args);

    if (len <= 0) return len;

    // Try to log to CDC.
    if (tud_cdc_connected()) {
        tinyusb_cdcacm_itf_t itf = tinyusb_cdcacm_itf_t::TINYUSB_CDC_ACM_0;

        // 1. Queue the data into the buffer
        size_t queued_bytes = tinyusb_cdcacm_write_queue(itf, (uint8_t*)buf, len);
        if (queued_bytes < len) {
            ESP_LOGW(TAG, "Not all data was queued, queued %d out of %d bytes", queued_bytes, len);
        }

        // 2. Flush the buffer to send the data over USB
        // A timeout of 0 will attempt to flush immediately. Consider using a small timeout if needed.
        esp_err_t err = tinyusb_cdcacm_write_flush(itf, pdMS_TO_TICKS(1));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to flush data: %d", err);
        } else {
            ESP_LOGI(TAG, "Data sent successfully");
        }
    }

    // Try to log to HID vendor.
    /// TODO: split message if too large.
    if (tud_hid_ready()) {
        uint8_t report[64] = {0};
        report[0] = (uint8_t)espy::usb::hid_reportid::vendor; // Report ID

        // Copy string into report (leaving room for ID and null terminator)
        strncpy((char*)&report[1], str, 63);

        // Send the report
        tud_hid_report( (uint8_t)espy::usb::hid_reportid::vendor, report + 1, 63);
    }

    return len;
}

auto espy::usb::cdc::task_echo_incoming(void* tparams) -> void {
    rx_message_t msg;

    while(true)
    {
        if (!xQueueReceive(rx_queue, &msg, portMAX_DELAY)) continue;
        if (!msg.buf_len) continue;

        /* Print received data*/
        ESP_LOGI("espy::usb::cdc::task_echo_incoming", "Data from channel %d:", msg.itf);
        ESP_LOG_BUFFER_HEXDUMP("espy::usb::cdc::task_echo_incoming", msg.buf, msg.buf_len, ESP_LOG_INFO);
    }

    vTaskDelete(NULL);
}
