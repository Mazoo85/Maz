#pragma once

#include "maz/platform/PlatformBackend.hpp" // platform::NetworkReachability

// maz::platform network-reachability policy — the pure decisions a game makes from a NetworkReachability
// snapshot. PlatformBackend::reachability() reports HOW the device is connected; these free functions turn
// that into WHAT TO DO: are we online at all, and is the link metered (so we should hold big downloads or
// warn before spending the player's cellular data). Kept out of the backend so the policy is deterministic
// and tunable, and a no-op-safe default where reachability is Unknown. Header-only.
namespace maz::platform {

// True only when a usable connection is confirmed. Unknown (not yet queried) and Offline both return false,
// so gating a network request on isOnline() fails safe.
inline bool isOnline(NetworkReachability r) {
    return r == NetworkReachability::Cellular || r == NetworkReachability::Wifi ||
           r == NetworkReachability::Ethernet;
}

// True when the link is (likely) metered / pay-per-byte — currently cellular. Wi-Fi and Ethernet are treated
// as unmetered; Unknown/Offline are not metered (there is nothing to meter). Use it to defer large downloads
// or prompt the player before spending their data plan.
inline bool isMetered(NetworkReachability r) {
    return r == NetworkReachability::Cellular;
}

// True when a large transfer is safe to start unprompted: online AND not metered.
inline bool isUnmeteredOnline(NetworkReachability r) {
    return isOnline(r) && !isMetered(r);
}

inline const char* reachabilityName(NetworkReachability r) {
    switch (r) {
        case NetworkReachability::Unknown: return "unknown";
        case NetworkReachability::Offline: return "offline";
        case NetworkReachability::Cellular: return "cellular";
        case NetworkReachability::Wifi: return "wifi";
        case NetworkReachability::Ethernet: return "ethernet";
    }
    return "unknown";
}

} // namespace maz::platform
