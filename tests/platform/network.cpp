// tests/platform/network.cpp — verifies platform::Network policy: classifying a NetworkReachability as
// online / metered / safe-for-large-downloads (isOnline / isMetered / isUnmeteredOnline), the name strings,
// and PlatformBackend::reachability()'s Unknown default on desktop. Pure deterministic logic; the live
// connection type comes from the mobile backend.
#include "maz/platform/Network.hpp"

#include "maz/platform/DesktopBackend.hpp"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

int main() {
    // --- 1. isOnline: only a confirmed connection counts; Unknown + Offline fail safe. ---
    {
        CHECK(!isOnline(NetworkReachability::Unknown), "Unknown is not online (fail safe)");
        CHECK(!isOnline(NetworkReachability::Offline), "Offline is not online");
        CHECK(isOnline(NetworkReachability::Cellular), "cellular is online");
        CHECK(isOnline(NetworkReachability::Wifi), "wifi is online");
        CHECK(isOnline(NetworkReachability::Ethernet), "ethernet is online");
    }

    // --- 2. isMetered: only cellular is metered. ---
    {
        CHECK(isMetered(NetworkReachability::Cellular), "cellular is metered");
        CHECK(!isMetered(NetworkReachability::Wifi), "wifi is not metered");
        CHECK(!isMetered(NetworkReachability::Ethernet), "ethernet is not metered");
        CHECK(!isMetered(NetworkReachability::Unknown) && !isMetered(NetworkReachability::Offline),
              "unknown/offline are not metered (nothing to meter)");
    }

    // --- 3. isUnmeteredOnline: online AND not metered (the "safe to download now" gate). ---
    {
        CHECK(isUnmeteredOnline(NetworkReachability::Wifi), "wifi is safe for large downloads");
        CHECK(isUnmeteredOnline(NetworkReachability::Ethernet), "ethernet is safe for large downloads");
        CHECK(!isUnmeteredOnline(NetworkReachability::Cellular), "cellular is online but metered -> not safe");
        CHECK(!isUnmeteredOnline(NetworkReachability::Offline), "offline is not safe");
        CHECK(!isUnmeteredOnline(NetworkReachability::Unknown), "unknown is not safe (fail safe)");
    }

    // --- 4. Name strings. ---
    {
        CHECK(std::strcmp(reachabilityName(NetworkReachability::Unknown), "unknown") == 0, "unknown name");
        CHECK(std::strcmp(reachabilityName(NetworkReachability::Offline), "offline") == 0, "offline name");
        CHECK(std::strcmp(reachabilityName(NetworkReachability::Cellular), "cellular") == 0, "cellular name");
        CHECK(std::strcmp(reachabilityName(NetworkReachability::Wifi), "wifi") == 0, "wifi name");
        CHECK(std::strcmp(reachabilityName(NetworkReachability::Ethernet), "ethernet") == 0, "ethernet name");
    }

    // --- 5. Backend seam: DesktopBackend reports Unknown by default (not queried here). ---
    {
        DesktopBackend db("MazEngine", "NetTest");
        CHECK(db.reachability() == NetworkReachability::Unknown, "desktop reachability is Unknown");
        CHECK(!isUnmeteredOnline(db.reachability()), "an unqueried desktop gate fails safe");
    }

    if (g_fail == 0) {
        std::printf("network: OK — online/metered/unmetered classification, names, backend seam.\n");
        return 0;
    }
    std::printf("network: %d failure(s).\n", g_fail);
    return 1;
}
