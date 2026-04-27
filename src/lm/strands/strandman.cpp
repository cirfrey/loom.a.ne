#include "lm/strands/strandman.hpp"

#include "lm/core/all.hpp"
#include "lm/fabric/all.hpp"
#include "lm/registry.hpp"
#include "lm/log.hpp"
#include "lm/chip/all.hpp"

#include <cstring>

lm::strands::strandman::strandman([[maybe_unused]] st max_strands, std::span<strand_t>& strands)
{
    strandman_q = fabric::queue<fabric::event>(config.strandman.strandman_queue_size);
    strandman_tok = fabric::bus::subscribe(strandman_q, fabric::topic::framework, {
        fabric::topic::framework_t::request_manager_resolve::type,
        fabric::topic::framework_t::request_manager_announce::type,
        fabric::topic::framework_t::request_register_strand::type,
        fabric::topic::framework_t::request_strand_running::type,
    }, [](void* _manager_strand, std::span<fabric::event> events){
        using request_register_strand = fabric::topic::framework_t::request_register_strand;

        // Filter requests for registering that we are not interested in.
        // We do this at the bus instead of on the strand so the end-user knows if it
        // was posted to our queue or not.
        auto* manager_strand = (strand_t*)_manager_strand;
        if(events[0].type == request_register_strand::type) {
            auto data = events[0].get_payload<request_register_strand>();
            if(data.manager_id == manager_strand->id)
                return fabric::bus::userfilter_return::pass;
            else return fabric::bus::userfilter_return::filter;
        }

        return fabric::bus::userfilter_return::pass;
    }, strands.data());

    status_q = fabric::queue<fabric::event>(config.strandman.status_queue_size);
    status_q_tok = fabric::bus::subscribe(status_q, fabric::topic::framework, {fabric::topic::framework_t::strand_status::type});
}

auto lm::strands::strandman::do_loop(st max_strands, std::span<strand_t>& strands) -> bool
{
    fabric::strand::sleep_ms(strands[0].sleep_ms);

    process_events(max_strands, strands);
    manage_strands(max_strands, strands);

    return true;
}

auto lm::strands::strandman::get_strand_named(u32 name_hash, std::span<strand_t>& strands) -> u8
{
    for(auto& s : strands)
        if(fnv1a_32(to_text(s.name)) == name_hash)
            return s.id;
    return 0;
}

auto lm::strands::strandman::on_event_request_manager_announce(
    fabric::event const& e,
    st max_strands,
    std::span<strand_t>& strands
) -> st
{
    using request  = fabric::topic::framework_t::request_manager_announce;
    using response = fabric::topic::framework_t::response_manager_announce;

    auto data = e.get_payload<request>();
    fabric::bus::publish(fabric::event{
        .topic     = fabric::topic::framework,
        .type      = response::type,
        .strand_id = strands[0].id,
    }.with_payload(response{
        .seqnum          = data.seqnum,
        .safe_timeout_ms = clamp(strands[0].sleep_ms, 1, strands[0].sleep_ms) * 3 | toe,
        .available_slots = clamp(max_strands - strands.size(), 0, unsigned_max<16>) | toe,
    }));

    return 1;
}

auto lm::strands::strandman::on_event_request_manager_resolve(
    fabric::event const& e,
    [[maybe_unused]] st max_strands,
    std::span<strand_t>& strands
) -> st
{
    using request = fabric::topic::framework_t::request_manager_resolve;
    using response = fabric::topic::framework_t::response_manager_resolve;

    auto data = e.get_payload<request>();
    u8 id = get_strand_named(data.name_hash, strands);
    if(id == 0) return 1;

    fabric::bus::publish(fabric::event{
        .topic = fabric::topic::framework,
        .type    = response::type,
        .strand_id = strands[0].id,
    }.with_payload(response{
        .resolved_id  = id,
        .seqnum       = data.seqnum,
        .requester_id = e.strand_id,
    }));

    return 1;
}

