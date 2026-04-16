// lm::chip::net — native (x86_64) implementation.
//
// Wraps POSIX sockets on Linux/macOS/Cygwin and WinSock2 on true Windows
// (MSVC / MinGW) behind a uniform interface. Platform differences are fully
// contained here — nothing above this layer should ever see a SOCKET, WSADATA,
// or errno.
//
// Cygwin note: Cygwin has its own POSIX socket layer and does NOT use WinSock.
// It must take the POSIX path even though LM_PORT_HOST_WINDOWS is set for it.
// LM_PORT_ENV_CYGWIN distinguishes it from true Windows.

#include "lm/chip/net.hpp"
#include "lm/port.hpp"

// ---------------------------------------------------------------------------
// Platform includes
// ---------------------------------------------------------------------------
#if LM_PORT_IS_POSIX
    // POSIX: Linux, macOS, Cygwin.
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <cerrno>
#else
    // True Windows: MSVC or MinGW, no Cygwin.
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    // On Windows, the socket handle is SOCKET (uintptr_t), but we store it
    // as socket_t (i32). Valid fds are never negative, so the cast is safe.
    // INVALID_SOCKET is (SOCKET)(~0) — we map it to chip::invalid_socket (-1).
    static auto to_socket_t(SOCKET s) -> lm::chip::socket_t {
        return (s == INVALID_SOCKET) ? lm::chip::invalid_socket : (lm::chip::socket_t)s;
    }
    static auto to_SOCKET(lm::chip::socket_t s) -> SOCKET {
        return (s == lm::chip::invalid_socket) ? INVALID_SOCKET : (SOCKET)s;
    }
#endif

