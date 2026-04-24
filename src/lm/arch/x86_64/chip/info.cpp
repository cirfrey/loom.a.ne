#include "lm/chip.hpp"
#include "lm/core.hpp"

#include "lm/config.hpp"

#include <random>

auto lm::chip::info::codename() -> text
{
    return "Bobby" | to_text;
}

auto lm::chip::info::name() -> text
{
    return "Loom Native Host Environment" | to_text;
}

auto lm::chip::info::uuid() -> text
{
    static char uuid_str[config_t::usbcommon::string_descriptor_max_len] = "loom.a.ne:x86_64";
    static bool is_init = false;

    if(!is_init){
        uuid_str[sizeof("loom.a.ne:x86_64") - 1] = '-';

        constexpr char uuid_characters[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

        std::random_device rd;
        std::seed_seq seed{
            config.system.use_seed == feature::on ? config.system.random_seed[0] : rd(),
            config.system.use_seed == feature::on ? config.system.random_seed[1] : rd(),
            config.system.use_seed == feature::on ? config.system.random_seed[2] : rd(),
            config.system.use_seed == feature::on ? config.system.random_seed[3] : rd(),
            config.system.use_seed == feature::on ? config.system.random_seed[4] : rd(),
            config.system.use_seed == feature::on ? config.system.random_seed[5] : rd(),
            config.system.use_seed == feature::on ? config.system.random_seed[6] : rd(),
            config.system.use_seed == feature::on ? config.system.random_seed[7] : rd(),
        };
        std::mt19937 generator(seed);
        std::uniform_int_distribution<> distribution(0, sizeof(uuid_characters) - 1);

        for (st i = sizeof("loom.a.ne:x86_64-") - 1; i < sizeof(uuid_str) - 1; ++i) {
            uuid_str[i] = uuid_characters[distribution(generator)];
        }

        uuid_str[sizeof(uuid_str) - 1] = '\0';
        is_init = true;
    };

    return { .data = uuid_str, .size = sizeof(uuid_str) - 1 };
}

auto lm::chip::info::banner() -> text
{
    return {nullptr, 0};
}
