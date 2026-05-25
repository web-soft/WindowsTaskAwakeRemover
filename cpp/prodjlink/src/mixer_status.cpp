#include "../include/prodjlink/mixer_status.hpp"
#include "core/packet_buffer.hpp"

namespace prodjlink {

MixerStatus::MixerStatus(std::vector<uint8_t> raw) : raw_(std::move(raw)) {}

std::optional<MixerStatus> MixerStatus::parse(const uint8_t* data, size_t len) {
    detail::PacketBuffer pkt(data, len);
    if (!pkt.isValidHeader()) return std::nullopt;
    if (pkt.type() != PacketType::MixerStatus) return std::nullopt;
    if (len < 0x40) return std::nullopt;
    return MixerStatus{std::vector<uint8_t>(data, data + len)};
}

uint8_t MixerStatus::playerNumber() const noexcept {
    return raw_.size() > 0x24 ? raw_[0x24] : 0;
}

uint16_t MixerStatus::rawBpm() const noexcept {
    if (raw_.size() < 0x3b) return 0;
    return (uint16_t(raw_[0x39]) << 8) | raw_[0x3a];
}

double MixerStatus::bpm() const noexcept { return rawBpm() / 100.0; }

uint8_t MixerStatus::beatInBar() const noexcept {
    return raw_.size() > 0x3b ? raw_[0x3b] : 0;
}

bool MixerStatus::isChannelOnAir(uint8_t channel) const noexcept {
    if (channel < 1 || channel > 4) return false;
    if (raw_.size() <= 0x3c) return false;
    return (raw_[0x3c] & (1 << (channel - 1))) != 0;
}

bool MixerStatus::isMaster() const noexcept {
    return raw_.size() > 0x3d && (raw_[0x3d] & 0x20) != 0;
}

bool MixerStatus::isSynced() const noexcept {
    return raw_.size() > 0x3d && (raw_[0x3d] & 0x10) != 0;
}

const std::vector<uint8_t>& MixerStatus::rawPacket() const noexcept { return raw_; }

} // namespace prodjlink
