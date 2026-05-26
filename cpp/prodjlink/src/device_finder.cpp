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

enum class DeviceState { Active, Stale, Lost };

struct TrackedDevice {
    DeviceAnnouncement da;
    DeviceState state = DeviceState::Active;
    std::chrono::steady_clock::time_point lastSeen;
};

struct DeviceFinder::Impl {
    std::optional<platform::UdpSocket> sock;
    std::atomic<bool> running{false};
    std::thread recvThread;
    std::thread dispatchThread;

    mutable std::mutex devMutex;
    std::unordered_map<uint8_t, TrackedDevice> devices;

    std::mutex cbMutex;
    std::vector<DeviceCallback> foundCbs;
    std::vector<DeviceCallback> lostCbs;
    std::vector<DeviceCallback> rejoinedCbs;

    // Dispatch queue: enum tag + announcement
    enum class DispatchTag { Found, Lost, Rejoined };
    std::mutex qMutex;
    std::condition_variable qCv;
    std::queue<std::pair<DispatchTag, DeviceAnnouncement>> dispatchQ;
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

    impl_->sock    = std::move(sock);
    impl_->running = true;

    impl_->dispatchThread = std::thread([this]() {
        while (impl_->running || !impl_->dispatchQ.empty()) {
            std::unique_lock<std::mutex> lk(impl_->qMutex);
            impl_->qCv.wait_for(lk, std::chrono::milliseconds(100),
                                [this]{ return !impl_->dispatchQ.empty(); });
            while (!impl_->dispatchQ.empty()) {
                auto [tag, da] = impl_->dispatchQ.front();
                impl_->dispatchQ.pop();
                lk.unlock();
                std::vector<DeviceCallback> cbs;
                {
                    std::lock_guard<std::mutex> cbLk(impl_->cbMutex);
                    if      (tag == Impl::DispatchTag::Found)    cbs = impl_->foundCbs;
                    else if (tag == Impl::DispatchTag::Lost)     cbs = impl_->lostCbs;
                    else if (tag == Impl::DispatchTag::Rejoined) cbs = impl_->rejoinedCbs;
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
        auto now = std::chrono::steady_clock::now;

        while (impl_->running) {
            if (!impl_->sock) break;

            int n = impl_->sock->recvFrom(buf, sizeof(buf), srcAddr, srcPort);

            // On every loop iteration (recv or timeout), check for stale/lost
            {
                auto t = now();
                std::vector<std::pair<Impl::DispatchTag, DeviceAnnouncement>> events;
                {
                    std::lock_guard<std::mutex> lk(impl_->devMutex);
                    for (auto& [num, td] : impl_->devices) {
                        auto age = t - td.lastSeen;
                        if (td.state == DeviceState::Active &&
                            age > STALE_TIMEOUT) {
                            td.state = DeviceState::Stale;
                            // Stale = silent, no callback yet
                        } else if (td.state == DeviceState::Stale &&
                                   age > LOST_TIMEOUT) {
                            td.state = DeviceState::Lost;
                            events.push_back({Impl::DispatchTag::Lost, td.da});
                        }
                    }
                    // Remove Lost devices from map
                    for (auto it = impl_->devices.begin(); it != impl_->devices.end(); ) {
                        if (it->second.state == DeviceState::Lost)
                            it = impl_->devices.erase(it);
                        else
                            ++it;
                    }
                }
                if (!events.empty()) {
                    std::lock_guard<std::mutex> qLk(impl_->qMutex);
                    for (auto& ev : events) impl_->dispatchQ.push(ev);
                    impl_->qCv.notify_one();
                }
            }

            if (n <= 0) continue;

            detail::PacketBuffer pkt(buf, static_cast<size_t>(n));
            if (!pkt.isValidHeader()) continue;
            if (pkt.type() != PacketType::DeviceKeepAlive) continue;

            auto da = DeviceAnnouncement::parse(buf, static_cast<size_t>(n));
            if (!da) continue;

            uint8_t num = da->playerNumber();
            Impl::DispatchTag tag;
            bool dispatch = false;
            {
                std::lock_guard<std::mutex> lk(impl_->devMutex);
                auto it = impl_->devices.find(num);
                if (it == impl_->devices.end()) {
                    impl_->devices.emplace(num, TrackedDevice{*da, DeviceState::Active, now()});
                    tag = Impl::DispatchTag::Found;
                    dispatch = true;
                } else {
                    bool wasStale = (it->second.state == DeviceState::Stale);
                    it->second.da       = *da;
                    it->second.state    = DeviceState::Active;
                    it->second.lastSeen = now();
                    if (wasStale) { tag = Impl::DispatchTag::Rejoined; dispatch = true; }
                }
            }
            if (dispatch) {
                std::lock_guard<std::mutex> qLk(impl_->qMutex);
                impl_->dispatchQ.push({tag, *da});
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
    if (impl_->recvThread.joinable()) impl_->recvThread.join();
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
    for (auto& [num, td] : impl_->devices)
        if (td.state != DeviceState::Lost) result.push_back(td.da);
    return result;
}

std::optional<DeviceAnnouncement> DeviceFinder::getDevice(uint8_t playerNumber) const {
    std::lock_guard<std::mutex> lk(impl_->devMutex);
    auto it = impl_->devices.find(playerNumber);
    if (it == impl_->devices.end() || it->second.state == DeviceState::Lost)
        return std::nullopt;
    return it->second.da;
}

void DeviceFinder::onDeviceFound(DeviceCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->foundCbs.push_back(std::move(cb));
}

void DeviceFinder::onDeviceLost(DeviceCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->lostCbs.push_back(std::move(cb));
}

void DeviceFinder::onDeviceRejoined(DeviceCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->rejoinedCbs.push_back(std::move(cb));
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
