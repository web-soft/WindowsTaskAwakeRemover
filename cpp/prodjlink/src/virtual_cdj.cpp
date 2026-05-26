#include "../include/prodjlink/virtual_cdj.hpp"
#include "../include/prodjlink/device_finder.hpp"
#include "../include/prodjlink/cdj_status.hpp"
#include "../include/prodjlink/mixer_status.hpp"
#include "core/packet_buffer.hpp"
#include "core/packet_builder.hpp"
#include "platform/socket.hpp"
#include "platform/network_interface.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace prodjlink {

struct VirtualCdj::Impl {
    Config config;
    std::atomic<bool> running{false};
    uint8_t playerNum = 0;
    std::array<uint8_t, 4> myIp{};
    std::array<uint8_t, 6> myMac{};
    std::array<uint8_t, 4> broadcastAddr{255, 255, 255, 255};

    std::optional<platform::UdpSocket> announceSock;      // TX keep-alive
    std::optional<platform::UdpSocket> announceRecvSock;  // RX DeviceHello (EVS response)
    std::optional<platform::UdpSocket> statusSock;        // RX CDJ/Mixer status

    std::thread announceThread;
    std::thread announceRecvThread;
    std::thread receiveThread;

    std::mutex cbMutex;
    std::vector<CdjStatusCallback>   cdjCbs;
    std::vector<MixerStatusCallback> mixerCbs;
    std::vector<MasterCallback>      masterCbs;
    uint8_t lastMaster = 0;

    // Per-player DJState — lock order: stateMutex → DJState::mtx
    mutable std::mutex stateMutex;
    std::unordered_map<uint8_t, DJState> playerStates;
    uint8_t currentMaster = 0;

    // Welford online latency statistics
    mutable std::mutex statsMutex;
    LatencyStats latencyStats;
    double latencyM2 = 0.0;

    void updateLatency(double us) {
        std::lock_guard<std::mutex> lk(statsMutex);
        auto& s = latencyStats;
        ++s.packetCount;
        double delta  = us - s.avgLatencyUs;
        s.avgLatencyUs += delta / static_cast<double>(s.packetCount);
        double delta2  = us - s.avgLatencyUs;
        latencyM2     += delta * delta2;
        if (s.packetCount > 1)
            s.jitterUs = std::sqrt(latencyM2 / static_cast<double>(s.packetCount - 1));
        if (us > s.maxLatencyUs) s.maxLatencyUs = us;
    }
};

VirtualCdj::VirtualCdj() : VirtualCdj(Config{}) {}

VirtualCdj::VirtualCdj(Config config) : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
}

VirtualCdj::~VirtualCdj() { stop(); }

