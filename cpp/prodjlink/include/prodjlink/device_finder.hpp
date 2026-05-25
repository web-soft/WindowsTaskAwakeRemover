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

    bool waitForDevices(std::chrono::milliseconds timeout);

    static constexpr std::chrono::seconds DEVICE_TIMEOUT{10};

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace prodjlink
