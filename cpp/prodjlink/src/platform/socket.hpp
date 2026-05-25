#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

namespace prodjlink::platform {

class UdpSocket {
public:
    enum class BindMode { Unicast, Broadcast };

    static std::optional<UdpSocket> create();

    ~UdpSocket();
    UdpSocket(UdpSocket&&) noexcept;
    UdpSocket& operator=(UdpSocket&&) noexcept;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool bind(uint16_t port, BindMode mode = BindMode::Broadcast);
    bool setBroadcast(bool enable);
    bool setReuseAddress(bool enable);
    bool setReceiveTimeout(std::chrono::milliseconds ms);
    bool setTTL(int ttl);

    // >0 = bytes received, 0 = timeout, -1 = error
    int recvFrom(uint8_t* buf, size_t len,
                 std::array<uint8_t, 4>& srcAddr, uint16_t& srcPort);

    bool sendTo(const uint8_t* buf, size_t len,
                const std::array<uint8_t, 4>& dstAddr, uint16_t dstPort);

    bool broadcast(const uint8_t* buf, size_t len, uint16_t port);

    void close();
    bool isValid() const noexcept;

    std::array<uint8_t, 4> broadcastAddr() const noexcept { return broadcastAddr_; }
    void setBroadcastAddr(const std::array<uint8_t, 4>& addr) { broadcastAddr_ = addr; }

private:
#ifdef _WIN32
    explicit UdpSocket(uintptr_t s) noexcept;
    uintptr_t sock_;
    static constexpr uintptr_t INVALID_SOCK = ~uintptr_t(0);
#else
    explicit UdpSocket(int fd) noexcept;
    int fd_;
#endif
    std::array<uint8_t, 4> broadcastAddr_{255, 255, 255, 255};
};

} // namespace prodjlink::platform