auto lm::strands::strandman::on_event_request_register_strand(
    fabric::event const& e,
    st max_strands,
    std::span<strand_t>& strands
) -> st
{
    using request = fabric::topic::framework_t::request_register_strand;
    using response = fabric::topic::framework_t::response_register_strand;

    st consumed = 1;
    auto data = e.get_payload<request>();

    // If we're too late me we'll discard it.
    auto register_micros = 1 * 1000;
    if(chip::time::uptime() + register_micros > e.timestamp + data.timeout_micros)
        return 1;

    // If we are already full we don't register the strand.
    if(strands.size() == max_strands) {
        fabric::bus::publish(fabric::event{
            .topic     = fabric::topic::framework,
            .type      = response::type,
            .strand_id = strands[0].id,
        }.with_payload(response{
            .requester_id = e.strand_id,
            .seqnum       = data.seqnum,
            .result       = response::manager_cant_handle_more_strands,
            .id           = 0,
        }));
        return consumed;
    }

    // If its malformed we ignore it.
    if(e.extension_count() < request::min_extension_count || !e.is_local()) {
        fabric::bus::publish(fabric::event{
            .topic     = fabric::topic::framework,
            .type      = response::type,
            .strand_id = strands[0].id,
        }.with_payload(response{
            .requester_id = e.strand_id,
            .seqnum       = data.seqnum,
            .result       = response::request_malformed,
            .id           = 0,
        }));
        return consumed;
    }

    auto [register_status, newstrand_id] = data.autoassign_id
        ? registry::strand_id.reserve()
        : registry::strand_id.reserve(data.id);
    if(register_status == registry::result::error) {
        fabric::bus::publish(fabric::event{
            .topic     = fabric::topic::framework,
            .type      = response::type,
            .strand_id = strands[0].id,
        }.with_payload(response{
            .requester_id = e.strand_id,
            .seqnum       = data.seqnum,
            .result       = response::id_in_use,
            .id           = data.id,
        }));
        return consumed;
    }

    request::extdata extdata;
    strandman_q.receive(&extdata, 0);
    ++consumed;
    if(extdata.strand == nullptr) {
        registry::strand_id.unreserve(newstrand_id);
        fabric::bus::publish(fabric::event{
            .topic     = fabric::topic::framework,
            .type      = response::type,
            .strand_id = strands[0].id,
        }.with_payload(response{
            .requester_id = e.strand_id,
            .seqnum       = data.seqnum,
            .result       = response::request_malformed,
            .id           = 0,
        }));
        return consumed;
    }

    request::extname extname;
    strandman_q.receive(&extname, 0);
    ++consumed;
    if(get_strand_named(fnv1a_32(to_text(extname.name)), strands))
    {
        registry::strand_id.unreserve(newstrand_id);
        fabric::bus::publish(fabric::event{
            .topic     = fabric::topic::framework,
            .type      = response::type,
            .strand_id = strands[0].id,
        }.with_payload(response{
            .requester_id = e.strand_id,
            .seqnum       = data.seqnum,
            .result       = response::name_in_use,
            .id           = 0,
        }));
        return consumed;
    }

    strand_t newstrand {
        .code       = extdata.strand,
        .id         = newstrand_id | toe,
        .stack_size = extdata.stack_size,
        .sleep_ms   = extdata.sleep_ms,
        .priority   = extdata.priority,
        .core_affinity = extdata.core_affinity,
        .requested_running = extdata.request_running,
    };
    std::memcpy(newstrand.name, extname.name, sizeof(request::name_t));

    bool too_many_depends = false;
    auto i = 0;
    request::extdepends extdepends;
    for(auto a = 0_st; a < e.extension_count() - 2; ++a)
    {
        strandman_q.receive(&extdepends, 0);
        ++consumed;
        for(auto& dep : extdepends.depends) {
            if(dep.other == 0) continue;
            if(i >= config_t::strandman_t::max_depends) {
                too_many_depends = true;
                continue;
            }
            newstrand.depends[i++] = dep;
        }
    }

    if(too_many_depends) {
        registry::strand_id.unreserve(newstrand_id);
        fabric::bus::publish(fabric::event{
            .topic     = fabric::topic::framework,
            .type      = response::type,
            .strand_id = strands[0].id,
        }.with_payload(response{
            .requester_id = e.strand_id,
            .seqnum       = data.seqnum,
            .result       = response::too_many_depends,
            .id           = 0,
        }));
        return consumed;
    }

    strands = {strands.data(), strands.size() + 1};
    strands[strands.size() - 1] = newstrand;
    fabric::bus::publish(fabric::event{
        .topic     = fabric::topic::framework,
        .type      = response::type,
        .strand_id = strands[0].id,
    }.with_payload(response{
        .requester_id = e.strand_id,
        .seqnum       = data.seqnum,
        .result       = response::ok,
        .id           = newstrand.id,
    }));
    log::debug("Registered strand [%s] with id [%u]\n", newstrand.name, newstrand.id);
    return consumed;
}

