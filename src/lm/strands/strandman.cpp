#include "lm/strands/strandman.hpp"

#include "lm/core.hpp"
#include "lm/fabric.hpp"
#include "lm/registry.hpp"
#include "lm/log.hpp"

#include <cstring>

lm::strands::strandman::strandman(st max_strands, std::span<strand_t>& strands)
{
    strandman_q = fabric::queue<fabric::event>(config.strandman.strandman_queue_size);
    strandman_tok = fabric::bus::subscribe(strandman_q, fabric::topic::framework, {
        fabric::topic::framework_t::request_manager_announce::type,
        fabric::topic::framework_t::request_register_strand::type,
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

auto lm::strands::strandman::process_events(st max_strands, std::span<strand_t>& strands) -> void
{
    // Handle registering strands.
    for (auto const& e : strandman_q.consume<fabric::event>())
    {
        using request_register_strand   = fabric::topic::framework_t::request_register_strand;
        using response_register_strand  = fabric::topic::framework_t::response_register_strand;
        using request_manager_announce  = fabric::topic::framework_t::request_manager_announce;
        using response_manager_announce = fabric::topic::framework_t::response_manager_announce;

        auto consume = [&](i32 count = -1){
            if(count < 0) count = e.extension_count();
            for(auto const& ext : strandman_q.consume<fabric::event>(count));
        };

        // Protect against future dummy-dum-dums.
        if(!(e.type == request_register_strand::type || e.type == request_manager_announce::type)) {
            consume();
            continue;
        }

        // If it's an announce request it's much simpler.
        if(e.type == request_manager_announce::type) {
            auto data = e.get_payload<request_manager_announce>();
            fabric::bus::publish(fabric::event{
                .topic     = fabric::topic::framework,
                .type      = response_manager_announce::type,
                .strand_id = strands[0].id,
            }.with_payload(response_manager_announce{
                .seqnum       = data.seqnum,
                .safe_timeout = 5 * 1000, // We'd for sure handle anything in 5ms, right... ?
                .available_slots = clamp(max_strands - strands.size(), 0, unsigned_max<16>) | toe,
            }));
            continue;
        }

        auto data = e.get_payload<request_register_strand>();

        // If we're too late me we'll discard it.
        auto register_micros = 1 * 1000;
        if(chip::time::uptime() + register_micros > e.timestamp + data.timeout_micros)
        {
            consume();
            continue;
        }

        // If we are already full we don't register the strand.
        if(strands.size() == max_strands) {
            consume();
            fabric::bus::publish(fabric::event{
                .topic     = fabric::topic::framework,
                .type      = response_register_strand::type,
                .strand_id = strands[0].id,
            }.with_payload(response_register_strand{
                .requester_id = e.strand_id,
                .seqnum       = data.seqnum,
                .result       = response_register_strand::manager_cant_handle_more_strands,
            }));
            continue;
        }

        // If its malformed we ignore it.
        if(e.extension_count() < request_register_strand::min_extension_count || !e.is_local()) {
            consume();
            fabric::bus::publish(fabric::event{
                .topic     = fabric::topic::framework,
                .type      = response_register_strand::type,
                .strand_id = strands[0].id,
            }.with_payload(response_register_strand{
                .requester_id = e.strand_id,
                .seqnum       = data.seqnum,
                .result       = response_register_strand::request_malformed,
            }));
            continue;
        }

        auto [register_status, newstrand_id] = data.autoassign_id
            ? registry::strand_id.reserve()
            : registry::strand_id.reserve(data.id);
        if(register_status == registry::result::error) {
            consume();
            fabric::bus::publish(fabric::event{
                .topic     = fabric::topic::framework,
                .type      = response_register_strand::type,
                .strand_id = strands[0].id,
            }.with_payload(response_register_strand{
                .requester_id = e.strand_id,
                .seqnum       = data.seqnum,
                .result       = response_register_strand::id_in_use,
            }));
            continue;
        }

        request_register_strand::extdata extdata;
        strandman_q.receive(&extdata, 0);
        if(extdata.strand == nullptr) {
            registry::strand_id.unreserve(newstrand_id);
            consume(e.extension_count() - 1);
            fabric::bus::publish(fabric::event{
                .topic     = fabric::topic::framework,
                .type      = response_register_strand::type,
                .strand_id = strands[0].id,
            }.with_payload(response_register_strand{
                .requester_id = e.strand_id,
                .seqnum       = data.seqnum,
                .result       = response_register_strand::request_malformed,
            }));
            continue;
        }

        request_register_strand::extname extname;
        strandman_q.receive(&extname, 0);
        // TODO: if we already have a strand with that name we'll inform the user.

        strand_t newstrand {
            .code       = extdata.strand,
            .id         = newstrand_id | toe,
            .stack_size = extdata.stack_size,
            .sleep_ms   = extdata.sleep_ms,
            .priority   = extdata.priority,
            .core_affinity = extdata.core_affinity,
            .requested_running = extdata.request_running,
        };
        std::memcpy(newstrand.name, extname.name, sizeof(request_register_strand::name_t));

        bool too_many_depends = false;
        auto i = 0;
        request_register_strand::extdepends extdepends;
        for(auto a = 0; a < e.extension_count() - 2; ++a)
        {
            strandman_q.receive(&extdepends, 0);
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
                .type      = response_register_strand::type,
                .strand_id = strands[0].id,
            }.with_payload(response_register_strand{
                .requester_id = e.strand_id,
                .seqnum       = data.seqnum,
                .result       = response_register_strand::too_many_depends,
            }));
            continue;
        }

        strands = {strands.data(), strands.size() + 1};
        strands[strands.size() - 1] = newstrand;
        fabric::bus::publish(fabric::event{
            .topic     = fabric::topic::framework,
            .type      = response_register_strand::type,
            .strand_id = strands[0].id,
        }.with_payload(response_register_strand{
            .requester_id = e.strand_id,
            .seqnum       = data.seqnum,
            .result       = response_register_strand::ok,
            .id           = newstrand.id,
        }));
        log::debug("Registered strand [%s] with id [%u]\n", newstrand.name, newstrand.id);
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

auto lm::strands::strandman::manage_strands(st max_strands, std::span<strand_t>& strands) -> void
{
    using status = strand_t::status_t;

    using signal = fabric::topic::framework_t::strand_signal;

    for(auto& strand : strands)
    {
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
            });
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
