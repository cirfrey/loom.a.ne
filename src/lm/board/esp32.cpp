#include "lm/board.hpp"

#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_mac.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/temperature_sensor.h>

#include <cstdio>

namespace lm::board
{
    static temperature_sensor_handle_t temperature_sensor_handle = nullptr;
    static char const banner_art[] = R"(
> .    . ___  .         ___   .        ___      .     ___
>       /\  \     .    /\  \       .  /\  \          /\  \         .
>    . /::\  \ .      /::\  \        /::\  \   .    /::\  \
>     /:/\:\  \   .  /:/\:\  \      /:/\:\  \      /:/\:\  \
>    /:/  \:\  \    /:/  \:\  \ .  /:/  \:\  \    /:/  \:\  \
>   /:/  / \:\  \  /:/  / \:\  \  /:/  / \:\  \  /:/  / \:\  \    .
>   \:\  \ /:/  / .\:\  \ /:/  /  \:\  \ /:/  /  \:\  \ /:/  /
>   .\:\  /:/  /    \:\  /:/  /    \:\  /:/  /    \:\  /:/  / .
>     \:\/:/  /  .   \:\/:/  /      \:\/:/  /  .   \:\/:/  /         .
>    . \::/  / .    . \::/  /   .    \::/  /        \::/  /
> .     \/__/   .     .\/__/          \/__/          \/__/
)";
}

// --- General ---

namespace lm::board::general
{

    auto reboot() -> void { esp_restart(); }

    // Place for global hardware init that isn't task-specific
    // e.g., initializing flash, NVS, or global bus power pins.
    auto init_hardware() -> void
    {
        temperature_sensor_config_t temp_sensor =
        {
            .range_min = 20,
            .range_max = 50
        };
        temperature_sensor_install(&temp_sensor, &temperature_sensor_handle);
    }

    auto get_uptime() -> u64
    {
        // This cast should only be a problem after a couple hundred thousand years.
        // I don't think we should worry about it.
        return (u64)esp_timer_get_time();
    }
}

namespace lm::board::misc
{
    auto get_pretty_name() -> char const*
    {
        static const auto chip_model = [](){
            esp_chip_info_t info;
            esp_chip_info(&info);
            return info.model;
        }();

        if(chip_model == CHIP_ESP32S2) return "ESP32-S2 XTensa LX7 32bit 1c@240MHz";
        /*else*/                       return "ESP32-S3 XTensa LX7 32bit 2c@240MHz";
    }

    auto get_mac_str() -> char const*
    {
        static char mac_str[18];
        static bool initialized = false;

        if (!initialized) {
            u8 mac_raw[6];
            esp_efuse_mac_get_default(mac_raw);

            std::snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac_raw[0], mac_raw[1], mac_raw[2],
            mac_raw[3], mac_raw[4], mac_raw[5]);
            initialized = true;
        }

        return mac_str;
    }

    auto get_banner_art() -> banner_art_t { return { .data = banner_art + 1, .len = sizeof(banner_art) - 2 }; }
}

// --- Console ---
namespace lm::board::console
{

    auto init_console(u32 baud_rate) -> void
    {
        const uart_config_t uart_config = {
            .baud_rate = (int)baud_rate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        // Install driver and apply configuration
        uart_driver_install(UART_NUM_1, 1024 * 2, 0, 0, NULL, 0);
        uart_param_config(UART_NUM_1, &uart_config);
        uart_set_pin(UART_NUM_1, GPIO_NUM_17, GPIO_NUM_18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }

    auto console_write(void const* data, u32 len) -> void { uart_write_bytes(UART_NUM_1, data, len); }
}


// --- Memory ---
namespace lm::board::mem
{
    auto get_ram() -> u32
    {
        // Returns total DRAM (Internal Data RAM)
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        return info.total_free_bytes + info.total_allocated_bytes;
    }

    auto get_free_ram() -> u32  { return esp_get_free_heap_size(); }

    auto get_peak_ram() -> u32
    {
        // High water mark of heap usage is calculated as:
        // Total Size - Minimum Free Ever Seen
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        return (info.total_free_bytes + info.total_allocated_bytes) - esp_get_minimum_free_heap_size();
    }

    auto get_largest_free_ram_block() -> u32 { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
}


namespace lm::board::gpio
{
    auto init_pin(pin_t p, pin_mode mode) -> void
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << p),
            .mode         = (mode == pin_mode::output) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
            .pull_up_en   = (mode == pin_mode::input_pullup) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
            .pull_down_en = (mode == pin_mode::input_pulldown) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE // Enabled specifically via attach_interrupt
        };
        gpio_config(&io_conf);
    }

    auto get_pin(pin_t p)             -> bool { return gpio_get_level((gpio_num_t)p); }
    auto set_pin(pin_t p, bool level) -> void { gpio_set_level((gpio_num_t)p, level); }

    auto attach_interrupt(pin_t p, interrupt_edge edge, isr_handler_t handler, void* arg) -> void
    {
        // Ensure the ISR service is installed (it's safe to call multiple times)
        gpio_install_isr_service(0);

        gpio_int_type_t intr_type;
        switch(edge) {
            case interrupt_edge::rising:  intr_type = GPIO_INTR_POSEDGE; break;
            case interrupt_edge::falling: intr_type = GPIO_INTR_NEGEDGE; break;
            case interrupt_edge::any:     intr_type = GPIO_INTR_ANYEDGE; break;
            default:                      intr_type = GPIO_INTR_DISABLE; break;
        }

        gpio_set_intr_type((gpio_num_t)p, intr_type);
        gpio_isr_handler_add((gpio_num_t)p, handler, arg);
    }

    auto detach_interrupt(pin_t p) -> void
    {
        gpio_set_intr_type((gpio_num_t)p, GPIO_INTR_DISABLE);
        gpio_isr_handler_remove((gpio_num_t)p);
    }
}

namespace lm::board::cpu
{
    auto get_physical_core(lm::task::affinity aff) -> int
    {
        static const auto chip_model = [](){
            esp_chip_info_t info;
            esp_chip_info(&info);
            return info.model;
        }();

        switch(aff) {
            case lm::task::affinity::core_any:        return -1; // We'll map -1 to tskNO_AFFINITY in task.cpp
            case lm::task::affinity::core_processing: return (chip_model == CHIP_ESP32S2) ? 0 : 1;
            case lm::task::affinity::core_wifi:       return 0;
            default:                                  return 0;
        }
    }

    auto get_core_count() -> u8
    {
        static const auto chip_cores = [](){
            esp_chip_info_t info;
            esp_chip_info(&info);
            return info.cores;
        }();
        return chip_cores;
    }

    auto get_cpu_temperature() -> f32
    {
        temperature_sensor_enable(temperature_sensor_handle);
        float temp;
        temperature_sensor_get_celsius(temperature_sensor_handle, &temp);
        // Disable the temperature sensor if it is not needed to save power.
        temperature_sensor_disable(temperature_sensor_handle);

        return temp;
    }
}
