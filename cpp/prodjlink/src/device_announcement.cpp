#include "../include/prodjlink/device_announcement.hpp"
#include "core/packet_buffer.hpp"
#include <iomanip>
#include <sstream>

namespace prodjlink {

std::optional<DeviceAnnouncement> DeviceAnnouncement::parse(const uint8_t* data, size_t len) {
    detail::PacketBuffer pkt(data, len);
    if (!pkt.isValidHeader()) return std::nullopt;
    if (pkt.type() != PacketType::DeviceKeepAlive) return std::nullopt;
    if (len < 0x36) return std::nullopt;

    DeviceAnnouncement da;
    da.name_      = pkt.str(0x0c, 20);
    da.playerNum_ = pkt.u8(0x24);
    uint8_t dt    = pkt.u8(0x25);
    da.type_      = (dt == 0x01) ? DeviceType::CDJ
                  : (dt == 0x02) ? DeviceType::Mixer
                  : DeviceType::Unknown;
    for (int i = 0; i < 6; i++) da.mac_[i] = pkt.u8(static_cast<size_t>(0x2c + i));
    for (int i = 0; i < 4; i++) da.ip_[i]  = pkt.u8(static_cast<size_t>(0x32 + i));
    return da;
}

std::string DeviceAnnouncement::ipString() const {
    std::ostringstream oss;
    oss << static_cast<int>(ip_[0]) << '.'
        << static_cast<int>(ip_[1]) << '.'
        << static_cast<int>(ip_[2]) << '.'
        << static_cast<int>(ip_[3]);
    return oss.str();
}

std::string DeviceAnnouncement::macString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++) {
        if (i > 0) oss << ':';
        oss << std::setw(2) << static_cast<int>(mac_[i]);
    }
    return oss.str();
}

} // namespace prodjlink
