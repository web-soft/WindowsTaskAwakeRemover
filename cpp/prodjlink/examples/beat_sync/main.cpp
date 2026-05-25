#include <prodjlink/prodjlink.hpp>
#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <thread>

static std::atomic<bool> running{true};

int main() {
    std::signal(SIGINT, [](int) { running = false; });

    prodjlink::DeviceFinder finder;
    prodjlink::BeatFinder   beats;

    finder.onDeviceFound([](const prodjlink::DeviceAnnouncement& d) {
        std::cout << "[+] Device: " << d.deviceName()
                  << "  player=" << static_cast<int>(d.playerNumber())
                  << "  ip=" << d.ipString()
                  << "  mac=" << d.macString() << "\n";
    });

    finder.onDeviceLost([](const prodjlink::DeviceAnnouncement& d) {
        std::cout << "[-] Lost: " << d.deviceName()
                  << "  player=" << static_cast<int>(d.playerNumber()) << "\n";
    });

    beats.onBeat([](const prodjlink::Beat& b) {
        std::cout << std::fixed << std::setprecision(2)
                  << "BEAT  player=" << static_cast<int>(b.playerNumber())
                  << "  bpm=" << b.effectiveBpm()
                  << "  bar=" << static_cast<int>(b.beatWithinBar())
                  << "/4  next=" << b.nextBeatMs() << "ms\n";
    });

    if (!finder.start()) { std::cerr << "Failed to start DeviceFinder\n"; return 1; }
    if (!beats.start())  { std::cerr << "Failed to start BeatFinder\n";   return 1; }

    std::cout << "Pro DJ Link beat monitor — press Ctrl+C to quit\n";

    while (running) std::this_thread::sleep_for(std::chrono::milliseconds(100));

    finder.stop();
    beats.stop();
    return 0;
}
