#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "types.hpp"

namespace prodjlink {

class CdjStatus {
public:
    static std::optional<CdjStatus> parse(const uint8_t* data, size_t len);

    uint8_t     playerNumber()        const noexcept;
    std::string deviceName()          const;
    bool        isActive()            const noexcept;
    uint8_t     sourcePlayer()        const noexcept;
    MediaSlot   sourceSlot()          const noexcept;
    TrackType   trackType()           const noexcept;
    uint32_t    rekordboxId()         const noexcept;
    uint16_t    trackNumber()         const noexcept;
    PlayState1  playState1()          const noexcept;
    PlayState2  playState2()          const noexcept;
    PlayState3  playState3()          const noexcept;
    bool        isPlaying()           const noexcept;
    bool        isMaster()            const noexcept;
    bool        isSynced()            const noexcept;
    bool        isOnAir()             const noexcept;
    uint16_t    rawBpm()              const noexcept;
    double      trackBpm()            const noexcept;
    uint32_t    rawPitch()            const noexcept;
    double      pitchPercent()        const noexcept;
    double      effectiveBpm()        const noexcept;
    uint8_t     beatInBar()           const noexcept;
    uint32_t    beatNumber()          const noexcept;
    uint8_t     masterHandoffTarget() const noexcept;
    uint16_t    cueCountdown()        const noexcept;
    bool        hasLoopInfo()         const noexcept;
    bool        isLooping()           const noexcept;
    uint32_t    loopStartMs()         const noexcept;
    uint32_t    loopEndMs()           const noexcept;
    const std::vector<uint8_t>& rawPacket() const noexcept;

private:
    explicit CdjStatus(std::vector<uint8_t> raw);
    std::vector<uint8_t> raw_;
};

} // namespace prodjlink
