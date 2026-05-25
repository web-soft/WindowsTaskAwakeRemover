#include <prodjlink/prodjlink.hpp>
#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <mutex>

static std::atomic<bool> running{true};

int main() {
    std::signal(SIGINT, [](int) { running = false; });

    prodjlink::DeviceFinder finder;

    finder.onDeviceFound([](const prodjlink::DeviceAnnouncement& d) {
        std::cout << "[+] " << d.deviceName()
                  << "  player=" << static_cast<int>(d.playerNumber())
                  << "  " << d.ipString() << "\n";
    });
    finder.onDeviceLost([](const prodjlink::DeviceAnnouncement& d) {
        std::cout << "[-] Lost player " << static_cast<int>(d.playerNumber()) << "\n";
    });

    if (!finder.start()) { std::cerr << "Failed to start DeviceFinder\n"; return 1; }

    std::cout << "Scanning for Pioneer Pro DJ Link devices (5s)...\n";
    bool found = finder.waitForDevices(std::chrono::milliseconds(5000));
    if (!found) {
        std::cerr << "No Pioneer devices found on the network.\n"
                  << "Make sure CDJs/XDJs are on the same subnet.\n";
        finder.stop();
        return 1;
    }

    prodjlink::VirtualCdj::Config cfg;
    cfg.preferredPlayerNumber = 5;
    cfg.deviceName = "prodjlink";

    prodjlink::VirtualCdj vcdj(cfg);

    // Track rekordbox ID changes per player
    std::mutex trackMutex;
    std::unordered_map<uint8_t, uint32_t> lastTrack;

    vcdj.onCdjStatus([&](const prodjlink::CdjStatus& s) {
        uint8_t  player = s.playerNumber();
        uint32_t rbId   = s.rekordboxId();
        {
            std::lock_guard<std::mutex> lk(trackMutex);
            auto it = lastTrack.find(player);
            if (it == lastTrack.end() || it->second != rbId) {
                lastTrack[player] = rbId;
                std::cout << "Player " << static_cast<int>(player)
                          << " track changed → rekordboxId=" << rbId
                          << "  bpm=" << std::fixed << std::setprecision(2)
                          << s.trackBpm()
                          << (s.isPlaying() ? "  PLAYING" : "  STOPPED")
                          << "\n";
            }
        }
    });

    vcdj.onMasterChanged([](uint8_t master) {
        std::cout << ">>> Master is now player " << static_cast<int>(master) << "\n";
    });

    if (!vcdj.start(finder)) {
        std::cerr << "Failed to claim a player number (all 1-6 slots taken?)\n";
        finder.stop();
        return 1;
    }

    std::cout << "Virtual CDJ active as player "
              << static_cast<int>(vcdj.playerNumber())
              << "  — press Ctrl+C to quit\n";

    while (running) std::this_thread::sleep_for(std::chrono::milliseconds(100));

    vcdj.stop();
    finder.stop();
    return 0;
}
