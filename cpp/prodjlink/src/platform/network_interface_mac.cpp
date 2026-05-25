#if defined(__APPLE__)

#include "network_interface.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <map>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <optional>

namespace prodjlink::platform {

std::vector<NetworkInterface> enumerateInterfaces() {
    std::vector<NetworkInterface> result;
    ifaddrs* ifas = nullptr;
    if (getifaddrs(&ifas) != 0) return result;

    std::map<std::string, NetworkInterface> byName;
    for (ifaddrs* ifa = ifas; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET && (ifa->ifa_flags & IFF_UP)) {
            NetworkInterface ni;
            ni.name       = ifa->ifa_name;
            ni.isLoopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;
            auto* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            uint32_t ip = ntohl(sin->sin_addr.s_addr);
            ni.ipAddress = {uint8_t(ip >> 24), uint8_t(ip >> 16), uint8_t(ip >> 8), uint8_t(ip)};
            if (ifa->ifa_netmask) {
                auto* mask = reinterpret_cast<sockaddr_in*>(ifa->ifa_netmask);
                uint32_t m = ntohl(mask->sin_addr.s_addr);
                ni.subnetMask = {uint8_t(m >> 24), uint8_t(m >> 16), uint8_t(m >> 8), uint8_t(m)};
                uint32_t b = ip | ~m;
                ni.broadcastAddress = {uint8_t(b >> 24), uint8_t(b >> 16), uint8_t(b >> 8), uint8_t(b)};
            }
            byName[ni.name] = ni;
        } else if (ifa->ifa_addr->sa_family == AF_LINK) {
            auto it = byName.find(ifa->ifa_name);
            if (it != byName.end()) {
                auto* sdl = reinterpret_cast<sockaddr_dl*>(ifa->ifa_addr);
                if (sdl->sdl_alen == 6)
                    std::memcpy(it->second.macAddress.data(), LLADDR(sdl), 6);
            }
        }
    }
    freeifaddrs(ifas);
    for (auto& [name, ni] : byName) result.push_back(ni);
    return result;
}

std::optional<NetworkInterface> pickInterface() {
    for (auto& ni : enumerateInterfaces())
        if (!ni.isLoopback) return ni;
    return std::nullopt;
}

} // namespace prodjlink::platform

#endif // __APPLE__
