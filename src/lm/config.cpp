// Yes, this is really all there is to it.
#include "lm/config.hpp"
#include "lm/config/boot.hpp"

namespace lm::config_detail
{
    static config_t config;
}

namespace lm
{
    config_t const& config    = config_detail::config;
    config_t&       config_rw = config_detail::config;
}
