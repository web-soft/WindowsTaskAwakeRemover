#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace prodjlink {

// Track metadata retrieved from a Pioneer player's Rekordbox database.
// Phase 3 — not yet implemented.
struct TrackMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string comment;
    std::string key;
    double      bpm         = 0.0;
    int         year        = 0;
    uint32_t    rekordboxId = 0;
};

class MetadataClient {
public:
    MetadataClient();
    ~MetadataClient();
    std::optional<TrackMetadata> fetchTrackMetadata(uint8_t playerNumber,
                                                    uint32_t rekordboxId);
};

} // namespace prodjlink
