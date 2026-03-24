#pragma once

namespace espy::usb::cdc
{
    struct init_settings
    {
        bool forward_log_to_cdc = true;
        bool echo_incoming = true;
    };
    auto init(const init_settings& = init_settings{}) -> void;
}
