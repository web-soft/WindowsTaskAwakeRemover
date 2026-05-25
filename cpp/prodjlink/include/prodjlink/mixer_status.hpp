#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "types.hpp"

namespace prodjlink {

class MixerStatus {
public:
    static std::optional<MixerStatus> parse(const uint8_t* data, size_t len);

    uint8_t  playerNumber() const noexcept;
    uint16_t rawBpm()       const noexcept;
    double   bpm()          const noexcept;
    uint8_t  beatInBar()    const noexcept;
    bool     isChannelOnAir(uint8_t channel) const noexcept; // channel 1-4
    bool     isMaster()     const noexcept;
    bool     isSynced()     const noexcept;
    const std::vector<uint8_t>& rawPacket() const noexcept;

private:
    explicit MixerStatus(std::vector<uint8_t> raw);
    std::vector<uint8_t> raw_;
};

} // namespace prodjlink
