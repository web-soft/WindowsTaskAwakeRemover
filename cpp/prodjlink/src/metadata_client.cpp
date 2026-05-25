#include "../include/prodjlink/metadata_client.hpp"

namespace prodjlink {

MetadataClient::MetadataClient() = default;
MetadataClient::~MetadataClient() = default;

std::optional<TrackMetadata> MetadataClient::fetchTrackMetadata(uint8_t, uint32_t) {
    // Phase 3 not yet implemented — requires TCP connection to Pioneer dbserver
    return std::nullopt;
}

} // namespace prodjlink
