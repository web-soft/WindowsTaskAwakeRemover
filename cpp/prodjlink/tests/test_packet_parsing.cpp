#include <catch2/catch_test_macros.hpp>
#include <prodjlink/beat.hpp>
#include <prodjlink/cdj_status.hpp>
#include <prodjlink/device_announcement.hpp>
#include <prodjlink/mixer_status.hpp>
#include <array>
#include <cstring>
#include <vector>

// Pro DJ Link magic header
static const uint8_t MAGIC[10] = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c
};

static std::vector<uint8_t> makeBeatPacket(uint8_t player, uint16_t bpm100,
                                            uint8_t beatInBar,
                                            uint32_t nextBeatMs) {
    std::vector<uint8_t> pkt(0x60, 0x00);
    std::memcpy(pkt.data(), MAGIC, 10);
    pkt[0x0a] = 0x28;  // Beat type
    pkt[0x24] = player;
    // Next beat time at 0x28 (big-endian uint32)
    pkt[0x28] = (nextBeatMs >> 24) & 0xff;
    pkt[0x29] = (nextBeatMs >> 16) & 0xff;
    pkt[0x2a] = (nextBeatMs >> 8)  & 0xff;
    pkt[0x2b] =  nextBeatMs        & 0xff;
    // BPM*100 at 0x58 (big-endian uint16)
    pkt[0x58] = (bpm100 >> 8) & 0xff;
    pkt[0x59] =  bpm100       & 0xff;
    // Beat in bar at 0x5c
    pkt[0x5c] = beatInBar;
    return pkt;
}

static std::vector<uint8_t> makeCdjStatusPacket(uint8_t player, uint16_t bpm100,
                                                  uint8_t beatInBar, uint8_t flags) {
    std::vector<uint8_t> pkt(0xd4, 0x00);
    std::memcpy(pkt.data(), MAGIC, 10);
    pkt[0x0a] = 0x0a;  // CDJ status type
    pkt[0x24] = player;
    // BPM*100 at 0x57-0x58
    pkt[0x57] = (bpm100 >> 8) & 0xff;
    pkt[0x58] =  bpm100       & 0xff;
    // Beat in bar at 0x6a
    pkt[0x6a] = beatInBar;
    // Flags at 0x68
    pkt[0x68] = flags;
    return pkt;
}

static std::vector<uint8_t> makeDeviceKeepAlive(uint8_t player, const char* name,
                                                 uint8_t devType) {
    std::vector<uint8_t> pkt(0x36, 0x00);
    std::memcpy(pkt.data(), MAGIC, 10);
    pkt[0x0a] = 0x06;  // KeepAlive type
    // Device name at 0x0c (20 bytes)
    std::strncpy(reinterpret_cast<char*>(pkt.data() + 0x0c), name, 20);
    pkt[0x24] = player;
    pkt[0x25] = devType;
    // MAC at 0x2c: dummy values
    pkt[0x2c] = 0xaa; pkt[0x2d] = 0xbb; pkt[0x2e] = 0xcc;
    pkt[0x2f] = 0xdd; pkt[0x30] = 0xee; pkt[0x31] = 0xff;
    // IP at 0x32: 192.168.1.player
    pkt[0x32] = 192; pkt[0x33] = 168; pkt[0x34] = 1; pkt[0x35] = player;
    return pkt;
}

TEST_CASE("Beat packet parsing", "[beat]") {
    auto pkt = makeBeatPacket(2, 12800, 3, 450);
    auto beat = prodjlink::Beat::parse(pkt.data(), pkt.size());

    REQUIRE(beat.has_value());
    REQUIRE(beat->playerNumber() == 2);
    REQUIRE(beat->rawBpm() == 12800);
    REQUIRE(beat->bpm() == 128.0);
    REQUIRE(beat->beatWithinBar() == 3);
    REQUIRE(beat->nextBeatMs() == 450);
}

TEST_CASE("Beat packet rejects too-short data", "[beat]") {
    auto pkt = makeBeatPacket(1, 12000, 1, 0);
    pkt.resize(0x50);  // too short (< 0x60)
    REQUIRE_FALSE(prodjlink::Beat::parse(pkt.data(), pkt.size()).has_value());
}

TEST_CASE("Beat packet rejects wrong type", "[beat]") {
    auto pkt = makeBeatPacket(1, 12000, 1, 0);
    pkt[0x0a] = 0x0a;  // wrong type
    REQUIRE_FALSE(prodjlink::Beat::parse(pkt.data(), pkt.size()).has_value());
}

