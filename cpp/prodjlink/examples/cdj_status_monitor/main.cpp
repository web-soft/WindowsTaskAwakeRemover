#include <prodjlink/prodjlink.hpp>
#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <thread>

static std::atomic<bool> running{true};

static const char* playStateStr(prodjlink::PlayState1 s) {
    using P = prodjlink::PlayState1;
    switch (s) {
        case P::Playing:   return "Playing";
        case P::Paused:    return "Paused";
        case P::Cued:      return "Cued";
        case P::Loading:   return "Loading";
        case P::Looping:   return "Looping";
        case P::Searching: return "Searching";
        case P::Ended:     return "Ended";
        default:           return "Unknown";
    }
}

int main() {
    std::signal(SIGINT, [](int) { running = false; });

    prodjlink::DeviceFinder finder;
    prodjlink::VirtualCdj   vcdj;

    finder.onDeviceFound([](const prodjlink::DeviceAnnouncement& d) {
        std::cout << "[+] Found: " << d.deviceName()
                  << "  player=" << static_cast<int>(d.playerNumber())
                  << "  ip=" << d.ipString() << "\n";
    });

    if (!finder.start()) { std::cerr << "Failed to start DeviceFinder\n"; return 1; }

    std::cout << "Waiting for Pioneer devices (5s)...\n";
    finder.waitForDevices(std::chrono::milliseconds(5000));

    vcdj.onCdjStatus([](const prodjlink::CdjStatus& s) {
        std::cout << std::fixed << std::setprecision(2)
                  << "CDJ[" << static_cast<int>(s.playerNumber()) << "]"
                  << "  state=" << playStateStr(s.playState1())
                  << "  bpm=" << s.effectiveBpm()
                  << "  bar=" << static_cast<int>(s.beatInBar()) << "/4"
                  << (s.isMaster() ? "  [MASTER]" : "")
                  << (s.isSynced() ? "  [SYNC]"   : "")
                  << (s.isOnAir()  ? "  [ON AIR]" : "")
                  << "\n";
    });

    vcdj.onMixerStatus([](const prodjlink::MixerStatus& s) {
        std::cout << "MIXER bpm=" << std::fixed << std::setprecision(2)
                  << s.bpm()
                  << "  beat=" << static_cast<int>(s.beatInBar()) << "/4\n";
    });

    vcdj.onMasterChanged([](uint8_t master) {
        std::cout << "Master changed to player " << static_cast<int>(master) << "\n";
    });

    if (!vcdj.start(finder)) {
        std::cerr << "Failed to start VirtualCdj (no network interface or all player slots taken)\n";
        return 1;
    }

    std::cout << "VirtualCDJ running as player "
              << static_cast<int>(vcdj.playerNumber())
              << "  — press Ctrl+C to quit\n";

    while (running) std::this_thread::sleep_for(std::chrono::milliseconds(100));

    vcdj.stop();
    finder.stop();
    return 0;
}
