#include "../include/prodjlink/cdj_status.hpp"
#include "core/packet_buffer.hpp"
#include <cstring>

namespace prodjlink {

CdjStatus::CdjStatus(std::vector<uint8_t> raw) : raw_(std::move(raw)) {}

std::optional<CdjStatus> CdjStatus::parse(const uint8_t* data, size_t len) {
    detail::PacketBuffer pkt(data, len);
    if (!pkt.isValidHeader()) return std::nullopt;
    if (pkt.type() != PacketType::CdjStatus) return std::nullopt;
    if (len < 0xd4) return std::nullopt;
    return CdjStatus{std::vector<uint8_t>(data, data + len)};
}

uint8_t CdjStatus::playerNumber() const noexcept {
    return raw_.size() > 0x24 ? raw_[0x24] : 0;
}

std::string CdjStatus::deviceName() const {
    if (raw_.size() <= 0x0c) return {};
    const char* p = reinterpret_cast<const char*>(raw_.data() + 0x0c);
    return std::string(p, strnlen(p, 20));
}

bool CdjStatus::isActive() const noexcept {
    return raw_.size() > 0x27 && raw_[0x27] != 0;
}

uint8_t CdjStatus::sourcePlayer() const noexcept {
    return raw_.size() > 0x28 ? raw_[0x28] : 0;
}

MediaSlot CdjStatus::sourceSlot() const noexcept {
    return raw_.size() > 0x29 ? static_cast<MediaSlot>(raw_[0x29]) : MediaSlot::None;
}

TrackType CdjStatus::trackType() const noexcept {
    return raw_.size() > 0x2a ? static_cast<TrackType>(raw_[0x2a]) : TrackType::None;
}

uint32_t CdjStatus::rekordboxId() const noexcept {
    if (raw_.size() < 0x30) return 0;
    return (uint32_t(raw_[0x2c]) << 24) | (uint32_t(raw_[0x2d]) << 16) |
           (uint32_t(raw_[0x2e]) << 8)  |  uint32_t(raw_[0x2f]);
}

uint16_t CdjStatus::trackNumber() const noexcept {
    if (raw_.size() < 0x32) return 0;
    return (uint16_t(raw_[0x30]) << 8) | raw_[0x31];
}

PlayState1 CdjStatus::playState1() const noexcept {
    return raw_.size() > 0x44 ? static_cast<PlayState1>(raw_[0x44]) : PlayState1::Unknown;
}

PlayState2 CdjStatus::playState2() const noexcept {
    return raw_.size() > 0x45 ? static_cast<PlayState2>(raw_[0x45]) : PlayState2::Unknown;
}

PlayState3 CdjStatus::playState3() const noexcept {
    return raw_.size() > 0x46 ? static_cast<PlayState3>(raw_[0x46]) : PlayState3::Unknown;
}

bool CdjStatus::isPlaying() const noexcept {
    return raw_.size() > 0x68 && (raw_[0x68] & 0x40) != 0;
}

bool CdjStatus::isMaster() const noexcept {
    return raw_.size() > 0x68 && (raw_[0x68] & 0x20) != 0;
}

bool CdjStatus::isSynced() const noexcept {
    return raw_.size() > 0x68 && (raw_[0x68] & 0x10) != 0;
}

bool CdjStatus::isOnAir() const noexcept {
    return raw_.size() > 0x68 && (raw_[0x68] & 0x08) != 0;
}

uint16_t CdjStatus::rawBpm() const noexcept {
    if (raw_.size() < 0x59) return 0;
    return (uint16_t(raw_[0x57]) << 8) | raw_[0x58];
}

double CdjStatus::trackBpm() const noexcept { return rawBpm() / 100.0; }

uint32_t CdjStatus::rawPitch() const noexcept {
    if (raw_.size() < 0x4b) return 0x100000;
    return (uint32_t(raw_[0x47]) << 24) | (uint32_t(raw_[0x48]) << 16) |
           (uint32_t(raw_[0x49]) << 8)  |  uint32_t(raw_[0x4a]);
}

double CdjStatus::pitchPercent() const noexcept {
    return (static_cast<double>(rawPitch()) / 1048576.0) * 100.0 - 100.0;
}

double CdjStatus::effectiveBpm() const noexcept {
    return trackBpm() * (1.0 + pitchPercent() / 100.0);
}

uint8_t CdjStatus::beatInBar() const noexcept {
    return raw_.size() > 0x6a ? raw_[0x6a] : 0;
}

uint32_t CdjStatus::beatNumber() const noexcept {
    if (raw_.size() < 0x5d) return 0;
    return (uint32_t(raw_[0x59]) << 24) | (uint32_t(raw_[0x5a]) << 16) |
           (uint32_t(raw_[0x5b]) << 8)  |  uint32_t(raw_[0x5c]);
}

uint8_t CdjStatus::masterHandoffTarget() const noexcept {
    return raw_.size() > 0x84 ? raw_[0x84] : 0;
}

uint16_t CdjStatus::cueCountdown() const noexcept {
    if (raw_.size() < 0x5f) return 0x01ff;
    return (uint16_t(raw_[0x5d]) << 8) | raw_[0x5e];
}

bool CdjStatus::hasLoopInfo() const noexcept { return raw_.size() >= 0x130; }

bool CdjStatus::isLooping() const noexcept {
    return hasLoopInfo() && raw_[0x124] != 0;
}

uint32_t CdjStatus::loopStartMs() const noexcept {
    if (!hasLoopInfo()) return 0;
    return (uint32_t(raw_[0x128]) << 24) | (uint32_t(raw_[0x129]) << 16) |
           (uint32_t(raw_[0x12a]) << 8)  |  uint32_t(raw_[0x12b]);
}

uint32_t CdjStatus::loopEndMs() const noexcept {
    if (!hasLoopInfo()) return 0;
    return (uint32_t(raw_[0x12c]) << 24) | (uint32_t(raw_[0x12d]) << 16) |
           (uint32_t(raw_[0x12e]) << 8)  |  uint32_t(raw_[0x12f]);
}

const std::vector<uint8_t>& CdjStatus::rawPacket() const noexcept { return raw_; }

} // namespace prodjlink
