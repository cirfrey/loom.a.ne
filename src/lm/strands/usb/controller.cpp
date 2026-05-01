#include "lm/strands/usb/controller.hpp"

#include "lm/core/helpers.hpp"

namespace framework = lm::fabric::topic::framework_t;

lm::strands::usb::controller::controller(ri& info, config_t::controller_t const* cfg)
    : info{info}
    , cfg{cfg}
{
    while(!cfg) {
        cfg = get_cfg({.id = info.id});
        // if(!cfg) {} // TODO: Error
    }

    q = fabric::queue<fabric::event>(64);
}

auto lm::strands::usb::controller::get_cfg(get_cfg_args args) -> config_t::controller_t const*
{
    // We basically ask if any managers know our name hash and use that to match against
    // config.controller[n].strand.name to find the correct config.
    if(args.name_hash == 0)
        args.name_hash = fabric::resolve_to_name(args.id, args.tries, args.timeout);

    for(auto& c : config.controller)
        if(fnv1a_32(c.strand.name | to_text) == args.name_hash)
            return &c;
    return nullptr;
}

auto lm::strands::usb::controller::on_ready() -> status { return status::ok; }
auto lm::strands::usb::controller::before_sleep() -> status { return status::ok; }
auto lm::strands::usb::controller::on_wake() -> status
{
    return status::ok;
}

lm::strands::usb::controller::~controller() {}
