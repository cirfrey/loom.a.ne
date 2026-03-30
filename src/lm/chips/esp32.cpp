#include "lm/chip/types.hpp"
#include "lm/chip/info.hpp"
#include "lm/chip/system.hpp"
#include "lm/chip/time.hpp"
#include "lm/chip/gpio.hpp"
#include "lm/chip/uart.hpp"
#include "lm/chip/memory.hpp"
#include "lm/chip/sensor.hpp"
#include "lm/chip/usb.hpp"

#include "lm/core/types.hpp"

#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_mac.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/temperature_sensor.h>

#include <cstdio>



/* --- chip::info --- */

auto lm::chip::info::name() -> text
{
    return text::from("Bob"); // Temp for now.
}

auto lm::chip::info::full_name() -> text
{
    static const auto chip_model = [](){
        esp_chip_info_t info;
        esp_chip_info(&info);
        return info.model;
    }();

    if(chip_model == CHIP_ESP32S2) return text::from("ESP32-S2 XTensa LX7 32bit 1c@240MHz");
    /*else*/                       return text::from("ESP32-S3 XTensa LX7 32bit 2c@240MHz");
}

auto lm::chip::info::uuid() -> text
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

    return { .data = mac_str, .size = sizeof(mac_str) - 1 };
}

auto lm::chip::info::banner() -> text
{
    static constexpr text banner = text::from(R"(
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
)");
    return { banner.data + 1, banner.size - 1 }; // Skip the first \n.
}



/* --- chip::system --- */

namespace lm::chip::system
{
    static temperature_sensor_handle_t temperature_sensor_handle = nullptr;
}
auto lm::chip::system::init() -> void
{
    temperature_sensor_config_t temp_sensor =
    {
        .range_min = 20,
        .range_max = 50
    };
    temperature_sensor_install(&temp_sensor, &temperature_sensor_handle);
}

auto lm::chip::system::reboot() -> void { esp_restart(); }

auto lm::chip::system::core_count() -> st
{
    static const auto chip_cores = [](){
        esp_chip_info_t info;
        esp_chip_info(&info);
        return info.cores;
    }();
    return chip_cores;
}



/* --- chip::time --- */

auto lm::chip::time::uptime() -> u64
{
    // This cast should only be a problem after a couple hundred thousand years.
    // I don't think we should worry about it.
    return (u64)esp_timer_get_time();
}



/* --- chip::gpio --- */

auto lm::chip::gpio::init(pin p, pin_mode mode) -> void
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << p),
        .mode         = (mode == pin_mode::output)
            ? GPIO_MODE_OUTPUT     : GPIO_MODE_INPUT,
        .pull_up_en   = (mode == pin_mode::input_pullup)
            ? GPIO_PULLUP_ENABLE   : GPIO_PULLUP_DISABLE,
        .pull_down_en = (mode == pin_mode::input_pulldown)
            ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE, // Enabled specifically via attach_interrupt
    };
    gpio_config(&io_conf);
}
auto lm::chip::gpio::get(pin p)             -> bool { return gpio_get_level((gpio_num_t)p);  }
auto lm::chip::gpio::set(pin p, bool level) -> void { gpio_set_level((gpio_num_t)p, level); }

auto lm::chip::gpio::attach_interrupt(
    pin p,
    interrupt_edge edge,
    isr_handler handler,
    void* arg
) -> void
{
    gpio_install_isr_service(0); /// TODO: should we pass any flags here?

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
auto lm::chip::gpio::detach_interrupt(pin p) -> void
{
    gpio_set_intr_type((gpio_num_t)p, GPIO_INTR_DISABLE);
    gpio_isr_handler_remove((gpio_num_t)p);
}



/* --- chip::uart --- */

auto lm::chip::uart::port_count() -> uart_port { return 3; }
auto lm::chip::uart::max_port()   -> uart_port { return 2; }

auto lm::chip::uart::init(uart_port port, pin tx, pin rx, st baud_rate) -> void
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
    uart_driver_install((uart_port_t)port, 1024 * 2, 0, 0, NULL, 0);
    uart_param_config((uart_port_t)port, &uart_config);
    uart_set_pin((uart_port_t)port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

auto lm::chip::uart::write(uart_port port, view data) -> void
{ uart_write_bytes((uart_port_t)port, data.data, data.size); }



/* --- chip::memory --- */

auto lm::chip::memory::total() -> st
{
    // Returns total DRAM (Internal Data RAM)
    static st total = []() -> st {
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        return (info.total_free_bytes + info.total_allocated_bytes);
    }();
    return total;
}

auto lm::chip::memory::free() -> st
{
    return esp_get_free_heap_size();
}

auto lm::chip::memory::peak_used() -> st
{
    // High water mark of heap usage is calculated as:
    // Total Size - Minimum Free Ever Seen
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return memory::total() - info.minimum_free_bytes;
}

auto lm::chip::memory::largest_free_block() -> st
{ return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }



/* --- chip::sensor --- */

auto lm::chip::sensor::internal_temperature() -> f32
{
    temperature_sensor_enable(system::temperature_sensor_handle);
    float temp;
    temperature_sensor_get_celsius(system::temperature_sensor_handle, &temp);
    // Disable the temperature sensor if it is not needed to save power.
    temperature_sensor_disable(system::temperature_sensor_handle);

    return temp;
}



/* --- chip::usb --- */
#include <esp_private/usb_phy.h>

auto lm::chip::usb::phy::power_up() -> void
{
    static usb_phy_handle_t phy_hdl;
        usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,      // Use the OTG controller
        .target = USB_PHY_TARGET_INT,        // Internal PHY pins (GPIO 19/20)
        .otg_mode = USB_OTG_MODE_DEVICE,     // Explicitly set as a Device
        .otg_speed = USB_PHY_SPEED_FULL,     // 12 Mbps
        .ext_io_conf = nullptr,              // Not using external PHY
        .otg_io_conf = nullptr               // Use default IO pins
    };
    usb_new_phy(&phy_conf, &phy_hdl);
}
