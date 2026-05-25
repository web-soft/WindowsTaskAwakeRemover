#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "types.hpp"

namespace prodjlink {

class Beat {
public:
    static std::optional<Beat> parse(const uint8_t* data, size_t len);

    uint8_t  playerNumber()  const noexcept { return playerNum_; }
    uint16_t rawBpm()        const noexcept { return rawBpm_; }
    double   bpm()           const noexcept { return rawBpm_ / 100.0; }
    double   effectiveBpm()  const noexcept;
    uint32_t rawPitch()      const noexcept { return rawPitch_; }
    double   pitchPercent()  const noexcept;
    uint8_t  beatWithinBar() const noexcept { return beatInBar_; }

    uint32_t nextBeatMs()   const noexcept { return timings_[0]; }
    uint32_t secondBeatMs() const noexcept { return timings_[1]; }
    uint32_t nextBarMs()    const noexcept { return timings_[2]; }
    uint32_t fourthBeatMs() const noexcept { return timings_[3]; }
    uint32_t secondBarMs()  const noexcept { return timings_[4]; }
    uint32_t eighthBeatMs() const noexcept { return timings_[5]; }

    const std::vector<uint8_t>& rawPacket() const noexcept { return raw_; }

private:
    explicit Beat(std::vector<uint8_t> raw);
    std::vector<uint8_t> raw_;
    uint8_t  playerNum_ = 0;
    uint16_t rawBpm_    = 0;
    uint32_t rawPitch_  = 0x100000;
    uint8_t  beatInBar_ = 0;
    uint32_t timings_[6]{};
};

} // namespace prodjlink
