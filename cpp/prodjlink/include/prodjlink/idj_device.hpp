#pragma once
#include <cstdint>

namespace prodjlink {

// Abstract interface for any DJ device that can be driven by a bridge.
// Implement this to add a Denon/Traktor/Serato writer without touching the Pioneer reader.
class IDJDevice {
public:
    virtual ~IDJDevice() = default;

    virtual void setBpm(float bpm)        = 0;
    virtual void setPlaying(bool playing) = 0;
    virtual void seek(float position)     = 0;   // 0.0–1.0 within track
    virtual void setMaster(bool isMaster) = 0;
    virtual void setPhase(float phase)    = 0;   // 0.0–1.0 within beat

    virtual uint8_t playerNumber() const  = 0;
    virtual bool    isConnected()  const  = 0;
};

} // namespace prodjlink
