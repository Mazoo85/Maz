// tests/game/magazine.cpp — verifies the weapon ammo model (game::Magazine): a loaded magazine, a spare
// reserve, and a timed reload that transfers rounds from reserve into the magazine. Ground truths are
// hand-counted round-by-round: firing decrements and stops at empty; you cannot fire mid-reload; a reload
// only starts when the magazine is not full and reserve remains; on completion it loads min(space,reserve);
// a partial reserve tops up only partway; cancel loads nothing; infinite reserve always fills. Deterministic.
#include "maz/game/Magazine.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::Magazine;

int main() {
    // --- 1. Fresh magazine is full and ready. ---
    {
        Magazine m(30, 90, 2.0);
        CHECK(m.count() == 30 && m.capacity() == 30 && m.reserve() == 90, "starts full with reserve");
        CHECK(m.canFire() && !m.isEmpty() && m.isFull(), "loaded and ready");
        CHECK(m.total() == 120, "total = loaded + reserve");
    }

    // --- 2. Firing consumes rounds and stops at empty. ---
    {
        Magazine m(5, 10, 1.0);
        for (int i = 0; i < 5; ++i) CHECK(m.tryFire(), "fires while loaded");
        CHECK(m.count() == 0 && m.isEmpty(), "empty after 5 shots");
        CHECK(!m.tryFire(), "empty click: cannot fire at 0");
        CHECK(m.count() == 0, "count does not go negative");
    }

    // --- 3. Cannot fire during a reload; reload takes the configured time. ---
    {
        Magazine m(10, 20, 2.0);
        for (int i = 0; i < 4; ++i) m.tryFire(); // 6 left
        CHECK(m.reload(), "reload starts (mag not full, reserve available)");
        CHECK(m.isReloading() && !m.canFire(), "cannot fire mid-reload");
        CHECK(!m.tryFire(), "tryFire blocked while reloading");
        m.tick(1.0);
        CHECK(m.isReloading() && std::fabs(m.reloadProgress() - 0.5) < 1e-9, "reload half done at 1s of 2s");
        m.tick(1.0); // completes: space was 4, reserve 20 -> load 4
        CHECK(!m.isReloading() && m.count() == 10 && m.reserve() == 16, "reload loads min(space,reserve)");
        CHECK(m.canFire(), "can fire again after reload");
    }

    // --- 4. Partial reserve tops up only partway and empties the reserve. ---
    {
        Magazine m(30, 10, 1.0);
        for (int i = 0; i < 30; ++i) m.tryFire(); // empty
        CHECK(m.reload(), "reload starts with partial reserve");
        m.tick(1.0);
        CHECK(m.count() == 10 && m.reserve() == 0, "partial reload: 10 loaded, reserve drained");
        CHECK(!m.reload(), "no reload with empty reserve");
    }

    // --- 5. Reload is refused when the magazine is already full. ---
    {
        Magazine m(10, 10, 1.0);
        CHECK(!m.reload(), "full magazine refuses reload");
        CHECK(m.reserve() == 10, "reserve untouched by refused reload");
    }

    // --- 6. cancelReload aborts without loading ammo. ---
    {
        Magazine m(10, 20, 2.0);
        for (int i = 0; i < 5; ++i) m.tryFire(); // 5 left
        m.reload();
        m.tick(1.0);
        m.cancelReload();
        CHECK(!m.isReloading() && m.count() == 5 && m.reserve() == 20, "cancel loads nothing");
        CHECK(m.reloadProgress() == 0.0, "progress resets after cancel");
    }

    // --- 7. A big dt (or zero reload time) completes the reload at once. ---
    {
        Magazine m(10, 20, 2.0);
        for (int i = 0; i < 10; ++i) m.tryFire();
        m.reload();
        m.tick(100.0);
        CHECK(!m.isReloading() && m.count() == 10 && m.reserve() == 10, "huge dt completes reload");

        Magazine instant(10, 20, 0.0);
        for (int i = 0; i < 10; ++i) instant.tryFire();
        CHECK(instant.reload() && !instant.isReloading() && instant.count() == 10,
              "zero reload time reloads instantly");
    }

    // --- 8. Infinite reserve always fills to full and never depletes. ---
    {
        Magazine m(10, 0, 1.0);
        m.setInfiniteReserve(true);
        for (int i = 0; i < 10; ++i) m.tryFire();
        CHECK(m.reload(), "infinite reserve allows reload even at 0 spare");
        m.tick(1.0);
        CHECK(m.count() == 10 && m.hasInfiniteReserve(), "infinite reload tops to full");
        CHECK(m.total() == Magazine::kInfiniteTotal, "infinite total sentinel");
    }

    // --- 9. Reserve pickup and fraction. ---
    {
        Magazine m(4, 0, 1.0);
        CHECK(std::fabs(m.fraction() - 1.0f) < 1e-6f, "full fraction is 1");
        m.tryFire(); m.tryFire();
        CHECK(std::fabs(m.fraction() - 0.5f) < 1e-6f, "half fraction after 2 of 4");
        m.addReserve(8);
        CHECK(m.reserve() == 8, "addReserve adds spare ammo");
    }

    // --- 10. tick with no reload / non-positive dt is a no-op. ---
    {
        Magazine m(10, 10, 2.0);
        m.tick(5.0); // not reloading -> nothing happens
        CHECK(m.count() == 10 && !m.isReloading(), "tick without reload is a no-op");
        m.tryFire();
        m.reload();
        m.tick(0.0); m.tick(-1.0);
        CHECK(m.isReloading() && m.reloadProgress() == 0.0, "non-positive dt does not advance reload");
    }

    if (g_fail == 0) std::printf("magazine: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
