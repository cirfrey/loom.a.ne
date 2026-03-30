#pragma once

#include "lm/core/types.hpp"

namespace lm::task { struct config; } // Forward decl.
namespace lm::bus  { struct event; } // Forward decl.

namespace lm::busmon
{
    auto task(lm::task::config const& cfg) -> void;
    auto init() -> void;

    using to_string_cb_t = u32/*how many bytes you wrote*/(*)(lm::bus::event const& /*event*/,  char* /*str_buf (write the string here)*/, u32 /*bufsize (how big the string can be)*/);
    struct event_data_t { to_string_cb_t to_string; };
    // Busmon events are special, just set:
    // - event.type as the id of the topic you want to teach it and;
    // - event.data as a event_data_t*.
    enum class event : u8 { };

    // Use this macro to define your printer (or not, can't make you), do something like this on the global scope:
    //     LOOM_BUSMON_PRINTER(fallback_printer, {
    //         auto topic_sv = lm::renum::reflect<lm::bus::topic_t>::semi_qualified(e.topic);
    //         return std::snprintf(buf, bufsize, "unkown_event{ .topic=%.*s, .type=%u, .data=%p }", topic_sv.size(), topic_sv.data(), e.type, e.data);
    //     });
    // Then, somewhere in your task code do:
    //     lm::bus::publish({ .data = &fallback_printer, .topic = lm::bus::busmon, .type = lm::bus::any });
    #define LOOM_BUSMON_PRINTER(name, funcbody) \
        auto name = lm::busmon::event_data_t{ .to_string = [](lm::bus::event const& e, char* buf, lm::u32 bufsize) -> lm::u32 { funcbody } };
}