// ---------------------------------------------------------------------------
// WSAStartup — true Windows only, called once per process via static init.
// ---------------------------------------------------------------------------
#if !LM_PORT_IS_POSIX
namespace {
    struct WsaInit {
        WsaInit()  { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
        ~WsaInit() { WSACleanup(); }
    };
    static WsaInit wsa_init; // constructed before main()
}
#endif

// ---------------------------------------------------------------------------
// Internal helpers — hide the platform fork from the function bodies below.
// ---------------------------------------------------------------------------
namespace {

auto native_close(lm::chip::socket_t s) -> void {
    #if LM_PORT_IS_POSIX
        ::close(s);
    #else
        ::closesocket(to_SOCKET(s));
    #endif
}

// setsockopt takes (const char*) on Windows and (const void*) on POSIX.
template <typename T>
auto setsock(lm::chip::socket_t s, int level, int optname, const T& val) -> void {
    #if LM_PORT_IS_POSIX
        ::setsockopt(s, level, optname, &val, sizeof(val));
    #else
        ::setsockopt(to_SOCKET(s), level, optname, (const char*)&val, sizeof(val));
    #endif
}

// hton16 — only needed for sin_port in make_listen_socket.
// USB/IP protocol byte-swapping lives in endian.hpp, not here.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    auto hton16(lm::u16 v) -> lm::u16 { return v; }
#else
    auto hton16(lm::u16 v) -> lm::u16 { return (lm::u16)((v >> 8) | (v << 8)); }
#endif

} // anonymous namespace

// ---------------------------------------------------------------------------
// make_listen_socket
//
// Creates a TCP socket, sets SO_REUSEADDR (avoid TIME_WAIT on restart),
// binds to 0.0.0.0:port, and calls listen(). The socket is blocking by
// default — call set_nonblocking() before using try_accept().
// ---------------------------------------------------------------------------
auto lm::chip::net::make_listen_socket(u16 port) -> socket_t
{
    #if LM_PORT_IS_POSIX
        auto fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    #else
        auto fd = to_socket_t(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    #endif
    if (fd == chip::invalid_socket) return chip::invalid_socket;

    int yes = 1;
    setsock(fd, SOL_SOCKET, SO_REUSEADDR, yes);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = hton16(port);
    addr.sin_addr.s_addr = 0; // INADDR_ANY

    #if LM_PORT_IS_POSIX
        if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0
            || ::listen(fd, 1) != 0)
    #else
        if (::bind(to_SOCKET(fd), (sockaddr*)&addr, sizeof(addr)) != 0
            || ::listen(to_SOCKET(fd), 1) != 0)
    #endif
    {
        native_close(fd);
        return chip::invalid_socket;
    }

    return fd;
}

// ---------------------------------------------------------------------------
// set_nonblocking
//
// After this call, all socket operations return immediately with WOULDBLOCK
// instead of blocking. Required before using try_accept().
// ---------------------------------------------------------------------------
auto lm::chip::net::set_nonblocking(socket_t fd) -> void
{
    #if LM_PORT_IS_POSIX
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    #else
        u_long mode = 1;
        ::ioctlsocket(to_SOCKET(fd), FIONBIO, &mode);
    #endif
}

// ---------------------------------------------------------------------------
// set_nodelay
//
// Disables Nagle's algorithm. Without this, small packets (like 48-byte
// USB/IP headers) get delayed up to 200ms waiting to be batched. Always call
// this on the connection socket after accept().
// ---------------------------------------------------------------------------
auto lm::chip::net::set_nodelay(socket_t fd) -> void
{
    int yes = 1;
    setsock(fd, IPPROTO_TCP, TCP_NODELAY, yes);
}

// ---------------------------------------------------------------------------
// try_accept
//
// Non-blocking accept. Returns invalid_socket if no client is waiting — this
// is the normal case when polling, not an error. Call set_nonblocking() on
// listen_sock before the first call.
// ---------------------------------------------------------------------------
auto lm::chip::net::try_accept(socket_t listen_sock) -> socket_t
{
    sockaddr_in client{};
    socklen_t   len = sizeof(client);

    #if LM_PORT_IS_POSIX
        int fd = ::accept(listen_sock, (sockaddr*)&client, &len);
        if (fd < 0) return chip::invalid_socket;
        return fd;
    #else
        SOCKET raw = ::accept(to_SOCKET(listen_sock), (sockaddr*)&client, &len);
        if (raw == INVALID_SOCKET) return chip::invalid_socket;
        return to_socket_t(raw);
    #endif
}

// ---------------------------------------------------------------------------
// avail
//
// Returns the number of bytes available to read without blocking (FIONREAD).
// Returns 0 on error or if the socket is invalid. Used by the USB/IP
// handshake state machine to avoid blocking reads.
// ---------------------------------------------------------------------------
auto lm::chip::net::avail(socket_t fd) -> st
{
    if (fd == chip::invalid_socket) return 0;

    #if LM_PORT_IS_POSIX
        int arg = 0;
        if (::ioctl(fd, FIONREAD, &arg) < 0) return 0;
        return (st)arg;
    #else
        u_long arg = 0;
        if (::ioctlsocket(to_SOCKET(fd), FIONREAD, &arg) != 0) return 0;
        return (st)arg;
    #endif
}

// ---------------------------------------------------------------------------
// recv_exact
//
// Reads exactly `len` bytes. Loops because a single recv() call may return
// fewer bytes than requested — the OS delivers however much is in the kernel
// buffer at that instant. Returns false if the connection closes or errors.
// ---------------------------------------------------------------------------
auto lm::chip::net::recv_exact(socket_t fd, void* buf, st len) -> bool
{
    auto* p = static_cast<u8*>(buf);
    st remaining = len;

    while (remaining > 0) {
        #if LM_PORT_IS_POSIX
            auto r = ::recv(fd, p, remaining, 0);
            if (r <= 0) return false;
        #else
            int r = ::recv(to_SOCKET(fd), (char*)p, (int)remaining, 0);
            if (r <= 0) return false;
        #endif
        p         += r;
        remaining -= (st)r;
    }
    return true;
}

// ---------------------------------------------------------------------------
// send_exact
//
// Sends exactly `len` bytes. Loops for the same reason as recv_exact.
// ---------------------------------------------------------------------------
auto lm::chip::net::send_exact(socket_t fd, const void* buf, st len) -> bool
{
    const auto* p = static_cast<const u8*>(buf);
    st remaining = len;

    while (remaining > 0) {
        #if LM_PORT_IS_POSIX
            auto s = ::send(fd, p, remaining, 0);
            if (s <= 0) return false;
        #else
            int s = ::send(to_SOCKET(fd), (const char*)p, (int)remaining, 0);
            if (s <= 0) return false;
        #endif
        p         += s;
        remaining -= (st)s;
    }
    return true;
}

// ---------------------------------------------------------------------------
// close
// ---------------------------------------------------------------------------
auto lm::chip::net::close(socket_t fd) -> void
{
    if (fd != chip::invalid_socket) native_close(fd);
}
