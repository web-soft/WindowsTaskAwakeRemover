#if defined(__linux__)

#include "network_interface.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <optional>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace prodjlink::platform {

std::vector<NetworkInterface> enumerateInterfaces() {
    std::vector<NetworkInterface> result;
    ifaddrs* ifas = nullptr;
    if (getifaddrs(&ifas) != 0) return result;

    for (ifaddrs* ifa = ifas; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;

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

        int tmpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (tmpSock >= 0) {
            ifreq ifr{};
            std::strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
            if (::ioctl(tmpSock, SIOCGIFHWADDR, &ifr) == 0)
                std::memcpy(ni.macAddress.data(), ifr.ifr_hwaddr.sa_data, 6);
            ::close(tmpSock);
        }
        result.push_back(ni);
    }
    freeifaddrs(ifas);
    return result;
}

std::optional<NetworkInterface> pickInterface() {
    for (auto& ni : enumerateInterfaces())
        if (!ni.isLoopback) return ni;
    return std::nullopt;
}

} // namespace prodjlink::platform

#endif // __linux__
