#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include "types.hpp"

namespace prodjlink {

class DeviceAnnouncement {
public:
    static std::optional<DeviceAnnouncement> parse(const uint8_t* data, size_t len);

    const std::string&            deviceName()   const noexcept { return name_; }
    uint8_t                       playerNumber() const noexcept { return playerNum_; }
    DeviceType                    deviceType()   const noexcept { return type_; }
    const std::array<uint8_t, 4>& ipAddress()    const noexcept { return ip_; }
    const std::array<uint8_t, 6>& macAddress()   const noexcept { return mac_; }

    std::string ipString()  const;
    std::string macString() const;

private:
    DeviceAnnouncement() = default;
    std::string           name_;
    uint8_t               playerNum_ = 0;
    DeviceType            type_      = DeviceType::Unknown;
    std::array<uint8_t,4> ip_{};
    std::array<uint8_t,6> mac_{};
};

} // namespace prodjlink
