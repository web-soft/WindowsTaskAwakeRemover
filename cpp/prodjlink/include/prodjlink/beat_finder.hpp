#pragma once
#include <functional>
#include <memory>
#include "beat.hpp"

namespace prodjlink {

class BeatFinder {
public:
    using BeatCallback = std::function<void(const Beat&)>;

    BeatFinder();
    ~BeatFinder();
    BeatFinder(const BeatFinder&) = delete;
    BeatFinder& operator=(const BeatFinder&) = delete;

    bool start();
    void stop();
    bool isRunning() const noexcept;

    // Callback executes on the receive thread — must return quickly (< 1ms)
    void onBeat(BeatCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace prodjlink
