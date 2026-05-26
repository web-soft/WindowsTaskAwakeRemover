#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include "cdj_status.hpp"
#include "dj_state.hpp"
#include "mixer_status.hpp"
#include "types.hpp"

namespace prodjlink {

class DeviceFinder;

class VirtualCdj {
public:
    using CdjStatusCallback   = std::function<void(const CdjStatus&)>;
    using MixerStatusCallback = std::function<void(const MixerStatus&)>;
    using MasterCallback      = std::function<void(uint8_t newMasterPlayer)>;

    struct Config {
        uint8_t  preferredPlayerNumber = 5;
        std::chrono::milliseconds keepAliveInterval{1500};
        std::string deviceName = "prodjlink";
    };

    // Latency statistics for the status receive pipeline
    struct LatencyStats {
        uint64_t packetCount  = 0;
        double   avgLatencyUs = 0.0;
        double   maxLatencyUs = 0.0;
        double   jitterUs     = 0.0;  // rolling std-dev
    };

    VirtualCdj();
    explicit VirtualCdj(Config config);
    ~VirtualCdj();
    VirtualCdj(const VirtualCdj&) = delete;
    VirtualCdj& operator=(const VirtualCdj&) = delete;

    // DeviceFinder must be running before calling start()
    bool start(const DeviceFinder& finder);
    void stop();
    bool isRunning() const noexcept;

    uint8_t playerNumber() const noexcept;

    // Thread-safe snapshot of the current master CDJ state
    DJState masterState() const;

    // Callbacks called on receive thread — must return quickly (< 1ms)
    void onCdjStatus(CdjStatusCallback cb);
    void onMixerStatus(MixerStatusCallback cb);
    void onMasterChanged(MasterCallback cb);

    bool requestSyncMode(uint8_t targetPlayer, bool enable);
    bool requestMaster(uint8_t targetPlayer);

    LatencyStats getLatencyStats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace prodjlink
