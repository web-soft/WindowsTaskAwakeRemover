#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#include "network_interface.hpp"
#include <cstring>
#include <optional>
#include <vector>

namespace prodjlink::platform {

std::vector<NetworkInterface> enumerateInterfaces() {
    std::vector<NetworkInterface> result;
    ULONG bufLen = 15000;
    std::vector<uint8_t> buf(bufLen);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &bufLen)
            == ERROR_BUFFER_OVERFLOW) {
        buf.resize(bufLen);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    }
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &bufLen)
            != NO_ERROR) return result;

    for (auto* a = adapters; a != nullptr; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        for (auto* ua = a->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;
            NetworkInterface ni;
            ni.name       = a->AdapterName;
            ni.isLoopback = (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK);
            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            uint32_t ip = ntohl(sin->sin_addr.s_addr);
            ni.ipAddress = {uint8_t(ip >> 24), uint8_t(ip >> 16), uint8_t(ip >> 8), uint8_t(ip)};
            uint32_t prefixLen = ua->OnLinkPrefixLength;
            uint32_t mask = prefixLen ? (~0u << (32 - prefixLen)) : 0;
            ni.subnetMask = {uint8_t(mask >> 24), uint8_t(mask >> 16), uint8_t(mask >> 8), uint8_t(mask)};
            uint32_t b = ip | ~mask;
            ni.broadcastAddress = {uint8_t(b >> 24), uint8_t(b >> 16), uint8_t(b >> 8), uint8_t(b)};
            if (a->PhysicalAddressLength == 6)
                std::memcpy(ni.macAddress.data(), a->PhysicalAddress, 6);
            result.push_back(ni);
            break;
        }
    }
    return result;
}

std::optional<NetworkInterface> pickInterface() {
    for (auto& ni : enumerateInterfaces())
        if (!ni.isLoopback) return ni;
    return std::nullopt;
}

} // namespace prodjlink::platform

#endif // _WIN32
