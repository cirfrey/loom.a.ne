#include "espy/usb/logging.hpp"
#include "espy/blink.hpp"

#include <cstdarg>

#include "esp_log.h"

#include "tusb.h"

auto espy::usb::logging::do_configuration_descriptor(
    configuration_descriptor_builder_state_t& state,
    cfg_t& cfg,
    std::span<ep_t> eps
) -> void
{
    if (!cfg.cdc) return;

    auto cdc_comm_itf = state.curr_itf_idx++;
    auto cdc_data_itf = state.curr_itf_idx++;

    auto [ep_notif_idx, ep_notif]  = espy::usb::find_unassigned_ep(eps, dir_t::IN, dir_t::INOUT);
    ep_notif->configured_direction = dir_t::IN;
    ep_notif->interface            = cdc_comm_itf;
    ep_notif->interface_type       = itf_t::cdc_comm;

    // NOTE: EP IN and EP OUT share interfaces.
    // --- EP OUT ---
    auto [ep_out_idx, ep_out]    = espy::usb::find_unassigned_ep(eps, dir_t::OUT, dir_t::INOUT);
    ep_out->configured_direction = dir_t::OUT;
    ep_out->interface            = cdc_data_itf;
    ep_out->interface_type       = itf_t::cdc_data;
    // --- EP IN ---
    auto [ep_in_idx, ep_in]     = espy::usb::find_unassigned_ep(eps, dir_t::IN, dir_t::INOUT);
    ep_in->configured_direction = dir_t::IN;
    ep_in->interface            = cdc_data_itf;
    ep_in->interface_type       = itf_t::cdc_data;

    state.append_desc({ TUD_CDC_DESCRIPTOR(
        cdc_comm_itf, // first itfnum (next one is itfnum+1).
        (u8)string_descriptor_idxs::idx_cdc, // stridx.
        (u8)(EP_DIR_IN | ep_notif_idx), // ep_notif.
        8, // ep_notif_size.
        ep_out_idx, // epout.
        (u8)(EP_DIR_IN | ep_in_idx), // epin.
        64 // ep_size.
    )});
}

auto espy::usb::logging::logf(const char *fmt, ...) -> int
{
    va_list args;
    va_start(args, fmt);

    /// TODO: bufsize configurable by macro.
    auto const bufsize = 256;
    char buf[bufsize];
    int len = vsnprintf(buf, bufsize, fmt, args);

    if (len <= 0)
    {
        va_end(args);
        return len;
    }

    // --- CDC Logging ---
    if (tud_cdc_connected())
    {
        const char* TAG = "espy::usb::logging::logf(cdc)";

        uint32_t queued_bytes = tud_cdc_n_write(0, buf, len);
        if (queued_bytes < len) {
            ESP_LOGW(TAG, "FIFO Full: queued %u out of %u bytes while trying to log \"\"\"%s\"\"\"", (unsigned int)queued_bytes, (unsigned int)len, buf);
        }

        uint32_t flushed_bytes = tud_cdc_n_write_flush(0);
        if (flushed_bytes == 0 && len > 0) {
            ESP_LOGE(TAG, "Hardware busy, failed to flush data while trying to log \"\"\"%s\"\"\"", buf);
        }
    }

    // --- HID Vendor Logging (Chunked) ---
    if (tud_hid_ready())
    {
        const int max_payload = 63; // 64 total - 1 for Report ID
        int bytes_sent = 0;

        while (bytes_sent < len) {
            uint8_t report[64] = {0};

            int chunk_size = len - bytes_sent;
            if (chunk_size > max_payload) chunk_size = max_payload;

            memcpy(report, &buf[bytes_sent], chunk_size);

            tud_hid_report((uint8_t)espy::usb::hid_reportid::vendor, report, 63);

            bytes_sent += chunk_size;

            // CRITICAL: HID needs time to clear the buffer.
            while (bytes_sent < len && !tud_hid_ready()) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }

    va_end(args);
    return len;
}


extern "C"
{
    void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
        (void) itf; (void) dtr; (void) rts;
    }
    void tud_cdc_rx_cb(uint8_t itf) { (void) itf; }
}
