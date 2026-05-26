#include "../include/prodjlink/cdj_status.hpp"
#include "core/packet_buffer.hpp"
#include "core/pioneer_packets.hpp"
#include "core/endian.hpp"
#include <cstring>

namespace prodjlink {

using namespace packets;
using namespace detail;

static inline const CdjStatusPacket* pkt(const std::vector<uint8_t>& raw) {
    return reinterpret_cast<const CdjStatusPacket*>(raw.data());
}

CdjStatus::CdjStatus(std::vector<uint8_t> raw) : raw_(std::move(raw)) {}

std::optional<CdjStatus> CdjStatus::parse(const uint8_t* data, size_t len) {
    PacketBuffer buf(data, len);
    if (!buf.isValidHeader()) return std::nullopt;
    if (buf.type() != PacketType::CdjStatus) return std::nullopt;
    if (len < 0xd4) return std::nullopt;
    return CdjStatus{std::vector<uint8_t>(data, data + len)};
}

uint8_t CdjStatus::playerNumber() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, hdr.playerNumber)
           ? pkt(raw_)->hdr.playerNumber : 0;
}

std::string CdjStatus::deviceName() const {
    if (raw_.size() < sizeof(Header)) return {};
    return std::string(pkt(raw_)->hdr.deviceName,
                       strnlen(pkt(raw_)->hdr.deviceName, 20));
}

bool CdjStatus::isActive() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, activity)
           && pkt(raw_)->activity != 0;
}

uint8_t CdjStatus::sourcePlayer() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, sourcePlayer)
           ? pkt(raw_)->sourcePlayer : 0;
}

MediaSlot CdjStatus::sourceSlot() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, sourceSlot)
           ? static_cast<MediaSlot>(pkt(raw_)->sourceSlot) : MediaSlot::None;
}

TrackType CdjStatus::trackType() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, trackType)
           ? static_cast<TrackType>(pkt(raw_)->trackType) : TrackType::None;
}

uint32_t CdjStatus::rekordboxId() const noexcept {
    if (raw_.size() < offsetof(CdjStatusPacket, rekordboxId) + 4) return 0;
    return readU32BE(pkt(raw_)->rekordboxId);
}

uint16_t CdjStatus::trackNumber() const noexcept {
    if (raw_.size() < offsetof(CdjStatusPacket, trackNumber) + 2) return 0;
    return readU16BE(pkt(raw_)->trackNumber);
}

PlayState1 CdjStatus::playState1() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, playState1)
           ? static_cast<PlayState1>(pkt(raw_)->playState1) : PlayState1::Unknown;
}

PlayState2 CdjStatus::playState2() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, playState2)
           ? static_cast<PlayState2>(pkt(raw_)->playState2) : PlayState2::Unknown;
}

PlayState3 CdjStatus::playState3() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, playState3)
           ? static_cast<PlayState3>(pkt(raw_)->playState3) : PlayState3::Unknown;
}

bool CdjStatus::isPlaying() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, flags)
           && (pkt(raw_)->flags & 0x40) != 0;
}

bool CdjStatus::isMaster() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, flags)
           && (pkt(raw_)->flags & 0x20) != 0;
}

bool CdjStatus::isSynced() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, flags)
           && (pkt(raw_)->flags & 0x10) != 0;
}

bool CdjStatus::isOnAir() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, flags)
           && (pkt(raw_)->flags & 0x08) != 0;
}

uint16_t CdjStatus::rawBpm() const noexcept {
    if (raw_.size() < offsetof(CdjStatusPacket, bpm100Lo) + 1) return 0;
    return (uint16_t(pkt(raw_)->bpm100Hi) << 8) | pkt(raw_)->bpm100Lo;
}

double CdjStatus::trackBpm() const noexcept { return rawBpm() / 100.0; }

uint32_t CdjStatus::rawPitch() const noexcept {
    if (raw_.size() < offsetof(CdjStatusPacket, pitch1) + 4) return 0x100000;
    return readU32BE(pkt(raw_)->pitch1);
}

double CdjStatus::pitchPercent() const noexcept {
    return (static_cast<double>(rawPitch()) / 1048576.0) * 100.0 - 100.0;
}

double CdjStatus::effectiveBpm() const noexcept {
    return trackBpm() * (1.0 + pitchPercent() / 100.0);
}

uint8_t CdjStatus::beatInBar() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, beatInBar)
           ? pkt(raw_)->beatInBar : 0;
}

uint32_t CdjStatus::beatNumber() const noexcept {
    if (raw_.size() < offsetof(CdjStatusPacket, beatNum) + 4) return 0;
    return readU32BE(pkt(raw_)->beatNum);
}

uint8_t CdjStatus::masterHandoffTarget() const noexcept {
    return raw_.size() > offsetof(CdjStatusPacket, masterTarget)
           ? pkt(raw_)->masterTarget : 0;
}

uint16_t CdjStatus::cueCountdown() const noexcept {
    if (raw_.size() < offsetof(CdjStatusPacket, cue) + 2) return 0x01ff;
    return readU16BE(pkt(raw_)->cue);
}

bool CdjStatus::hasLoopInfo() const noexcept { return raw_.size() >= 0x130; }

bool CdjStatus::isLooping() const noexcept {
    return hasLoopInfo() && raw_[0x124] != 0;
}

uint32_t CdjStatus::loopStartMs() const noexcept {
    if (!hasLoopInfo()) return 0;
    return readU32BE(raw_.data() + 0x128);
}

uint32_t CdjStatus::loopEndMs() const noexcept {
    if (!hasLoopInfo()) return 0;
    return readU32BE(raw_.data() + 0x12c);
}

const std::vector<uint8_t>& CdjStatus::rawPacket() const noexcept { return raw_; }

} // namespace prodjlink
