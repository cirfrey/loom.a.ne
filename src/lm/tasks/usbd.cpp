#include "lm/tasks/usbd.hpp"

#include "lm/chip/usb.hpp"
#include "lm/usbd/esp32.hpp" // TODO: abstract

#include "lm/config.hpp"

#include <tusb.h>

lm::tasks::usbd::usbd(fabric::task_runtime_info& info)
{
    get_status_q = fabric::queue<fabric::event>(1);
    get_status_q_tok = fabric::bus::subscribe(
        get_status_q,
        fabric::topic::usbd,
        { event::get_status }
    );

    cfg = {
        .cdc  = true,
        .uac  = lm::usbd::cfg_t::no_uac,
        .hid  = true,
        .midi = lm::usbd::cfg_t::midi_inout,
        .midi_cable_count = 16,
        .msc  = false,
    };
    endpoints = lm::usbd::esp32_endpoints;
    descriptors = {};

    /// TODO: refactor all usbd internal state into this task.
    lm::usbd::init(cfg, endpoints, descriptors);
    chip::usb::phy::power_up();
}

auto lm::tasks::usbd::on_ready() -> fabric::managed_task_status
{
    tud_task_handle = fabric::task::create(config::task::tud, {}, [](void*){
        tusb_init(); // This will trigger the descriptor callbacks immediately.
        while(1) tud_task();
    });

    broadcast_status();

    return fabric::managed_task_status::ok;
}

auto lm::tasks::usbd::before_sleep() -> fabric::managed_task_status
{ return fabric::managed_task_status::ok; }

auto lm::tasks::usbd::on_wake() -> fabric::managed_task_status
{
    for(auto& e : get_status_q.consume<fabric::event>())
    { broadcast_status(); }

    return fabric::managed_task_status::ok;
}

lm::tasks::usbd::~usbd() {
    if(tud_task_handle) fabric::task::reap(tud_task_handle);
    tusb_deinit(0);
}

auto lm::tasks::usbd::broadcast_status() -> void
{
    fabric::bus::publish({
        .topic = fabric::topic::usbd,
        .type  = cfg.cdc ? event::cdc_enabled : event::cdc_disabled
    });
    fabric::bus::publish({
        .topic = fabric::topic::usbd,
        .type  = cfg.hid ? event::hid_enabled : event::hid_disabled
    });
}