auto lm::strands::strandman::on_event_request_strand_running(
    fabric::event const& e,
    [[maybe_unused]] st max_strands,
    std::span<strand_t>& strands
) -> st
{
    using request = fabric::topic::framework_t::request_strand_running;
    using response = fabric::topic::framework_t::response_strand_running;

    auto data = e.get_payload<request>();

    strand_t* s = nullptr;
    for(auto& st : strands)
        if(st.id == data.strand_id) {
            s = &st;
            break;
        }
    if(!s) return 1;

    if(data.set_running_state)
        s->requested_running = data.new_running_state;

    fabric::bus::publish(fabric::event{
        .topic     = fabric::topic::framework,
        .type      = response::type,
        .strand_id = strands[0].id
    }.with_payload(response{
        .requester_id  = e.strand_id,
        .seqnum        = data.seqnum,
        .running_state = data.new_running_state,
    }));

    return 1;
}

auto lm::strands::strandman::process_events(st max_strands, std::span<strand_t>& strands) -> void
{
    // Handle general requests.
    for (auto const& e : strandman_q.consume<fabric::event>())
    {
        using namespace fabric::topic::framework_t;

        st consumed = 1;
        switch(e.type){
            case request_manager_announce::type:
                consumed = on_event_request_manager_announce(e, max_strands, strands);
                break;
            case request_manager_resolve::type:
                consumed = on_event_request_manager_resolve(e, max_strands, strands);
                break;
            case request_register_strand::type:
                consumed = on_event_request_register_strand(e, max_strands, strands);
                break;
            case request_strand_running::type:
                consumed = on_event_request_strand_running(e, max_strands, strands);
                break;
        }
        auto remaning = e.extension_count() + 1 - consumed;
        for([[maybe_unused]] auto const& _ : strandman_q.consume<fabric::event>(remaning));
    }

    // Handle strand updates.
    for (auto const& e : status_q.consume<fabric::event>()) {
        strand_t* target_strand = nullptr;
        for(auto& strand : strands) {
            if(strand.id != e.strand_id) continue;

            target_strand = &strand;
            break;
        }
        if(!target_strand) continue;

        auto new_status = e.get_payload<fabric::topic::framework_t::strand_status>().status;
        // Prevent stale messages.
        if(e.timestamp < target_strand->status_timestamp || new_status == target_strand->status) continue;

        auto prev = renum<strand_t::status_t>::unqualified(target_strand->status);
        auto curr = renum<strand_t::status_t>::unqualified(new_status);
        log::regular(
            "[%s]: [%.*s] => [%.*s]\n",
            target_strand->name,
            prev.size, prev.data,
            curr.size, curr.data
        );
        target_strand->status_timestamp = e.timestamp;
        target_strand->status = new_status;
    }
}

auto lm::strands::strandman::dependencies_satisfied(
    std::span<strand_t> strands,
    strand_t const& strand,
    strand_t::status_t target_status
) -> bool
{
    bool dependencies_satisfied = true;
    for(auto& dep : strand.depends) {
        if(dep.my_status != target_status) continue;

        strand_t* other_strand = nullptr;
        for(auto& other : strands) {
            auto othername = to_text((char*)other.name);
            if(other.id == dep.other || lm::fnv1a_32({othername.data, othername.size}) == dep.other){
                other_strand = &other;
                break;
            }
        }
        // Theres a dependency on a strand I don't know about, clearly
        // we can't with sane conscience say that the dependencies are
        // satisfied.
        // TODO: Ideally we'd log this.
        if(!other_strand) return false;

        dependencies_satisfied = dependencies_satisfied && other_strand->status >= dep.other_status;
    }
    return dependencies_satisfied;
}

