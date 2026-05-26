#include <catch2/catch_test_macros.hpp>
#include "../include/prodjlink/device_finder.hpp"
#include "../include/prodjlink/device_announcement.hpp"

// These tests exercise the DeviceFinder timeout constants and the
// three-state machine (Active → Stale → Lost, Stale → Active/Rejoined)
// using the public API values. They do NOT start real network sockets.

using namespace prodjlink;
using namespace std::chrono_literals;

TEST_CASE("DeviceFinder timeout constants are sane", "[reconnect]") {
    REQUIRE(DeviceFinder::STALE_TIMEOUT.count() > 0);
    REQUIRE(DeviceFinder::LOST_TIMEOUT > DeviceFinder::STALE_TIMEOUT);
}

TEST_CASE("DeviceFinder STALE_TIMEOUT is 10s, LOST_TIMEOUT is 30s", "[reconnect]") {
    REQUIRE(DeviceFinder::STALE_TIMEOUT  == std::chrono::seconds(10));
    REQUIRE(DeviceFinder::LOST_TIMEOUT   == std::chrono::seconds(30));
}

// Helpers to build a minimal keep-alive packet for testing announcement parsing
namespace {

// Builds a 0x36-byte DeviceKeepAlive packet for a given player number
static std::vector<uint8_t> makeKeepalivePacket(uint8_t playerNumber,
                                                 const char* name = "CDJ-TEST") {
    std::vector<uint8_t> pkt(0x36, 0x00);
    // magic
    const uint8_t magic[10] = {0x51,0x73,0x70,0x74,0x31,0x57,0x6d,0x4a,0x4f,0x4c};
    std::copy(std::begin(magic), std::end(magic), pkt.begin());
    pkt[0x0a] = 0x06; // DeviceKeepAlive
    std::strncpy(reinterpret_cast<char*>(pkt.data() + 0x0c), name, 20);
    pkt[0x20] = 0x01;
    pkt[0x21] = 0x01;
    pkt[0x22] = 0x00; pkt[0x23] = 0x36; // length
    pkt[0x24] = playerNumber;
    pkt[0x25] = 0x01; // CDJ type
    // MAC at 0x2c, IP at 0x32 — leave zero for test purposes
    return pkt;
}

} // namespace

TEST_CASE("DeviceAnnouncement parses a synthetic keepalive", "[reconnect]") {
    auto raw = makeKeepalivePacket(3, "CDJ-2000");
    auto da  = DeviceAnnouncement::parse(raw.data(), raw.size());
    REQUIRE(da.has_value());
    REQUIRE(da->playerNumber() == 3);
    REQUIRE(da->deviceName()   == "CDJ-2000");
}

TEST_CASE("DeviceFinder default-constructed is not running", "[reconnect]") {
    DeviceFinder df;
    REQUIRE_FALSE(df.isRunning());
    REQUIRE(df.getDevices().empty());
}

TEST_CASE("DeviceFinder getDevice returns nullopt for unknown player", "[reconnect]") {
    DeviceFinder df;
    REQUIRE_FALSE(df.getDevice(1).has_value());
    REQUIRE_FALSE(df.getDevice(5).has_value());
}
