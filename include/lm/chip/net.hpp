#pragma once

#include "lm/core/types.hpp"
#include "lm/chip/types.hpp"

namespace lm::chip::net
{
    // Create a TCP listening socket bound to port. Returns invalid_socket on failure.
    auto make_listen_socket(u16 port) -> socket_t;

    // Non-blocking accept. Returns invalid_socket if no connection pending.
    auto try_accept(socket_t listen_sock) -> socket_t;

    // Exact-byte recv/send. Returns false if connection closed or error.
    auto recv_exact(socket_t fd, void* buf, st len) -> bool;
    auto avail(socket_t fd) -> st;
    auto send_exact(socket_t fd, const void* buf, st len) -> bool;

    auto close(socket_t fd) -> void;
    auto set_nodelay(socket_t fd) -> void;

    // Set socket to non-blocking mode (call after make_listen_socket and after accept).
    auto set_nonblocking(socket_t fd) -> void;
}