TEST_CASE("Beat packet rejects bad magic", "[beat]") {
    auto pkt = makeBeatPacket(1, 12000, 1, 0);
    pkt[0] = 0x00;  // corrupt magic
    REQUIRE_FALSE(prodjlink::Beat::parse(pkt.data(), pkt.size()).has_value());
}

TEST_CASE("Beat effective BPM with neutral pitch", "[beat]") {
    // Neutral pitch = 0x100000 = 1048576 → pitchPercent = 0.0 → effectiveBpm == bpm
    auto pkt = makeBeatPacket(1, 13400, 1, 0);
    // Set neutral pitch (0x100000) at 0x54
    pkt[0x54] = 0x10; pkt[0x55] = 0x00; pkt[0x56] = 0x00;
    auto beat = prodjlink::Beat::parse(pkt.data(), pkt.size());
    REQUIRE(beat.has_value());
    REQUIRE(beat->bpm() == 134.0);
    REQUIRE(beat->pitchPercent() == 0.0);
    REQUIRE(beat->effectiveBpm() == 134.0);
}

TEST_CASE("CdjStatus packet parsing", "[cdj_status]") {
    // flags: bit6=playing, bit5=master, bit4=sync, bit3=onair → 0x78
    auto pkt = makeCdjStatusPacket(1, 12800, 2, 0x78);
    auto status = prodjlink::CdjStatus::parse(pkt.data(), pkt.size());

    REQUIRE(status.has_value());
    REQUIRE(status->playerNumber() == 1);
    REQUIRE(status->rawBpm() == 12800);
    REQUIRE(status->trackBpm() == 128.0);
    REQUIRE(status->beatInBar() == 2);
    REQUIRE(status->isPlaying());
    REQUIRE(status->isMaster());
    REQUIRE(status->isSynced());
    REQUIRE(status->isOnAir());
}

TEST_CASE("CdjStatus not playing", "[cdj_status]") {
    auto pkt = makeCdjStatusPacket(3, 10000, 1, 0x00);
    auto status = prodjlink::CdjStatus::parse(pkt.data(), pkt.size());
    REQUIRE(status.has_value());
    REQUIRE_FALSE(status->isPlaying());
    REQUIRE_FALSE(status->isMaster());
}

TEST_CASE("CdjStatus rejects too-short packet", "[cdj_status]") {
    auto pkt = makeCdjStatusPacket(1, 12800, 1, 0);
    pkt.resize(0xc0);  // < 0xd4
    REQUIRE_FALSE(prodjlink::CdjStatus::parse(pkt.data(), pkt.size()).has_value());
}

TEST_CASE("DeviceAnnouncement parsing", "[device_announcement]") {
    auto pkt = makeDeviceKeepAlive(3, "CDJ-2000NXS2", 0x01);
    auto da = prodjlink::DeviceAnnouncement::parse(pkt.data(), pkt.size());

    REQUIRE(da.has_value());
    REQUIRE(da->playerNumber() == 3);
    REQUIRE(da->deviceType() == prodjlink::DeviceType::CDJ);
    REQUIRE(da->deviceName() == "CDJ-2000NXS2");
    REQUIRE(da->ipString() == "192.168.1.3");
    REQUIRE(da->macString() == "aa:bb:cc:dd:ee:ff");
}

TEST_CASE("DeviceAnnouncement mixer type", "[device_announcement]") {
    auto pkt = makeDeviceKeepAlive(33, "DJM-900NXS2", 0x02);
    auto da = prodjlink::DeviceAnnouncement::parse(pkt.data(), pkt.size());
    REQUIRE(da.has_value());
    REQUIRE(da->deviceType() == prodjlink::DeviceType::Mixer);
}

TEST_CASE("DeviceAnnouncement rejects wrong type byte", "[device_announcement]") {
    auto pkt = makeDeviceKeepAlive(1, "CDJ", 0x01);
    pkt[0x0a] = 0x28;  // wrong type (Beat instead of KeepAlive)
    REQUIRE_FALSE(prodjlink::DeviceAnnouncement::parse(pkt.data(), pkt.size()).has_value());
}

TEST_CASE("Null packet is rejected by all parsers", "[robustness]") {
    uint8_t empty[2] = {0x00, 0x00};
    REQUIRE_FALSE(prodjlink::Beat::parse(empty, 2).has_value());
    REQUIRE_FALSE(prodjlink::CdjStatus::parse(empty, 2).has_value());
    REQUIRE_FALSE(prodjlink::DeviceAnnouncement::parse(empty, 2).has_value());
    REQUIRE_FALSE(prodjlink::MixerStatus::parse(empty, 2).has_value());
}
