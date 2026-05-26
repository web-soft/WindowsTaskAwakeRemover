#pragma once
#include <cstdint>

// Packed structs mapping Pro DJ Link UDP packet bytes directly.
// All multi-byte integers are big-endian on the wire.
// Use endian.hpp helpers (readU16BE/readU32BE) when reading numeric fields.

#pragma pack(push, 1)

namespace prodjlink::packets {

// 10-byte magic signature + type present in every packet
struct Magic {
    uint8_t sig[10];  // 51 73 70 74 31 57 6d 4a 4f 4c
    uint8_t type;
};

// Common header shared by all Pioneer Pro DJ Link UDP packets (0x00–0x24)
struct Header {
    uint8_t  sig[10];        // 0x00 magic
    uint8_t  type;           // 0x0a packet type
    uint8_t  _pad0;          // 0x0b (0x00)
    char     deviceName[20]; // 0x0c null-padded
    uint8_t  _c1;            // 0x20 (0x01)
    uint8_t  subtype;        // 0x21
    uint8_t  lengthHi;       // 0x22 packet length big-endian
    uint8_t  lengthLo;       // 0x23
    uint8_t  playerNumber;   // 0x24
};
static_assert(sizeof(Header) == 0x25, "Header size mismatch");

// ─── Port 50000 — Device announcement ─────────────────────────────────────

// 0x06 — DeviceKeepAlive (0x36 bytes)
struct KeepAlive {
    Header   hdr;            // 0x00–0x24
    uint8_t  deviceType;     // 0x25 (0x01=CDJ, 0x02=Mixer)
    uint8_t  _pad[6];        // 0x26–0x2b
    uint8_t  mac[6];         // 0x2c MAC address
    uint8_t  ip[4];          // 0x32 IP address (big-endian octets)
};
static_assert(sizeof(KeepAlive) == 0x36, "KeepAlive size mismatch");

// 0x0a — DeviceHello (0x2c bytes)
struct DeviceHello {
    Header   hdr;
    uint8_t  ip[4];          // 0x25 (alternate position in hello)
    uint8_t  mac[6];         // 0x29
    uint8_t  _pad[3];
};

// 0x00 — ClaimStage1 (0x2c bytes)
struct ClaimStage1 {
    Header  hdr;
    uint8_t counter;         // 0x25 (1–3)
    uint8_t _pad;
    uint8_t mac[6];          // 0x27
    uint8_t _end[3];
};

// ─── Port 50001 — Beat sync ────────────────────────────────────────────────

// 0x28 — Beat (0x60 bytes)
struct BeatPacket {
    Header   hdr;            // 0x00–0x24
    uint8_t  _pad[3];        // 0x25–0x27
    uint8_t  nextBeat[4];    // 0x28 next beat time ms (uint32_be)
    uint8_t  beat2[4];       // 0x2c
    uint8_t  nextBar[4];     // 0x30 (5th beat = bar start)
    uint8_t  beat4[4];       // 0x34
    uint8_t  bar2[4];        // 0x38 (9th beat)
    uint8_t  beat8[4];       // 0x3c
    uint8_t  _pad2[20];      // 0x40–0x53
    uint8_t  pitch[3];       // 0x54 raw pitch (uint24_be, 0x100000 = neutral)
    uint8_t  _pad3;          // 0x57
    uint8_t  bpm100Hi;       // 0x58 BPM×100 high byte
    uint8_t  bpm100Lo;       // 0x59 BPM×100 low byte
    uint8_t  _pad4[2];       // 0x5a–0x5b
    uint8_t  beatInBar;      // 0x5c (1–4)
    uint8_t  _end[3];        // 0x5d–0x5f
};
static_assert(sizeof(BeatPacket) == 0x60, "BeatPacket size mismatch");

// ─── Port 50002 — Device status ───────────────────────────────────────────

// 0x0a — CdjStatus (0xd4 bytes for Nexus, larger for CDJ-3000)
struct CdjStatusPacket {
    Header   hdr;            // 0x00–0x24
    uint8_t  _pad1[2];       // 0x25–0x26
    uint8_t  activity;       // 0x27 (0=idle, non-zero=active)
    uint8_t  sourcePlayer;   // 0x28
    uint8_t  sourceSlot;     // 0x29 MediaSlot
    uint8_t  trackType;      // 0x2a TrackType
    uint8_t  _pad2;          // 0x2b
    uint8_t  rekordboxId[4]; // 0x2c (uint32_be)
    uint8_t  trackNumber[2]; // 0x30 (uint16_be)
    uint8_t  _pad3[18];      // 0x32–0x43
    uint8_t  playState1;     // 0x44
    uint8_t  playState2;     // 0x45
    uint8_t  playState3;     // 0x46
    uint8_t  pitch1[4];      // 0x47 raw pitch (uint32_be, 0x100000=neutral)
    uint8_t  pitch2[4];      // 0x4b
    uint8_t  pitch3[4];      // 0x4f
    uint8_t  pitch4[4];      // 0x53–0x56
    uint8_t  bpm100Hi;       // 0x57 BPM×100 high byte
    uint8_t  bpm100Lo;       // 0x58 BPM×100 low byte
    uint8_t  beatNum[4];     // 0x59–0x5c beat number in track (uint32_be)
    uint8_t  cue[2];         // 0x5d–0x5e cue countdown (uint16_be, 0x01ff=none)
    uint8_t  _pad5[9];       // 0x5f–0x67
    uint8_t  flags;          // 0x68 bit6=playing bit5=master bit4=sync bit3=onair
    uint8_t  _pad6;          // 0x69
    uint8_t  beatInBar;      // 0x6a (1–4)
    uint8_t  _pad7[25];      // 0x6b–0x83
    uint8_t  masterTarget;   // 0x84 master handoff target player
};

// 0x29 — MixerStatus (minimum 0x40 bytes)
struct MixerStatusPacket {
    Header   hdr;
    uint8_t  _pad[20];       // 0x25–0x38
    uint8_t  bpm100Hi;       // 0x39
    uint8_t  bpm100Lo;       // 0x3a
    uint8_t  beatInBar;      // 0x3b
    uint8_t  onAirFlags;     // 0x3c bit N = channel N+1 on air
    uint8_t  syncFlags;      // 0x3d bit5=master bit4=sync
};

} // namespace prodjlink::packets

#pragma pack(pop)
