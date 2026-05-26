#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../include/prodjlink/dj_state.hpp"
#include <atomic>
#include <thread>
#include <vector>

using namespace prodjlink;

TEST_CASE("DJState default values", "[djstate]") {
    DJState s;
    REQUIRE(s.isPlaying  == false);
    REQUIRE(s.bpm        == Catch::Approx(120.0f));
    REQUIRE(s.phase      == Catch::Approx(0.0f));
    REQUIRE(s.beatInBar  == 1);
    REQUIRE(s.isMaster   == false);
    REQUIRE(s.playerID   == 0);
    REQUIRE(s.rekordboxId == 0);
}

TEST_CASE("DJState::update sets fields under lock", "[djstate]") {
    DJState s;
    s.update(128.0f, 0.5f, true, 3);
    auto snap = s.snapshot();
    REQUIRE(snap.bpm      == Catch::Approx(128.0f));
    REQUIRE(snap.phase    == Catch::Approx(0.5f));
    REQUIRE(snap.isPlaying);
    REQUIRE(snap.beatInBar == 3);
}

TEST_CASE("DJState::updateFull sets all fields", "[djstate]") {
    DJState s;
    s.updateFull(140.0f, 0.25f, true, 2, true, true, false, 0xDEADBEEF, 42);
    auto snap = s.snapshot();
    REQUIRE(snap.bpm         == Catch::Approx(140.0f));
    REQUIRE(snap.phase       == Catch::Approx(0.25f));
    REQUIRE(snap.isPlaying);
    REQUIRE(snap.beatInBar   == 2);
    REQUIRE(snap.isMaster);
    REQUIRE(snap.isSynced);
    REQUIRE_FALSE(snap.isOnAir);
    REQUIRE(snap.rekordboxId == 0xDEADBEEF);
    REQUIRE(snap.trackNumber == 42);
}

TEST_CASE("DJState::snapshot returns a value copy, not a reference", "[djstate]") {
    DJState s;
    s.update(100.0f, 0.0f, false);
    DJState copy = s.snapshot();
    s.update(200.0f, 0.0f, true);
    // copy should still reflect the old values
    REQUIRE(copy.bpm      == Catch::Approx(100.0f));
    REQUIRE_FALSE(copy.isPlaying);
    REQUIRE(s.snapshot().bpm == Catch::Approx(200.0f));
}

TEST_CASE("DJState concurrent update + snapshot is race-free", "[djstate][thread]") {
    DJState s;
    s.update(120.0f, 0.0f, false);

    std::atomic<bool> go{false};
    std::atomic<int>  errors{0};
    constexpr int WRITERS = 4;
    constexpr int READERS = 4;
    constexpr int ITERS   = 500;

    std::vector<std::thread> threads;
    threads.reserve(WRITERS + READERS);

    for (int w = 0; w < WRITERS; ++w) {
        threads.emplace_back([&, w]() {
            while (!go) {}
            for (int i = 0; i < ITERS; ++i)
                s.update(static_cast<float>(100 + w), static_cast<float>(i) / ITERS,
                         i % 2 == 0, static_cast<uint8_t>((i % 4) + 1));
        });
    }
    for (int r = 0; r < READERS; ++r) {
        threads.emplace_back([&]() {
            while (!go) {}
            for (int i = 0; i < ITERS; ++i) {
                auto snap = s.snapshot();
                // Basic sanity: beatInBar must be 1–4 or default 1
                if (snap.beatInBar < 1 || snap.beatInBar > 4)
                    ++errors;
            }
        });
    }

    go = true;
    for (auto& t : threads) t.join();
    REQUIRE(errors == 0);
}
