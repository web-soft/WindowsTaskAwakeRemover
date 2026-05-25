#include "../include/prodjlink/beat.hpp"
#include "core/packet_buffer.hpp"

namespace prodjlink {

Beat::Beat(std::vector<uint8_t> raw) : raw_(std::move(raw)) {
    detail::PacketBuffer pkt(raw_.data(), raw_.size());
    playerNum_  = pkt.u8(0x24);
    timings_[0] = pkt.u32be(0x28);
    timings_[1] = pkt.u32be(0x2c);
    timings_[2] = pkt.u32be(0x30);
    timings_[3] = pkt.u32be(0x34);
    timings_[4] = pkt.u32be(0x38);
    timings_[5] = pkt.u32be(0x3c);
    rawPitch_   = pkt.u24be(0x54);
    rawBpm_     = pkt.u16be(0x58);
    beatInBar_  = pkt.u8(0x5c);
}

std::optional<Beat> Beat::parse(const uint8_t* data, size_t len) {
    detail::PacketBuffer pkt(data, len);
    if (!pkt.isValidHeader()) return std::nullopt;
    if (pkt.type() != PacketType::Beat) return std::nullopt;
    if (len < 0x60) return std::nullopt;
    return Beat{std::vector<uint8_t>(data, data + len)};
}

double Beat::pitchPercent() const noexcept {
    return (static_cast<double>(rawPitch_) / 1048576.0) * 100.0 - 100.0;
}

double Beat::effectiveBpm() const noexcept {
    return bpm() * (1.0 + pitchPercent() / 100.0);
}

} // namespace prodjlink
