// maz/net/UdpSocket.hpp — a real, portable UDP transport socket.
//
// This is the piece that was missing under maz/net: the engine already had the *logic* of
// networking (BitStream, Reliability/ack, Connection, Replication, Rpc, Snapshot, Spawner) but no
// actual operating-system socket moving bytes between two endpoints. UdpSocket is that transport.
//
// It is a thin, header-only wrapper over the platform UDP socket API — POSIX (Linux/macOS) via
// <sys/socket.h>, Windows via Winsock2 — exposing the handful of operations the rest of maz/net
// needs: open, bind, sendTo, recvFrom, non-blocking mode, and the actual bound port. The reliability
// layer (net::AckSender/AckReceiver) sits on top of this to provide ordered/reliable delivery; this
// class deliberately stays a dumb datagram pipe.
//
// Verified here by a real localhost loopback round-trip between two sockets (tests/net/loopback) —
// which is the same code path used for two physical machines, only the destination IP differs.

#ifndef MAZ_NET_UDPSOCKET_HPP
#define MAZ_NET_UDPSOCKET_HPP

#include <cstdint>
#include <cstring>
#include <string>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

namespace maz {
namespace net {

#if defined(_WIN32)
using socket_t = SOCKET;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
using socket_t = int;
inline constexpr socket_t kInvalidSocket = -1;
#endif

// A UDP peer address: dotted-quad IPv4 string + port. Kept as a string so callers read naturally
// ("127.0.0.1"); it is resolved to a sockaddr_in at send time.
struct Endpoint {
    std::string ip = "0.0.0.0";
    std::uint16_t port = 0;

    Endpoint() = default;
    Endpoint(std::string ip_, std::uint16_t port_) : ip(std::move(ip_)), port(port_) {}

    bool operator==(const Endpoint& o) const { return port == o.port && ip == o.ip; }
    bool operator!=(const Endpoint& o) const { return !(*this == o); }
};

// One-time Winsock init on Windows; a no-op elsewhere. Constructing a UdpSocket ensures it ran.
inline bool platformNetInit() {
#if defined(_WIN32)
    static bool started = [] {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }();
    return started;
#else
    return true;
#endif
}

class UdpSocket {
public:
    UdpSocket() { platformNetInit(); }
    ~UdpSocket() { close(); }

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& o) noexcept : m_fd(o.m_fd) { o.m_fd = kInvalidSocket; }
    UdpSocket& operator=(UdpSocket&& o) noexcept {
        if (this != &o) { close(); m_fd = o.m_fd; o.m_fd = kInvalidSocket; }
        return *this;
    }

    // Create the underlying UDP socket. Returns false if the OS refuses.
    bool open() {
        close();
        m_fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        return m_fd != kInvalidSocket;
    }

    bool isOpen() const { return m_fd != kInvalidSocket; }

    // Bind to a local port (0 = let the OS pick an ephemeral port, retrievable via localPort()).
    // ip defaults to all interfaces; use "127.0.0.1" for loopback-only.
    bool bind(std::uint16_t port, const std::string& ip = "0.0.0.0") {
        if (!isOpen() && !open()) return false;
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) return false;
        return ::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    }

    // The actual local port this socket is bound to (resolves an ephemeral 0-port binding).
    std::uint16_t localPort() const {
        if (!isOpen()) return 0;
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        std::memset(&addr, 0, sizeof(addr));
        if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) return 0;
        return ntohs(addr.sin_port);
    }

    // Toggle non-blocking mode (recvFrom returns immediately with -1 when no datagram is waiting).
    bool setNonBlocking(bool on) {
        if (!isOpen()) return false;
#if defined(_WIN32)
        u_long mode = on ? 1u : 0u;
        return ioctlsocket(m_fd, static_cast<long>(FIONBIO), &mode) == 0;
#else
        int flags = ::fcntl(m_fd, F_GETFL, 0);
        if (flags < 0) return false;
        flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
        return ::fcntl(m_fd, F_SETFL, flags) == 0;
#endif
    }

    // Send a datagram to dst. Returns bytes sent, or -1 on error.
    long sendTo(const void* data, std::size_t len, const Endpoint& dst) {
        if (!isOpen()) return -1;
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(dst.port);
        if (::inet_pton(AF_INET, dst.ip.c_str(), &addr.sin_addr) != 1) return -1;
#if defined(_WIN32)
        int n = ::sendto(m_fd, static_cast<const char*>(data), static_cast<int>(len), 0,
                         reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        return static_cast<long>(n);
#else
        ssize_t n = ::sendto(m_fd, data, len, 0,
                             reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        return static_cast<long>(n);
#endif
    }

    // Receive a datagram into buf (capacity cap). On success returns bytes received and fills
    // `from` with the sender's address. Returns 0 for an empty datagram, or -1 when there is
    // nothing to read (non-blocking) or on error.
    long recvFrom(void* buf, std::size_t cap, Endpoint& from) {
        if (!isOpen()) return -1;
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        std::memset(&addr, 0, sizeof(addr));
#if defined(_WIN32)
        int n = ::recvfrom(m_fd, static_cast<char*>(buf), static_cast<int>(cap), 0,
                           reinterpret_cast<sockaddr*>(&addr), &addrLen);
        if (n < 0) return -1;
#else
        ssize_t n = ::recvfrom(m_fd, buf, cap, 0,
                               reinterpret_cast<sockaddr*>(&addr), &addrLen);
        if (n < 0) return -1;
#endif
        char ipbuf[INET_ADDRSTRLEN];
        std::memset(ipbuf, 0, sizeof(ipbuf));
        ::inet_ntop(AF_INET, &addr.sin_addr, ipbuf, sizeof(ipbuf));
        from.ip = ipbuf;
        from.port = ntohs(addr.sin_port);
        return static_cast<long>(n);
    }

    void close() {
        if (m_fd != kInvalidSocket) {
#if defined(_WIN32)
            ::closesocket(m_fd);
#else
            ::close(m_fd);
#endif
            m_fd = kInvalidSocket;
        }
    }

    socket_t handle() const { return m_fd; }

private:
    socket_t m_fd = kInvalidSocket;
};

}  // namespace net
}  // namespace maz

#endif  // MAZ_NET_UDPSOCKET_HPP
