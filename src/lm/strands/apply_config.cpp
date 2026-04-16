#include "lm/strands/apply_config.hpp"

#include "lm/log.hpp"
#include "lm/fabric/strand.hpp"

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
    log::debug("Applying config\n");

    using f = ini::field;
    constexpr f config_ini_fields[] = {
        {
            .key = "logging.toggle" | to_text,
            .output = &config.logging.toggle,
            .type = f::enumeration,
            .enumeration_data = { .parse = f::default_enum_parser_for<config_t::feature_ini>() },
        }
        // TODO: the rest.
    };

    static constexpr auto feature_normalizer = [](auto& f){
        auto  input  = *(config_t::feature_ini*)f.output;
        auto& output = *(config_t::feature*)f.output;
        output = config_t::normalize_feature_ini(input);
    };
    // TODO: parse config here.
    auto testfeature = config_t::feature::on;
    auto testfeaturefield = ini::enumeration_field<config_t::feature_ini>(
        "my.test.feature" | to_text,
        testfeature,
        feature_normalizer
    );

    ini::parse_result res;
    res = testfeaturefield.parse({"bogus", 5});
    res = testfeaturefield.parse({"off", 3});

    // Our work here is done.
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
