#include "lm/strands/strandman.hpp"

#include "lm/core.hpp"
#include "lm/fabric.hpp"

#include "lm/log.hpp"

lm::strands::strandman::strandman()
{
    status_q = fabric::queue<fabric::event>(16);
    status_q_tok = fabric::bus::subscribe(status_q, fabric::topic::strand, {
        fabric::strand::event::created,
        fabric::strand::event::ready,
        fabric::strand::event::running,
        fabric::strand::event::waiting_for_reap,
    });
}

auto lm::strands::strandman::do_loop(st max_strands, std::span<strand_info_t>& strands) -> bool
{
    fabric::strand::sleep_ms(strands[0].runtime.sleep_ms);

    process_events(strands);
    spawn_strands(strands);
    set_running_strands(strands);
    reap_stopped_strands(strands);

    return true;
}

auto lm::strands::strandman::process_events(std::span<strand_info_t>& strands) -> void
{
    for (auto const& e : status_q.consume<fabric::event>()) {
        strand_info_t* info = nullptr;
        for(auto& t : strands) {
            if(t.runtime.id != e.strand_id) continue;

            info = &t;
            break;
        }
        if(!info) return;

        // Update handle if provided and we don't know it yet.
        auto new_handle = e.get_payload<fabric::strand::status_event>().handle;
        if(info->handle == nullptr && new_handle) info->handle = new_handle;

        using st = strand_info_t::strand_status;
        using se = fabric::strand::event;
        auto new_status = st::not_created;
        switch ((se)e.type) {
            case se::created:          new_status = st::created; break;
            case se::ready:            new_status = st::ready;   break;
            case se::running:          new_status = st::running; break;
            case se::waiting_for_reap: new_status = st::stopped; break;
        }

        // We can't really go backwards in status unless the strand was deleted and we want
        // to recreate it. If we got a lesser status than we already have, that just means
        // we received a stale message or there was some sync/bus contention issues (they
        // sent their message with their old status before they got ours or something like that).
        /// TODO: handle exactly that case.
        //
        /// TODO: we *could* solve it by adding a timestamp to the event and keeping track
        ///       of *when* the state changed, ignoring messages older than the last state
        ///       change, but that just seems like a lot of effort.
        if(new_status <= info->status) continue;

        auto prev = renum<st>::unqualified(info->status);
        auto curr = renum<st>::unqualified(new_status);
        log::regular(
            "[%s]: [%.*s] => [%.*s]\n",
            info->constants.name,
            prev.size, prev.data,
            curr.size, curr.data
        );
        info->status = new_status;
    }
}

auto lm::strands::strandman::dependencies_satisfied(
    std::span<strand_info_t>& strands,
    strand_info_t strand,
    strand_info_t::strand_status target_status
) -> bool
{
    bool dependencies_satisfied = true;
    for(auto& dep : strand.depends) {
        if(dep.my_status != target_status) continue;

        strand_info_t* other_strand = nullptr;
        for(auto& other : strands) {
            if(other.runtime.id == dep.other_id){
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

auto lm::strands::strandman::spawn_strands(std::span<strand_info_t>& strands) -> void
{
    // TODO: very simple logic for now, need to implement the dependencies logic.
    for(auto& strand : strands)
    {
        if(!(strand.status == strand_info_t::not_created && strand.should_be_running)) continue;

        if(!dependencies_satisfied(strands, strand, strand_info_t::requested_creation)) continue;

        // TODO: handle strand.code == null;
        auto h = fabric::strand::create(strand.constants, strand.runtime, strand.code);
        strand.status = strand_info_t::requested_creation;
        strand.handle = h;
    }
}

auto lm::strands::strandman::set_running_strands(std::span<strand_info_t>& strands) -> void
{
    for(auto& strand : strands)
    {
        if(!(strand.status == strand_info_t::ready && strand.should_be_running)) continue;

        if(!dependencies_satisfied(strands, strand, strand_info_t::running)) continue;

        fabric::bus::publish(fabric::event{
            .topic = fabric::topic::strand,
            .type  = fabric::strand::event::signal_start,
            .strand_id = strands[0].runtime.id,
        }.with_payload(fabric::strand::signal_event{ .strand_id = strand.runtime.id }));
    }
}

auto lm::strands::strandman::reap_stopped_strands(std::span<strand_info_t>& strands) -> void
{
    for(auto& strand : strands)
    {
        if(!(strand.status == strand_info_t::stopped)) continue;

        if(!dependencies_satisfied(strands, strand, strand_info_t::deleted)) continue;

        // TODO: erroring on native for now.
        return;

        if(!strand.handle) return; // TODO: logme! I really wanna reap this strand but i don't know
                                   //       it's handle.
        fabric::strand::reap(strand.handle);
        strand.status = strand_info_t::deleted;
        strand.handle = nullptr;
    }
}
