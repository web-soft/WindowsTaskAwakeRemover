#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "socket.hpp"
#include <mutex>

namespace {
static std::once_flag wsaOnce;
void ensureWsa() {
    std::call_once(wsaOnce, []() {
        WSADATA wsa{};
        WSAStartup(MAKEWORD(2, 2), &wsa);
    });
}
} // namespace

namespace prodjlink::platform {

UdpSocket::UdpSocket(uintptr_t s) noexcept : sock_(s) {}

UdpSocket::UdpSocket(UdpSocket&& o) noexcept
    : sock_(o.sock_), broadcastAddr_(o.broadcastAddr_) { o.sock_ = INVALID_SOCK; }

UdpSocket& UdpSocket::operator=(UdpSocket&& o) noexcept {
    if (this != &o) { close(); sock_ = o.sock_; broadcastAddr_ = o.broadcastAddr_; o.sock_ = INVALID_SOCK; }
    return *this;
}

UdpSocket::~UdpSocket() { close(); }

std::optional<UdpSocket> UdpSocket::create() {
    ensureWsa();
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return std::nullopt;
    return UdpSocket{static_cast<uintptr_t>(s)};
}

bool UdpSocket::bind(uint16_t port, BindMode mode) {
    if (!isValid()) return false;
    SOCKET s = static_cast<SOCKET>(sock_);
    if (mode == BindMode::Broadcast) {
        BOOL yes = TRUE;
        ::setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof(yes));
        ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
    }
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    return ::bind(s, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == 0;
}

bool UdpSocket::setBroadcast(bool enable) {
    if (!isValid()) return false;
    BOOL val = enable ? TRUE : FALSE;
    return ::setsockopt(static_cast<SOCKET>(sock_), SOL_SOCKET, SO_BROADCAST,
                        reinterpret_cast<const char*>(&val), sizeof(val)) == 0;
}

bool UdpSocket::setReuseAddress(bool enable) {
    if (!isValid()) return false;
    BOOL val = enable ? TRUE : FALSE;
    return ::setsockopt(static_cast<SOCKET>(sock_), SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<const char*>(&val), sizeof(val)) == 0;
}

bool UdpSocket::setReceiveTimeout(std::chrono::milliseconds ms) {
    if (!isValid()) return false;
    DWORD timeout = static_cast<DWORD>(ms.count());
    return ::setsockopt(static_cast<SOCKET>(sock_), SOL_SOCKET, SO_RCVTIMEO,
                        reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
}

bool UdpSocket::setTTL(int ttl) {
    if (!isValid()) return false;
    return ::setsockopt(static_cast<SOCKET>(sock_), IPPROTO_IP, IP_TTL,
                        reinterpret_cast<const char*>(&ttl), sizeof(ttl)) == 0;
}

int UdpSocket::recvFrom(uint8_t* buf, size_t len,
                        std::array<uint8_t, 4>& srcAddr, uint16_t& srcPort) {
    if (!isValid()) return -1;
    sockaddr_in from{};
    int fromLen = sizeof(from);
    int n = ::recvfrom(static_cast<SOCKET>(sock_),
                       reinterpret_cast<char*>(buf), static_cast<int>(len), 0,
                       reinterpret_cast<SOCKADDR*>(&from), &fromLen);
    if (n == SOCKET_ERROR) {
        int err = WSAGetLastError();
        return (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) ? 0 : -1;
    }
    uint32_t ip = ntohl(from.sin_addr.s_addr);
    srcAddr[0] = (ip >> 24) & 0xff; srcAddr[1] = (ip >> 16) & 0xff;
    srcAddr[2] = (ip >> 8)  & 0xff; srcAddr[3] =  ip        & 0xff;
    srcPort = ntohs(from.sin_port);
    return n;
}

bool UdpSocket::sendTo(const uint8_t* buf, size_t len,
                       const std::array<uint8_t, 4>& dstAddr, uint16_t dstPort) {
    if (!isValid()) return false;
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(dstPort);
    dst.sin_addr.s_addr = htonl(
        (uint32_t(dstAddr[0]) << 24) | (uint32_t(dstAddr[1]) << 16) |
        (uint32_t(dstAddr[2]) << 8)  |  uint32_t(dstAddr[3]));
    int sent = ::sendto(static_cast<SOCKET>(sock_),
                        reinterpret_cast<const char*>(buf), static_cast<int>(len), 0,
                        reinterpret_cast<const SOCKADDR*>(&dst), sizeof(dst));
    return sent == static_cast<int>(len);
}

bool UdpSocket::broadcast(const uint8_t* buf, size_t len, uint16_t port) {
    return sendTo(buf, len, broadcastAddr_, port);
}

void UdpSocket::close() {
    if (sock_ != INVALID_SOCK) { ::closesocket(static_cast<SOCKET>(sock_)); sock_ = INVALID_SOCK; }
}

bool UdpSocket::isValid() const noexcept { return sock_ != INVALID_SOCK; }

} // namespace prodjlink::platform

#endif // _WIN32
