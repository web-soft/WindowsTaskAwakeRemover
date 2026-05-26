#include "../include/prodjlink/mixer_status.hpp"
#include "core/packet_buffer.hpp"
#include "core/pioneer_packets.hpp"
#include "core/endian.hpp"

namespace prodjlink {

using namespace packets;
using namespace detail;

static inline const MixerStatusPacket* pkt(const std::vector<uint8_t>& raw) {
    return reinterpret_cast<const MixerStatusPacket*>(raw.data());
}

MixerStatus::MixerStatus(std::vector<uint8_t> raw) : raw_(std::move(raw)) {}

std::optional<MixerStatus> MixerStatus::parse(const uint8_t* data, size_t len) {
    PacketBuffer buf(data, len);
    if (!buf.isValidHeader()) return std::nullopt;
    if (buf.type() != PacketType::MixerStatus) return std::nullopt;
    if (len < sizeof(MixerStatusPacket)) return std::nullopt;
    return MixerStatus{std::vector<uint8_t>(data, data + len)};
}

uint8_t MixerStatus::playerNumber() const noexcept {
    return raw_.size() >= sizeof(Header) ? pkt(raw_)->hdr.playerNumber : 0;
}

uint16_t MixerStatus::rawBpm() const noexcept {
    if (raw_.size() < sizeof(MixerStatusPacket)) return 0;
    return (uint16_t(pkt(raw_)->bpm100Hi) << 8) | pkt(raw_)->bpm100Lo;
}

double MixerStatus::bpm() const noexcept { return rawBpm() / 100.0; }

uint8_t MixerStatus::beatInBar() const noexcept {
    return raw_.size() >= sizeof(MixerStatusPacket) ? pkt(raw_)->beatInBar : 0;
}

bool MixerStatus::isChannelOnAir(uint8_t channel) const noexcept {
    if (channel < 1 || channel > 4) return false;
    if (raw_.size() < sizeof(MixerStatusPacket)) return false;
    return (pkt(raw_)->onAirFlags & (1 << (channel - 1))) != 0;
}

bool MixerStatus::isMaster() const noexcept {
    return raw_.size() >= sizeof(MixerStatusPacket)
           && (pkt(raw_)->syncFlags & 0x20) != 0;
}

bool MixerStatus::isSynced() const noexcept {
    return raw_.size() >= sizeof(MixerStatusPacket)
           && (pkt(raw_)->syncFlags & 0x10) != 0;
}

const std::vector<uint8_t>& MixerStatus::rawPacket() const noexcept { return raw_; }

} // namespace prodjlink
