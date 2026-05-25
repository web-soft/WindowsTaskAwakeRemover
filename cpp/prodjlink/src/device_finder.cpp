#include "../include/prodjlink/device_finder.hpp"
#include "core/packet_buffer.hpp"
#include "platform/socket.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <utility>

namespace prodjlink {

struct DeviceFinder::Impl {
    std::optional<platform::UdpSocket> sock;
    std::atomic<bool> running{false};
    std::thread recvThread;
    std::thread dispatchThread;

    mutable std::mutex devMutex;
    std::unordered_map<uint8_t, DeviceAnnouncement> devices;
    std::unordered_map<uint8_t, std::chrono::steady_clock::time_point> lastSeen;

    std::mutex cbMutex;
    std::vector<DeviceCallback> foundCbs;
    std::vector<DeviceCallback> lostCbs;

    std::mutex qMutex;
    std::condition_variable qCv;
    // true = found, false = lost
    std::queue<std::pair<bool, DeviceAnnouncement>> dispatchQ;
};

DeviceFinder::DeviceFinder() : impl_(std::make_unique<Impl>()) {}

DeviceFinder::~DeviceFinder() { stop(); }

bool DeviceFinder::start() {
    if (impl_->running) return true;

    auto sock = platform::UdpSocket::create();
    if (!sock) return false;
    sock->setReuseAddress(true);
    sock->setReceiveTimeout(std::chrono::milliseconds(500));
    if (!sock->bind(PORT_ANNOUNCEMENT, platform::UdpSocket::BindMode::Broadcast))
        return false;

    impl_->sock = std::move(sock);
    impl_->running = true;

    impl_->dispatchThread = std::thread([this]() {
        while (impl_->running || !impl_->dispatchQ.empty()) {
            std::unique_lock<std::mutex> lk(impl_->qMutex);
            impl_->qCv.wait_for(lk, std::chrono::milliseconds(100),
                                [this]{ return !impl_->dispatchQ.empty(); });
            while (!impl_->dispatchQ.empty()) {
                auto [found, da] = impl_->dispatchQ.front();
                impl_->dispatchQ.pop();
                lk.unlock();
                std::vector<DeviceCallback> cbs;
                {
                    std::lock_guard<std::mutex> cbLk(impl_->cbMutex);
                    cbs = found ? impl_->foundCbs : impl_->lostCbs;
                }
                for (auto& cb : cbs) cb(da);
                lk.lock();
            }
        }
    });

    impl_->recvThread = std::thread([this]() {
        uint8_t buf[2048];
        std::array<uint8_t, 4> srcAddr{};
        uint16_t srcPort = 0;

        while (impl_->running) {
            if (!impl_->sock) break;
            int n = impl_->sock->recvFrom(buf, sizeof(buf), srcAddr, srcPort);
            if (n <= 0) {
                // Timeout or error — check for expired devices
                auto now = std::chrono::steady_clock::now();
                std::vector<std::pair<uint8_t, DeviceAnnouncement>> expired;
                {
                    std::lock_guard<std::mutex> lk(impl_->devMutex);
                    for (auto it = impl_->lastSeen.begin(); it != impl_->lastSeen.end(); ) {
                        if (now - it->second > DEVICE_TIMEOUT) {
                            auto devIt = impl_->devices.find(it->first);
                            if (devIt != impl_->devices.end()) {
                                expired.push_back({it->first, devIt->second});
                                impl_->devices.erase(devIt);
                            }
                            it = impl_->lastSeen.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                for (auto& [num, da] : expired) {
                    std::lock_guard<std::mutex> qLk(impl_->qMutex);
                    impl_->dispatchQ.push({false, da});
                    impl_->qCv.notify_one();
                }
                continue;
            }

            detail::PacketBuffer pkt(buf, static_cast<size_t>(n));
            if (!pkt.isValidHeader()) continue;
            if (pkt.type() != PacketType::DeviceKeepAlive) continue;

            auto da = DeviceAnnouncement::parse(buf, static_cast<size_t>(n));
            if (!da) continue;

            uint8_t num = da->playerNumber();
            bool isNew = false;
            {
                std::lock_guard<std::mutex> lk(impl_->devMutex);
                isNew = (impl_->devices.find(num) == impl_->devices.end());
                impl_->devices[num] = *da;
                impl_->lastSeen[num] = std::chrono::steady_clock::now();
            }
            if (isNew) {
                std::lock_guard<std::mutex> qLk(impl_->qMutex);
                impl_->dispatchQ.push({true, *da});
                impl_->qCv.notify_one();
            }
        }
    });

    return true;
}

void DeviceFinder::stop() {
    if (!impl_->running) return;
    impl_->running = false;
    if (impl_->sock) impl_->sock->close();
    if (impl_->recvThread.joinable())    impl_->recvThread.join();
    {
        std::lock_guard<std::mutex> lk(impl_->qMutex);
        impl_->qCv.notify_all();
    }
    if (impl_->dispatchThread.joinable()) impl_->dispatchThread.join();
}

bool DeviceFinder::isRunning() const noexcept { return impl_->running; }

std::vector<DeviceAnnouncement> DeviceFinder::getDevices() const {
    std::lock_guard<std::mutex> lk(impl_->devMutex);
    std::vector<DeviceAnnouncement> result;
    result.reserve(impl_->devices.size());
    for (auto& [num, da] : impl_->devices) result.push_back(da);
    return result;
}

std::optional<DeviceAnnouncement> DeviceFinder::getDevice(uint8_t playerNumber) const {
    std::lock_guard<std::mutex> lk(impl_->devMutex);
    auto it = impl_->devices.find(playerNumber);
    if (it == impl_->devices.end()) return std::nullopt;
    return it->second;
}

void DeviceFinder::onDeviceFound(DeviceCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->foundCbs.push_back(std::move(cb));
}

void DeviceFinder::onDeviceLost(DeviceCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->lostCbs.push_back(std::move(cb));
}

bool DeviceFinder::waitForDevices(std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!getDevices().empty()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return !getDevices().empty();
}

} // namespace prodjlink
