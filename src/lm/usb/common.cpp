#include "lm/usb/common.hpp"

#include <cstring>

auto lm::usb::configuration_descriptor_builder_state_t::append_desc(
    u8 const* data,
    st size
) -> st
{
    if(desc_curr_len + size > desc_max_len) return 0;

    std::memcpy(desc + desc_curr_len, data, size);
    desc_curr_len += size;
    return size;
}
