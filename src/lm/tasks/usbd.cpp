#include "lm/tasks/usbd.hpp"

#include "lm/chip/usb.hpp"
#include "lm/usbd/esp32.hpp"

#include <tusb.h>

lm::tasks::usbd::usbd(fabric::task_runtime_info& info)
{
    cfg = {

    };
    endpoints = lm::usbd::esp32_endpoints;
    descriptors = {};

    lm::usbd::init(cfg, endpoints, descriptors);
    chip::usb::phy::power_up();
}

auto lm::tasks::usbd::on_ready() -> fabric::managed_task_status
{
    tusb_init(); // This will trigger the descriptor callbacks immediately.
    return fabric::managed_task_status::ok;
}

auto lm::tasks::usbd::before_sleep() -> fabric::managed_task_status
{ return fabric::managed_task_status::ok; }

auto lm::tasks::usbd::on_wake() -> fabric::managed_task_status
{
    tud_task(); // Device event handling loop

    /// TODO: respond to usb::get_status requests.
    //if(cfg.cdc) lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::cdc_enabled});
    //else        lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::cdc_disabled});
    //if(cfg.hid) lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::hid_enabled});
    //else        lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::hid_disabled});

    return fabric::managed_task_status::ok;
}

lm::tasks::usbd::~usbd() {
    tusb_deinit(0);
}
