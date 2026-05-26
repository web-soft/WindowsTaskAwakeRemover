#pragma once
#include <cstdint>
#include <mutex>
#include <string>

namespace prodjlink {

// Normalized state — protocol-independent pivot between Pioneer Reader and any Writer (Denon, etc.)
struct DJState {
    bool     isPlaying   = false;
    float    bpm         = 120.0f;
    float    phase       = 0.0f;      // beat position 0.0–1.0
    uint8_t  beatInBar   = 1;         // 1–4
    bool     isMaster    = false;
    bool     isSynced    = false;
    bool     isOnAir     = false;
    uint8_t  playerID    = 0;
    uint32_t rekordboxId = 0;
    uint16_t trackNumber = 0;
    std::string trackName;

    mutable std::mutex mtx;

    DJState() = default;

    // Copy constructor acquires source mutex; the new instance gets its own unlocked mutex.
    DJState(const DJState& o) {
        std::lock_guard<std::mutex> lk(o.mtx);
        isPlaying   = o.isPlaying;
        bpm         = o.bpm;
        phase       = o.phase;
        beatInBar   = o.beatInBar;
        isMaster    = o.isMaster;
        isSynced    = o.isSynced;
        isOnAir     = o.isOnAir;
        playerID    = o.playerID;
        rekordboxId = o.rekordboxId;
        trackNumber = o.trackNumber;
        trackName   = o.trackName;
    }

    DJState& operator=(const DJState& o) {
        if (this == &o) return *this;
        std::lock_guard<std::mutex> lk(o.mtx);
        std::lock_guard<std::mutex> lk2(mtx);
        isPlaying   = o.isPlaying;
        bpm         = o.bpm;
        phase       = o.phase;
        beatInBar   = o.beatInBar;
        isMaster    = o.isMaster;
        isSynced    = o.isSynced;
        isOnAir     = o.isOnAir;
        playerID    = o.playerID;
        rekordboxId = o.rekordboxId;
        trackNumber = o.trackNumber;
        trackName   = o.trackName;
        return *this;
    }

    void update(float newBpm, float newPhase, bool playing, uint8_t beat = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        bpm       = newBpm;
        phase     = newPhase;
        isPlaying = playing;
        if (beat > 0) beatInBar = beat;
    }

    // Full update from a CDJ status packet
    void updateFull(float newBpm, float newPhase, bool playing, uint8_t beat,
                    bool master, bool synced, bool onAir,
                    uint32_t rbId, uint16_t trkNum) {
        std::lock_guard<std::mutex> lk(mtx);
        bpm        = newBpm;
        phase      = newPhase;
        isPlaying  = playing;
        beatInBar  = beat;
        isMaster   = master;
        isSynced   = synced;
        isOnAir    = onAir;
        rekordboxId = rbId;
        trackNumber = trkNum;
    }

    // Returns a locked copy — safe for read-only consumers
    DJState snapshot() const { return DJState(*this); }

    void debugPrint() const;
};

} // namespace prodjlink
