#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "endian.hpp"
#include "../../include/prodjlink/types.hpp"

namespace prodjlink::detail {

constexpr size_t DEVICE_NAME_MAX = 20;

class PacketBuilder {
public:
    static std::vector<uint8_t> makeBasePacket(uint8_t type,
                                               const std::string& deviceName,
                                               uint8_t subtype,
                                               uint16_t totalLength,
                                               uint8_t deviceNumber) {
        std::vector<uint8_t> pkt(totalLength, 0x00);
        std::memcpy(pkt.data(), PACKET_MAGIC, 10);
        pkt[0x0a] = type;
        pkt[0x0b] = 0x00;
        size_t nameLen = std::min(deviceName.size(), DEVICE_NAME_MAX);
        std::memcpy(pkt.data() + 0x0c, deviceName.c_str(), nameLen);
        pkt[0x20] = 0x01;
        pkt[0x21] = subtype;
        detail::writeU16BE(pkt.data() + 0x22, totalLength);
        pkt[0x24] = deviceNumber;
        return pkt;
    }

    static std::vector<uint8_t> makeHello(const std::string& name,
                                          uint8_t deviceNumber,
                                          const std::array<uint8_t, 6>& mac,
                                          const std::array<uint8_t, 4>& ip) {
        constexpr uint16_t LEN = 0x2c;
        auto pkt = makeBasePacket(0x0a, name, 0x01, LEN, deviceNumber);
        std::memcpy(pkt.data() + 0x24, ip.data(), 4);
        std::memcpy(pkt.data() + 0x28, mac.data(), 6);
        return pkt;
    }

    static std::vector<uint8_t> makeKeepAlive(const std::string& name,
                                              uint8_t deviceNumber,
                                              const std::array<uint8_t, 6>& mac,
                                              const std::array<uint8_t, 4>& ip) {
        constexpr uint16_t LEN = 0x36;
        auto pkt = makeBasePacket(0x06, name, 0x01, LEN, deviceNumber);
        pkt[0x25] = 0x01; // CDJ type
        std::memcpy(pkt.data() + 0x2c, mac.data(), 6);
        std::memcpy(pkt.data() + 0x30, ip.data(), 4);
        return pkt;
    }

    static std::vector<uint8_t> makeClaimStage1(const std::string& name,
                                                uint8_t counter,
                                                const std::array<uint8_t, 6>& mac) {
        constexpr uint16_t LEN = 0x2c;
        auto pkt = makeBasePacket(0x00, name, 0x01, LEN, 0x00);
        pkt[0x24] = counter;
        std::memcpy(pkt.data() + 0x26, mac.data(), 6);
        return pkt;
    }

    static std::vector<uint8_t> makeClaimStage2(const std::string& name,
                                                uint8_t counter,
                                                uint8_t deviceNumber,
                                                const std::array<uint8_t, 6>& mac,
                                                const std::array<uint8_t, 4>& ip) {
        constexpr uint16_t LEN = 0x32;
        auto pkt = makeBasePacket(0x02, name, 0x01, LEN, deviceNumber);
        pkt[0x24] = counter;
        std::memcpy(pkt.data() + 0x26, ip.data(), 4);
        std::memcpy(pkt.data() + 0x2a, mac.data(), 6);
        pkt[0x30] = deviceNumber;
        pkt[0x31] = 0x01;
        return pkt;
    }

    static std::vector<uint8_t> makeClaimStage3(const std::string& name,
                                                uint8_t counter,
                                                uint8_t deviceNumber,
                                                const std::array<uint8_t, 6>& mac) {
        constexpr uint16_t LEN = 0x26;
        auto pkt = makeBasePacket(0x04, name, 0x01, LEN, deviceNumber);
        pkt[0x24] = counter;
        std::memcpy(pkt.data() + 0x25, mac.data(), 6);
        if (pkt.size() > 0x2b) pkt[0x2b] = deviceNumber;
        return pkt;
    }

    static std::vector<uint8_t> makeSyncControl(const std::string& name,
                                                uint8_t fromDevice,
                                                uint8_t targetDevice,
                                                bool enable) {
        constexpr uint16_t LEN = 0x2c;
        auto pkt = makeBasePacket(0x2a, name, 0x01, LEN, fromDevice);
        pkt[0x25] = targetDevice;
        pkt[0x26] = enable ? 0x10 : 0x20;
        return pkt;
    }

    // 0x26 — MasterHandoff: ask targetDevice to become master
    static std::vector<uint8_t> makeMasterHandoff(const std::string& name,
                                                   uint8_t fromDevice,
                                                   uint8_t targetDevice) {
        constexpr uint16_t LEN = 0x28;
        auto pkt = makeBasePacket(0x26, name, 0x01, LEN, fromDevice);
        pkt[0x25] = targetDevice;
        pkt[0x26] = fromDevice;
        return pkt;
    }
};

} // namespace prodjlink::detail