auto lm::strands::strandman::manage_strands([[maybe_unused]] st max_strands, std::span<strand_t>& strands) -> void
{
    using status = strand_t::status_t;

    using signal = fabric::topic::framework_t::strand_signal;

    for(auto i = 1_st; i < strands.size(); ++i)
    {
        auto& strand = strands[i];

        auto can_be_created   =
            strand.status == status::not_created || strand.status == status::reaped;
        auto can_set_running  = strand.status == status::ready;
        auto can_stop         = strand.status == status::running;
        auto can_reap         = strand.status == status::stopped;
        auto should_be_created  = can_be_created && strand.requested_running == true;
        auto should_set_running = can_set_running && strand.requested_running == true;
        auto should_stop        = can_stop && strand.requested_running == false;
        auto should_reap        = can_reap;

        if(should_be_created && dependencies_satisfied(strands, strand, status::created))
        {
            auto h = fabric::strand::create(fabric::strand::create_strand_args{
                .name = strand.name,
                .id = strand.id,
                .priority = strand.priority,
                .stack_size = strand.stack_size,
                // TODO: fixme!
                .core_affinity = strand.core_affinity == 255 ? fabric::strand::create_strand_args::no_affinity : strand.core_affinity,
                .sleep_ms = strand.sleep_ms,
                .code = strand.code,
            }, fabric::strand::managed_strand_params{ .id = strand.id, .sleep_ms = strand.sleep_ms } | smuggle<void*>);
            if(h != fabric::strand::bad_handle) {
                using status = fabric::topic::framework_t::strand_status;
                fabric::bus::publish(fabric::event{
                    .topic = fabric::topic::framework,
                    .type  = status::type,
                    .strand_id = strand.id,
                }.with_payload(status{ .status = status::created }));
                strand.status           = status::created;
                strand.status_timestamp = chip::time::uptime();
                strand.handle           = h;
            }
        }
        else if(should_set_running && dependencies_satisfied(strands, strand, status::running))
        {
            fabric::bus::publish(fabric::event{
                .topic     = fabric::topic::framework,
                .type      = signal::type,
                .strand_id = strands[0].id,
            }.with_payload(signal{
                .target_strand = strand.id,
                .signal        = signal::start,
            }));
        }
        else if(should_stop && dependencies_satisfied(strands, strand, status::stopped))
        {
            fabric::bus::publish(fabric::event{
                .topic     = fabric::topic::framework,
                .type      = signal::type,
                .strand_id = strands[0].id,
            }.with_payload(signal{
                .target_strand = strand.id,
                .signal        = signal::stop,
            }));
        }
        else if(should_reap && dependencies_satisfied(strands, strand, status::reaped))
        {
            // On native, we need to let the thread code run off before we reap it, on
            // freertos we can just straight up kill it.
            // That's why we try to kill it first and then notify that it should run off (and try
            // to kill it on the next iteration if it has run off).
            if(fabric::strand::reap(strand.handle) == fabric::strand::reap_status::success) {
                using status = fabric::topic::framework_t::strand_status;
                fabric::bus::publish(fabric::event{
                    .topic = fabric::topic::framework,
                    .type  = status::type,
                    .strand_id = strand.id,
                }.with_payload(status{ .status = status::reaped }));
                strand.status           = status::reaped;
                strand.status_timestamp = chip::time::uptime();
                strand.handle           = nullptr;
            }
            fabric::bus::publish(fabric::event{
                .topic     = fabric::topic::framework,
                .type      = signal::type,
                .strand_id = strands[0].id,
            }.with_payload(signal{
                .target_strand = strand.id,
                .signal        = signal::die,
            }));
        }
    }
}
