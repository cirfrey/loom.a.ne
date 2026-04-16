#include "lm/strands/apply_config.hpp"

#include "lm/log.hpp"
#include "lm/fabric/strand.hpp"

#include "lm/entrypoint.hpp"

#include "lm/config.hpp"
#include "lm/ini.hpp"

lm::strands::apply_config::apply_config(fabric::strand_runtime_info& info)
{
}

// TODO: Ideally we'd parse more or less line by line in a loop until the whole file is done
//       to not overrun the logger task with one massive chunk of messages if a bunch of stuff
//       goes wrong.
auto lm::strands::apply_config::on_ready() -> fabric::managed_strand_status
{
    log::debug("Applying arch config\n");
    lm::entrypoint::arch_config();

    log::debug("Parsing ini\n");
    // TODO: parse the damn ini.

    // Our work here is done.
    log::debug("apply_config done, exiting\n");
    return fabric::managed_strand_status::suicidal;
}

auto lm::strands::apply_config::before_sleep() -> fabric::managed_strand_status
{
    return fabric::managed_strand_status::ok;
}

auto lm::strands::apply_config::on_wake() -> fabric::managed_strand_status
{
    return fabric::managed_strand_status::ok;
}

lm::strands::apply_config::~apply_config()
{
}
