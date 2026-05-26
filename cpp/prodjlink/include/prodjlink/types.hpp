#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace prodjlink {

constexpr uint16_t PORT_ANNOUNCEMENT = 50000;
constexpr uint16_t PORT_BEAT         = 50001;
constexpr uint16_t PORT_STATUS       = 50002;

constexpr uint8_t PACKET_MAGIC[10] = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c
};

constexpr size_t PACKET_MIN_SIZE = 11;

enum class PacketType : uint8_t {
    ClaimStage1     = 0x00,
    ClaimStage2     = 0x02,
    ClaimStage3     = 0x04,
    DeviceKeepAlive = 0x06,
    ClaimConflict   = 0x08,
    DeviceHello     = 0x0a,
    ChannelsOnAir   = 0x03,
    Beat            = 0x28,
    MasterHandoff   = 0x26,
    HandoffResponse = 0x27,
    SyncControl     = 0x2a,
    MediaQuery      = 0x05,
    CdjStatus       = 0x0a,
    LoadTrack       = 0x19,
    LoadTrackAck    = 0x1a,
    MixerStatus     = 0x29,
    Unknown         = 0xff,
};

enum class DeviceType : uint8_t {
    CDJ     = 0x01,
    Mixer   = 0x02,
    Unknown = 0xff,
};

enum class MediaSlot : uint8_t {
    None       = 0x00,
    CD         = 0x01,
    SD         = 0x02,
    USB        = 0x03,
    Collection = 0x04,
    USB2       = 0x07,
};

enum class TrackType : uint8_t {
    None       = 0x00,
    Rekordbox  = 0x01,
    Unanalyzed = 0x02,
    CD         = 0x05,
    Streaming  = 0x06,
};

enum class PlayState1 : uint8_t {
    NoTrack       = 0x00,
    Loading       = 0x02,
    Playing       = 0x03,
    Looping       = 0x04,
    Paused        = 0x05,
    Cued          = 0x06,
    CuePlaying    = 0x07,
    CueScratching = 0x08,
    Searching     = 0x09,
    Ended         = 0x11,
    Unknown       = 0xff,
};

enum class PlayState2 : uint8_t {
    Moving  = 0x7a,
    Stopped = 0x7e,
    Unknown = 0xff,
};

enum class PlayState3 : uint8_t {
    NoTrack       = 0x00,
    PausedReverse = 0x01,
    ForwardVinyl  = 0x09,
    ForwardCDJ    = 0x0d,
    Unknown       = 0xff,
};

} // namespace prodjlink
