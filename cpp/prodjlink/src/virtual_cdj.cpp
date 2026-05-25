#include "../include/prodjlink/virtual_cdj.hpp"
#include "../include/prodjlink/device_finder.hpp"
#include "core/packet_buffer.hpp"
#include "core/packet_builder.hpp"
#include "platform/socket.hpp"
#include "platform/network_interface.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace prodjlink {

struct VirtualCdj::Impl {
    Config config;
    std::atomic<bool> running{false};
    uint8_t playerNum = 0;
    std::array<uint8_t, 4> myIp{};
    std::array<uint8_t, 6> myMac{};
    std::array<uint8_t, 4> broadcastAddr{255, 255, 255, 255};

    std::optional<platform::UdpSocket> announceSock;
    std::optional<platform::UdpSocket> statusSock;

    std::thread announceThread;
    std::thread receiveThread;

    std::mutex cbMutex;
    std::vector<CdjStatusCallback>   cdjCbs;
    std::vector<MixerStatusCallback> mixerCbs;
    std::vector<MasterCallback>      masterCbs;
    uint8_t lastMaster = 0;
};

VirtualCdj::VirtualCdj(Config config) : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
}

VirtualCdj::~VirtualCdj() { stop(); }

bool VirtualCdj::start(const DeviceFinder& finder) {
    if (impl_->running) return true;

    // Pick network interface
    auto iface = platform::pickInterface();
    if (!iface) return false;
    impl_->myIp  = iface->ipAddress;
    impl_->myMac = iface->macAddress;
    impl_->broadcastAddr = iface->broadcastAddress;

    // Determine player number (avoid conflicts with existing devices)
    uint8_t preferredNum = impl_->config.preferredPlayerNumber;
    auto existingDevices = finder.getDevices();
    auto isTaken = [&](uint8_t n) {
        for (auto& d : existingDevices)
            if (d.playerNumber() == n) return true;
        return false;
    };
    uint8_t chosenNum = 0;
    for (uint8_t candidate = preferredNum; candidate <= 6; candidate++) {
        if (!isTaken(candidate)) { chosenNum = candidate; break; }
    }
    if (chosenNum == 0) {
        for (uint8_t candidate = 1; candidate < preferredNum; candidate++) {
            if (!isTaken(candidate)) { chosenNum = candidate; break; }
        }
    }
    if (chosenNum == 0) return false; // all slots taken
    impl_->playerNum = chosenNum;

    // Create announce socket for sending
    auto announceSock = platform::UdpSocket::create();
    if (!announceSock) return false;
    announceSock->setBroadcast(true);
    announceSock->setBroadcastAddr(impl_->broadcastAddr);
    impl_->announceSock = std::move(announceSock);

    const auto& name = impl_->config.deviceName;
    const auto& mac  = impl_->myMac;
    const auto& ip   = impl_->myIp;
    const auto& bcast = impl_->broadcastAddr;

    // Claim stage 1: hello + 3x stage1
    auto hello = detail::PacketBuilder::makeHello(name, chosenNum, mac, ip);
    for (int i = 0; i < 3; i++) {
        impl_->announceSock->sendTo(hello.data(), hello.size(), bcast, PORT_ANNOUNCEMENT);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    for (int i = 1; i <= 3; i++) {
        auto pkt = detail::PacketBuilder::makeClaimStage1(name, static_cast<uint8_t>(i), mac);
        impl_->announceSock->sendTo(pkt.data(), pkt.size(), bcast, PORT_ANNOUNCEMENT);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    for (int i = 1; i <= 3; i++) {
        auto pkt = detail::PacketBuilder::makeClaimStage2(name, static_cast<uint8_t>(i), chosenNum, mac, ip);
        impl_->announceSock->sendTo(pkt.data(), pkt.size(), bcast, PORT_ANNOUNCEMENT);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    for (int i = 1; i <= 3; i++) {
        auto pkt = detail::PacketBuilder::makeClaimStage3(name, static_cast<uint8_t>(i), chosenNum, mac);
        impl_->announceSock->sendTo(pkt.data(), pkt.size(), bcast, PORT_ANNOUNCEMENT);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Create status receive socket
    auto statusSock = platform::UdpSocket::create();
    if (!statusSock) return false;
    statusSock->setReuseAddress(true);
    statusSock->setReceiveTimeout(std::chrono::milliseconds(500));
    if (!statusSock->bind(PORT_STATUS, platform::UdpSocket::BindMode::Broadcast))
        return false;
    impl_->statusSock = std::move(statusSock);

    impl_->running = true;

    // Announce thread: keep-alive broadcast
    impl_->announceThread = std::thread([this]() {
        while (impl_->running) {
            auto pkt = detail::PacketBuilder::makeKeepAlive(
                impl_->config.deviceName, impl_->playerNum, impl_->myMac, impl_->myIp);
            impl_->announceSock->sendTo(pkt.data(), pkt.size(),
                                        impl_->broadcastAddr, PORT_ANNOUNCEMENT);
            std::this_thread::sleep_for(impl_->config.keepAliveInterval);
        }
    });

    // Receive thread: CDJ/mixer status
    impl_->receiveThread = std::thread([this]() {
        uint8_t buf[2048];
        std::array<uint8_t, 4> srcAddr{};
        uint16_t srcPort = 0;

        while (impl_->running) {
            if (!impl_->statusSock) break;
            int n = impl_->statusSock->recvFrom(buf, sizeof(buf), srcAddr, srcPort);
            if (n <= 0) continue;

            detail::PacketBuffer pkt(buf, static_cast<size_t>(n));
            if (!pkt.isValidHeader()) continue;

            if (pkt.type() == PacketType::CdjStatus) {
                auto status = CdjStatus::parse(buf, static_cast<size_t>(n));
                if (!status) continue;

                // Check master change
                if (status->isMaster() && status->playerNumber() != impl_->lastMaster) {
                    uint8_t newMaster = status->playerNumber();
                    std::vector<MasterCallback> cbs;
                    {
                        std::lock_guard<std::mutex> lk(impl_->cbMutex);
                        cbs = impl_->masterCbs;
                        impl_->lastMaster = newMaster;
                    }
                    for (auto& cb : cbs) cb(newMaster);
                }

                std::vector<CdjStatusCallback> cbs;
                {
                    std::lock_guard<std::mutex> lk(impl_->cbMutex);
                    cbs = impl_->cdjCbs;
                }
                for (auto& cb : cbs) cb(*status);

            } else if (pkt.type() == PacketType::MixerStatus) {
                auto status = MixerStatus::parse(buf, static_cast<size_t>(n));
                if (!status) continue;

                std::vector<MixerStatusCallback> cbs;
                {
                    std::lock_guard<std::mutex> lk(impl_->cbMutex);
                    cbs = impl_->mixerCbs;
                }
                for (auto& cb : cbs) cb(*status);
            }
        }
    });

    return true;
}

void VirtualCdj::stop() {
    if (!impl_->running) return;
    impl_->running = false;
    if (impl_->statusSock)   impl_->statusSock->close();
    if (impl_->announceThread.joinable()) impl_->announceThread.join();
    if (impl_->receiveThread.joinable())  impl_->receiveThread.join();
}

bool VirtualCdj::isRunning() const noexcept { return impl_->running; }

uint8_t VirtualCdj::playerNumber() const noexcept { return impl_->playerNum; }

void VirtualCdj::onCdjStatus(CdjStatusCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->cdjCbs.push_back(std::move(cb));
}

void VirtualCdj::onMixerStatus(MixerStatusCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->mixerCbs.push_back(std::move(cb));
}

void VirtualCdj::onMasterChanged(MasterCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->masterCbs.push_back(std::move(cb));
}

bool VirtualCdj::requestSyncMode(uint8_t targetPlayer, bool enable) {
    if (!impl_->running || !impl_->announceSock) return false;
    auto pkt = detail::PacketBuilder::makeSyncControl(
        impl_->config.deviceName, impl_->playerNum, targetPlayer, enable);
    return impl_->announceSock->sendTo(pkt.data(), pkt.size(),
                                       impl_->broadcastAddr, PORT_BEAT);
}

bool VirtualCdj::requestMaster(uint8_t targetPlayer) {
    (void)targetPlayer;
    // Master handoff requires a more complex exchange; stub for now
    return false;
}

} // namespace prodjlink
