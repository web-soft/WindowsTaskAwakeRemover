#include "../include/prodjlink/beat_finder.hpp"
#include "core/packet_buffer.hpp"
#include "platform/socket.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace prodjlink {

struct BeatFinder::Impl {
    std::optional<platform::UdpSocket> sock;
    std::atomic<bool> running{false};
    std::thread recvThread;
    std::mutex cbMutex;
    std::vector<BeatCallback> beatCbs;
};

BeatFinder::BeatFinder() : impl_(std::make_unique<Impl>()) {}

BeatFinder::~BeatFinder() { stop(); }

bool BeatFinder::start() {
    if (impl_->running) return true;

    auto sock = platform::UdpSocket::create();
    if (!sock) return false;
    sock->setReuseAddress(true);
    sock->setReceiveTimeout(std::chrono::milliseconds(500));
    if (!sock->bind(PORT_BEAT, platform::UdpSocket::BindMode::Broadcast))
        return false;

    impl_->sock = std::move(sock);
    impl_->running = true;

    impl_->recvThread = std::thread([this]() {
        uint8_t buf[2048];
        std::array<uint8_t, 4> srcAddr{};
        uint16_t srcPort = 0;

        while (impl_->running) {
            if (!impl_->sock) break;
            int n = impl_->sock->recvFrom(buf, sizeof(buf), srcAddr, srcPort);
            if (n <= 0) continue;

            auto beat = Beat::parse(buf, static_cast<size_t>(n));
            if (!beat) continue;

            std::vector<BeatCallback> cbs;
            {
                std::lock_guard<std::mutex> lk(impl_->cbMutex);
                cbs = impl_->beatCbs;
            }
            for (auto& cb : cbs) cb(*beat);
        }
    });

    return true;
}

void BeatFinder::stop() {
    if (!impl_->running) return;
    impl_->running = false;
    if (impl_->sock) impl_->sock->close();
    if (impl_->recvThread.joinable()) impl_->recvThread.join();
}

bool BeatFinder::isRunning() const noexcept { return impl_->running; }

void BeatFinder::onBeat(BeatCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->beatCbs.push_back(std::move(cb));
}

} // namespace prodjlink
