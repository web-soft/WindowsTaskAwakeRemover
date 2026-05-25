#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include "cdj_status.hpp"
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

    explicit VirtualCdj(Config config = {});
    ~VirtualCdj();
    VirtualCdj(const VirtualCdj&) = delete;
    VirtualCdj& operator=(const VirtualCdj&) = delete;

    // Requires DeviceFinder to be running first
    bool start(const DeviceFinder& finder);
    void stop();
    bool isRunning() const noexcept;

    uint8_t playerNumber() const noexcept;

    // Callbacks called on receive thread — must return quickly
    void onCdjStatus(CdjStatusCallback cb);
    void onMixerStatus(MixerStatusCallback cb);
    void onMasterChanged(MasterCallback cb);

    bool requestSyncMode(uint8_t targetPlayer, bool enable);
    bool requestMaster(uint8_t targetPlayer);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace prodjlink
