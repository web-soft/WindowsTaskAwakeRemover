#include "../include/prodjlink/beat.hpp"
#include "core/packet_buffer.hpp"
#include "core/pioneer_packets.hpp"
#include "core/endian.hpp"

namespace prodjlink {

using namespace packets;
using namespace detail;

Beat::Beat(std::vector<uint8_t> raw) : raw_(std::move(raw)) {
    const auto* p = reinterpret_cast<const BeatPacket*>(raw_.data());
    playerNum_  = p->hdr.playerNumber;
    timings_[0] = readU32BE(p->nextBeat);
    timings_[1] = readU32BE(p->beat2);
    timings_[2] = readU32BE(p->nextBar);
    timings_[3] = readU32BE(p->beat4);
    timings_[4] = readU32BE(p->bar2);
    timings_[5] = readU32BE(p->beat8);
    rawPitch_   = readU24BE(p->pitch);
    rawBpm_     = (uint16_t(p->bpm100Hi) << 8) | p->bpm100Lo;
    beatInBar_  = p->beatInBar;
}

std::optional<Beat> Beat::parse(const uint8_t* data, size_t len) {
    detail::PacketBuffer buf(data, len);
    if (!buf.isValidHeader()) return std::nullopt;
    if (buf.type() != PacketType::Beat) return std::nullopt;
    if (len < sizeof(BeatPacket)) return std::nullopt;
    return Beat{std::vector<uint8_t>(data, data + len)};
}

double Beat::pitchPercent() const noexcept {
    return (static_cast<double>(rawPitch_) / 1048576.0) * 100.0 - 100.0;
}

double Beat::effectiveBpm() const noexcept {
    return bpm() * (1.0 + pitchPercent() / 100.0);
}

} // namespace prodjlink
