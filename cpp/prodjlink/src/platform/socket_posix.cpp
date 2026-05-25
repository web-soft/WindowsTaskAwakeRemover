#ifndef _WIN32

#include "socket.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace prodjlink::platform {

UdpSocket::UdpSocket(int fd) noexcept : fd_(fd) {}

UdpSocket::UdpSocket(UdpSocket&& o) noexcept
    : fd_(o.fd_), broadcastAddr_(o.broadcastAddr_) { o.fd_ = -1; }

UdpSocket& UdpSocket::operator=(UdpSocket&& o) noexcept {
    if (this != &o) { close(); fd_ = o.fd_; broadcastAddr_ = o.broadcastAddr_; o.fd_ = -1; }
    return *this;
}

UdpSocket::~UdpSocket() { close(); }

std::optional<UdpSocket> UdpSocket::create() {
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return std::nullopt;
    return UdpSocket{fd};
}

bool UdpSocket::bind(uint16_t port, BindMode mode) {
    if (!isValid()) return false;
    if (mode == BindMode::Broadcast) {
        int yes = 1;
        ::setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
        ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
        ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
    }
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    return ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

bool UdpSocket::setBroadcast(bool enable) {
    if (!isValid()) return false;
    int val = enable ? 1 : 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) == 0;
}

bool UdpSocket::setReuseAddress(bool enable) {
    if (!isValid()) return false;
    int val = enable ? 1 : 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == 0;
}

bool UdpSocket::setReceiveTimeout(std::chrono::milliseconds ms) {
    if (!isValid()) return false;
    struct timeval tv;
    tv.tv_sec  = static_cast<long>(ms.count() / 1000);
    tv.tv_usec = static_cast<long>((ms.count() % 1000) * 1000);
    return ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

bool UdpSocket::setTTL(int ttl) {
    if (!isValid()) return false;
    return ::setsockopt(fd_, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0;
}

int UdpSocket::recvFrom(uint8_t* buf, size_t len,
                        std::array<uint8_t, 4>& srcAddr, uint16_t& srcPort) {
    if (!isValid()) return -1;
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);
    ssize_t n = ::recvfrom(fd_, buf, len, 0,
                           reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) return 0;
        return -1;
    }
    uint32_t ip = ntohl(from.sin_addr.s_addr);
    srcAddr[0] = (ip >> 24) & 0xff; srcAddr[1] = (ip >> 16) & 0xff;
    srcAddr[2] = (ip >> 8)  & 0xff; srcAddr[3] =  ip        & 0xff;
    srcPort = ntohs(from.sin_port);
    return static_cast<int>(n);
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
    ssize_t sent = ::sendto(fd_, buf, len, 0,
                            reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
    return sent == static_cast<ssize_t>(len);
}

bool UdpSocket::broadcast(const uint8_t* buf, size_t len, uint16_t port) {
    return sendTo(buf, len, broadcastAddr_, port);
}

void UdpSocket::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool UdpSocket::isValid() const noexcept { return fd_ >= 0; }

} // namespace prodjlink::platform

#endif // !_WIN32