bool VirtualCdj::start(const DeviceFinder& finder) {
    if (impl_->running) return true;

    auto iface = platform::pickInterface();
    if (!iface) return false;
    impl_->myIp          = iface->ipAddress;
    impl_->myMac         = iface->macAddress;
    impl_->broadcastAddr = iface->broadcastAddress;

    // Choose player number not already in use
    uint8_t preferredNum = impl_->config.preferredPlayerNumber;
    auto existingDevices = finder.getDevices();
    auto isTaken = [&](uint8_t n) {
        for (auto& d : existingDevices)
            if (d.playerNumber() == n) return true;
        return false;
    };
    uint8_t chosenNum = 0;
    for (uint8_t c = preferredNum; c <= 6 && chosenNum == 0; ++c)
        if (!isTaken(c)) chosenNum = c;
    for (uint8_t c = 1; c < preferredNum && chosenNum == 0; ++c)
        if (!isTaken(c)) chosenNum = c;
    if (chosenNum == 0) return false;
    impl_->playerNum = chosenNum;

    // TX socket for keep-alive broadcasts and control packets
    auto announceSock = platform::UdpSocket::create();
    if (!announceSock) return false;
    announceSock->setBroadcast(true);
    announceSock->setBroadcastAddr(impl_->broadcastAddr);
    impl_->announceSock = std::move(announceSock);

    // RX socket on PORT_ANNOUNCEMENT to detect new devices (EVS response)
    {
        auto s = platform::UdpSocket::create();
        if (s) {
            s->setReuseAddress(true);
            s->setReceiveTimeout(std::chrono::milliseconds(500));
            if (s->bind(PORT_ANNOUNCEMENT, platform::UdpSocket::BindMode::Broadcast))
                impl_->announceRecvSock = std::move(s);
        }
    }

    const auto& name  = impl_->config.deviceName;
    const auto& mac   = impl_->myMac;
    const auto& ip    = impl_->myIp;
    const auto& bcast = impl_->broadcastAddr;

    // Full EVS claim sequence: Hello ×3, Stage1 ×3, Stage2 ×3, Stage3 ×3
    auto hello = detail::PacketBuilder::makeHello(name, chosenNum, mac, ip);
    for (int i = 0; i < 3; ++i) {
        impl_->announceSock->sendTo(hello.data(), hello.size(), bcast, PORT_ANNOUNCEMENT);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    for (int i = 1; i <= 3; ++i) {
        auto pkt = detail::PacketBuilder::makeClaimStage1(name, static_cast<uint8_t>(i), mac);
        impl_->announceSock->sendTo(pkt.data(), pkt.size(), bcast, PORT_ANNOUNCEMENT);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    for (int i = 1; i <= 3; ++i) {
        auto pkt = detail::PacketBuilder::makeClaimStage2(name, static_cast<uint8_t>(i), chosenNum, mac, ip);
        impl_->announceSock->sendTo(pkt.data(), pkt.size(), bcast, PORT_ANNOUNCEMENT);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    for (int i = 1; i <= 3; ++i) {
        auto pkt = detail::PacketBuilder::makeClaimStage3(name, static_cast<uint8_t>(i), chosenNum, mac);
        impl_->announceSock->sendTo(pkt.data(), pkt.size(), bcast, PORT_ANNOUNCEMENT);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Status RX socket
    auto statusSock = platform::UdpSocket::create();
    if (!statusSock) return false;
    statusSock->setReuseAddress(true);
    statusSock->setReceiveTimeout(std::chrono::milliseconds(500));
    if (!statusSock->bind(PORT_STATUS, platform::UdpSocket::BindMode::Broadcast))
        return false;
    impl_->statusSock = std::move(statusSock);

    impl_->running = true;

    // Thread 1: periodic keep-alive broadcast
    impl_->announceThread = std::thread([this]() {
        while (impl_->running) {
            auto pkt = detail::PacketBuilder::makeKeepAlive(
                impl_->config.deviceName, impl_->playerNum, impl_->myMac, impl_->myIp);
            impl_->announceSock->sendTo(pkt.data(), pkt.size(),
                                        impl_->broadcastAddr, PORT_ANNOUNCEMENT);
            std::this_thread::sleep_for(impl_->config.keepAliveInterval);
        }
    });

    // Thread 2: EVS response — reply to DeviceHello from new devices joining the network
    impl_->announceRecvThread = std::thread([this]() {
        if (!impl_->announceRecvSock) return;
        uint8_t buf[512];
        std::array<uint8_t, 4> srcAddr{};
        uint16_t srcPort = 0;
        while (impl_->running) {
            int n = impl_->announceRecvSock->recvFrom(buf, sizeof(buf), srcAddr, srcPort);
            if (n <= 0) continue;
            detail::PacketBuffer pkt(buf, static_cast<size_t>(n));
            if (!pkt.isValidHeader()) continue;
            if (pkt.type() == PacketType::DeviceHello) {
                // A device announced itself — reply immediately so it sees us
                auto reply = detail::PacketBuilder::makeKeepAlive(
                    impl_->config.deviceName, impl_->playerNum, impl_->myMac, impl_->myIp);
                impl_->announceSock->sendTo(reply.data(), reply.size(),
                                            impl_->broadcastAddr, PORT_ANNOUNCEMENT);
            }
        }
    });

    // Thread 3: receive CDJ/Mixer status with latency monitoring
    impl_->receiveThread = std::thread([this]() {
        uint8_t buf[2048];
        std::array<uint8_t, 4> srcAddr{};
        uint16_t srcPort = 0;
        while (impl_->running) {
            if (!impl_->statusSock) break;
            int n = impl_->statusSock->recvFrom(buf, sizeof(buf), srcAddr, srcPort);
            if (n <= 0) continue;

            auto t0 = std::chrono::high_resolution_clock::now();

            detail::PacketBuffer pkt(buf, static_cast<size_t>(n));
            if (!pkt.isValidHeader()) continue;

            if (pkt.type() == PacketType::CdjStatus) {
                auto status = CdjStatus::parse(buf, static_cast<size_t>(n));
                if (!status) continue;

                // Update per-player DJState
                {
                    std::lock_guard<std::mutex> lk(impl_->stateMutex);
                    auto& state = impl_->playerStates[status->playerNumber()];
                    state.playerID = status->playerNumber();
                    state.updateFull(
                        static_cast<float>(status->effectiveBpm()),
                        0.0f,
                        status->isPlaying(),
                        status->beatInBar(),
                        status->isMaster(),
                        status->isSynced(),
                        status->isOnAir(),
                        status->rekordboxId(),
                        status->trackNumber()
                    );
                    if (status->isMaster())
                        impl_->currentMaster = status->playerNumber();
                }

                if (status->isMaster() && status->playerNumber() != impl_->lastMaster) {
                    uint8_t newMaster = status->playerNumber();
                    std::vector<MasterCallback> cbs;
                    {
                        std::lock_guard<std::mutex> lk(impl_->cbMutex);
                        cbs              = impl_->masterCbs;
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

            auto dt = std::chrono::high_resolution_clock::now() - t0;
            impl_->updateLatency(static_cast<double>(
                std::chrono::duration_cast<std::chrono::microseconds>(dt).count()));
        }
    });

    return true;
}

void VirtualCdj::stop() {
    if (!impl_->running) return;
    impl_->running = false;
    if (impl_->statusSock)        impl_->statusSock->close();
    if (impl_->announceRecvSock)  impl_->announceRecvSock->close();
    if (impl_->announceThread.joinable())     impl_->announceThread.join();
    if (impl_->announceRecvThread.joinable()) impl_->announceRecvThread.join();
    if (impl_->receiveThread.joinable())      impl_->receiveThread.join();
}

bool VirtualCdj::isRunning() const noexcept { return impl_->running; }

uint8_t VirtualCdj::playerNumber() const noexcept { return impl_->playerNum; }

DJState VirtualCdj::masterState() const {
    std::lock_guard<std::mutex> lk(impl_->stateMutex);
    auto it = impl_->playerStates.find(impl_->currentMaster);
    if (it != impl_->playerStates.end())
        return it->second.snapshot();
    return DJState{};
}

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
    if (!impl_->running || !impl_->announceSock) return false;
    auto pkt = detail::PacketBuilder::makeMasterHandoff(
        impl_->config.deviceName, impl_->playerNum, targetPlayer);
    return impl_->announceSock->sendTo(pkt.data(), pkt.size(),
                                       impl_->broadcastAddr, PORT_STATUS);
}

VirtualCdj::LatencyStats VirtualCdj::getLatencyStats() const {
    std::lock_guard<std::mutex> lk(impl_->statsMutex);
    return impl_->latencyStats;
}

} // namespace prodjlink
