#pragma once

#include <atomic>
#include <bitset>

#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>

#include "stopwatch.hpp"
#include "aliases.hpp"

namespace espy
{
    template <u16 MaxPatternSize = 32>
    struct blink_pattern
    {
        constexpr static auto max_pattern_size = MaxPatternSize;

        enum type_t {
            loop,
            play_once,
        };
        blink_pattern(type_t repeats,  const char* pattern, u32 interval);
        blink_pattern(u32 num_repeats, const char* pattern, u32 interval);

        auto tick(u32 dt) -> u32 /* leftover_dt */;
        auto is_done() -> bool;
        auto state() -> bool;

        i64 remaining_repeats;

        u32 element_interval; // Interval between playing each part of the pattern.

        std::bitset<max_pattern_size> pattern;
        u16 curr_pattern_size;
        u16 curr_pattern_offset;
    };

}

namespace espy
{
    struct blink_controller
    {
        // one_offs are not preemptable.
        enum class pattern_state_t {
            one_off,
            standby
        } pattern_state = pattern_state_t::standby;

        enum class state_t
        {
            rising,
            falling,
            on,
            off
        };

        blink_pattern<32> one_off_pattern = blink_pattern<32>(1, "01", 50 * 1000);
        blink_pattern<32> standby_pattern = blink_pattern<32>(blink_pattern<32>::loop, "01", 1000 * 1000);

        bool prev_raw_state = false;
        bool curr_raw_state = false;

        u32 leftover_micros = 0;

        auto one_off(u32 num_repeats, const char* pattern, u32 interval)
        {
            if(pattern_state == pattern_state_t::one_off) return;
            pattern_state = pattern_state_t::one_off;
            one_off_pattern = decltype(one_off_pattern)(num_repeats, pattern, interval);
            leftover_micros = 0; // Clear the timing "debt"
        }

        auto standby(const char* pattern, u32 interval)
        {
            using standby_pattern_t = decltype(standby_pattern);
            standby_pattern = standby_pattern_t(standby_pattern_t::loop, pattern, interval);
            leftover_micros = 0; // Clear the timing "debt"
        }

        auto tick(u32 dt_micros) -> bool
        {
            prev_raw_state = curr_raw_state;

            switch(pattern_state)
            {
                case pattern_state_t::standby: {
                    leftover_micros = standby_pattern.tick(dt_micros + leftover_micros);
                    break;
                }
                case pattern_state_t::one_off: {
                    leftover_micros = one_off_pattern.tick(dt_micros + leftover_micros);
                    if(one_off_pattern.is_done())
                    {
                        pattern_state = pattern_state_t::standby;
                        tick(0);
                    }
                }
            }

            curr_raw_state = (pattern_state == pattern_state_t::one_off)
                ? one_off_pattern.state()
                : standby_pattern.state();
            return raw_state();
        }

        auto raw_state() -> bool { return curr_raw_state; }

        auto state() -> state_t
        {
            bool state = raw_state();
            if(     prev_raw_state == false && state == false) return state_t::off;
            else if(prev_raw_state == false && state == true)  return state_t::rising;
            else if(prev_raw_state == true  && state == true)  return state_t::on;
            else                                               return state_t::falling;
        }
    };

    // Global blink instance for convenience.
    extern blink_controller blink;

    struct blink_task_params {
        blink_controller& bc;
        u32 const gpio_pin;
    };
    auto create_blink_task(blink_task_params* bp) -> TaskHandle_t;
}

template <espy::u16 MPS>
auto espy::blink_pattern<MPS>::tick(u32 dt_micros) -> u32
{
    while(!is_done() && dt_micros >= element_interval)
    {
        dt_micros -= element_interval;
        remaining_repeats -= (curr_pattern_offset == curr_pattern_size - 1);
        curr_pattern_offset = (curr_pattern_offset + 1) % curr_pattern_size;
    }

    return dt_micros;
}

template <espy::u16 MPS>
espy::blink_pattern<MPS>::blink_pattern(type_t repeats, const char* _pattern, u32 interval)
{
    remaining_repeats = repeats == type_t::loop ? -1 : 0;
    curr_pattern_size = 0;
    curr_pattern_offset = 0;
    element_interval = interval;
    while(*_pattern != '\0'){
        if(*_pattern != '0' && *_pattern != '1') {
            ++_pattern;
            continue;
        }
        pattern[curr_pattern_size++] = *(_pattern++) != '0';
    }

    // On empty patterns we just assume 0.
    if(curr_pattern_size == 0) {
        pattern[0] = false;
        curr_pattern_size = 1;
    }
}

template <espy::u16 MPS>
espy::blink_pattern<MPS>::blink_pattern(u32 num_repeats, const char* _pattern, u32 interval)
    : blink_pattern(type_t::loop, _pattern, interval)
{
    // Skip to the end when repeats set to 0 (why would you even do this?).
    if(num_repeats == 0) {
        remaining_repeats = 0;
        curr_pattern_offset = curr_pattern_size - 1;
    } else {
        remaining_repeats = num_repeats - 1;
    }
}

template <espy::u16 MPS>
auto espy::blink_pattern<MPS>::is_done() -> bool
{
    if(remaining_repeats < 0) return false;
    return (remaining_repeats == 0) && (curr_pattern_offset == curr_pattern_size - 1);
}

template <espy::u16 MPS>
auto espy::blink_pattern<MPS>::state() -> bool
{
    return pattern[curr_pattern_offset];
}
