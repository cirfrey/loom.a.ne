// Use this as a starting point for your strand.

#include "lm/strands/example.hpp"

lm::strands::example::example(ri& info) : info{info} {}

auto lm::strands::example::on_ready()     -> status { return status::ok; }
auto lm::strands::example::before_sleep() -> status { return status::ok; }
auto lm::strands::example::on_wake()      -> status { return status::ok; }

lm::strands::example::~example() {}
