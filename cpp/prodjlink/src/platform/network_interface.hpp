#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace prodjlink::platform {

struct NetworkInterface {
    std::string name;
    std::array<uint8_t, 4> ipAddress{};
    std::array<uint8_t, 4> subnetMask{};
    std::array<uint8_t, 4> broadcastAddress{};
    std::array<uint8_t, 6> macAddress{};
    bool isLoopback = false;
};

std::vector<NetworkInterface> enumerateInterfaces();
std::optional<NetworkInterface> pickInterface();

} // namespace prodjlink::platform
