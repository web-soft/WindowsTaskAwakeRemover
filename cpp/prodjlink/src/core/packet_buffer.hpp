#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include "endian.hpp"
#include "../../include/prodjlink/types.hpp"

namespace prodjlink::detail {

class PacketBuffer {
public:
    PacketBuffer(const uint8_t* data, size_t len) noexcept
        : data_(data), len_(len) {}

    bool isValidHeader() const noexcept {
        if (len_ < PACKET_MIN_SIZE) return false;
        return std::memcmp(data_, PACKET_MAGIC, 10) == 0;
    }

    PacketType type() const noexcept {
        if (len_ < 11) return PacketType::Unknown;
        return static_cast<PacketType>(data_[0x0a]);
    }

    uint8_t u8(size_t offset) const noexcept {
        if (offset >= len_) return 0;
        return data_[offset];
    }

    uint16_t u16be(size_t offset) const noexcept {
        if (offset + 2 > len_) return 0;
        return detail::readU16BE(data_ + offset);
    }

    uint32_t u32be(size_t offset) const noexcept {
        if (offset + 4 > len_) return 0;
        return detail::readU32BE(data_ + offset);
    }

    uint32_t u24be(size_t offset) const noexcept {
        if (offset + 3 > len_) return 0;
        return detail::readU24BE(data_ + offset);
    }

    std::string str(size_t offset, size_t maxLen) const {
        if (offset >= len_) return {};
        size_t available = std::min(maxLen, len_ - offset);
        const char* p = reinterpret_cast<const char*>(data_ + offset);
        size_t slen = strnlen(p, available);
        return std::string(p, slen);
    }

    size_t size() const noexcept { return len_; }
    const uint8_t* data() const noexcept { return data_; }

private:
    const uint8_t* data_;
    size_t len_;
};

} // namespace prodjlink::detail
