#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include "device_announcement.hpp"

namespace prodjlink {

class DeviceFinder {
public:
    using DeviceCallback = std::function<void(const DeviceAnnouncement&)>;

    DeviceFinder();
    ~DeviceFinder();
    DeviceFinder(const DeviceFinder&) = delete;
    DeviceFinder& operator=(const DeviceFinder&) = delete;

    bool start();
    void stop();
    bool isRunning() const noexcept;

    std::vector<DeviceAnnouncement> getDevices() const;
    std::optional<DeviceAnnouncement> getDevice(uint8_t playerNumber) const;

    void onDeviceFound(DeviceCallback cb);
    void onDeviceLost(DeviceCallback cb);
    // Called when a stale device sends a keep-alive again
    void onDeviceRejoined(DeviceCallback cb);

    bool waitForDevices(std::chrono::milliseconds timeout);

    // Active→Stale after STALE_TIMEOUT, Stale→Lost after LOST_TIMEOUT
    static constexpr std::chrono::seconds STALE_TIMEOUT{10};
    static constexpr std::chrono::seconds LOST_TIMEOUT{30};

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace prodjlink
