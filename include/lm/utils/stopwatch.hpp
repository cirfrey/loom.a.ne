#pragma once

#include "lm/aliases.hpp"

#include <chrono>

namespace lm
{
    template <typename Timer = std::chrono::high_resolution_clock>
    struct stopwatch
    {
        using timer = Timer;
        using time_point = decltype(timer::now());

        stopwatch();

        constexpr auto restart() -> stopwatch&;
        constexpr auto click() -> stopwatch&;

        template <typename Duration = std::chrono::duration<float>>
        constexpr auto last_segment() const;

        template <typename Duration = std::chrono::duration<float>>
        constexpr auto since_click() const;

        template <typename Duration = std::chrono::duration<float>>
        constexpr auto since_beginning() const;

        constexpr auto start_point()    const -> time_point { return start; }
        constexpr auto previous_point() const -> time_point { return previous_segment_start; }
        constexpr auto this_point()     const -> time_point { return this_segment_start; }

    private:
        time_point start;
        time_point previous_segment_start;
        time_point this_segment_start;
    };
    using default_stopwatch = stopwatch< std::chrono::high_resolution_clock >;

    template <typename Precision = std::chrono::microseconds, typename Timer = std::chrono::high_resolution_clock>
    struct simple_timer
    {
        using precision = Precision;
        using timer = Timer;

        // Interval's unit corresponds to the precision you set.
        simple_timer(u32 interval) : interval{interval} {}

        constexpr auto start() -> void { sw.click(); }
        constexpr auto restart() -> void { sw.click(); }
        constexpr auto is_done() -> bool { return sw.template since_click<precision>() > interval; }

    private:
        u32 interval;
        stopwatch<timer> sw;
    };
}


// Implementations.

template <typename Timer>
lm::stopwatch<Timer>::stopwatch()
    : start{timer::now()}
    , previous_segment_start{start}
    , this_segment_start{start}
{}

template <typename Timer>
constexpr auto lm::stopwatch<Timer>::restart() -> stopwatch&
{ return (*this = stopwatch()); }

template <typename Timer>
constexpr auto lm::stopwatch<Timer>::click() -> stopwatch&
{
    previous_segment_start = this_segment_start;
    this_segment_start = timer::now();
    return *this;
}

template <typename Timer>
template <typename Duration>
constexpr auto lm::stopwatch<Timer>::last_segment() const
{ return std::chrono::duration_cast<Duration>(this_segment_start - previous_segment_start).count(); }

template <typename Timer>
template <typename Duration>
constexpr auto lm::stopwatch<Timer>::since_click() const
{ return std::chrono::duration_cast<Duration>(timer::now() - this_segment_start).count(); }

template <typename Timer>
template <typename Duration>
constexpr auto lm::stopwatch<Timer>::since_beginning() const
{ return std::chrono::duration_cast<Duration>(timer::now() - start).count(); }
