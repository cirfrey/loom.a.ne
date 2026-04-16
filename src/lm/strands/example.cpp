// Use this as a starting point for your strand.

#include "lm/strands/example.hpp"

lm::strands::example::example(fabric::strand_runtime_info& info)
{
}

auto lm::strands::example::on_ready() -> fabric::managed_strand_status
{ 
    return fabric::managed_strand_status::ok; 
}

auto lm::strands::example::before_sleep() -> fabric::managed_strand_status
{ 
    return fabric::managed_strand_status::ok;
}

auto lm::strands::example::on_wake() -> fabric::managed_strand_status
{
    return fabric::managed_strand_status::ok;
}

lm::strands::example::~example()
{
}
