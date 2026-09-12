// tests/game/assetdef.cpp — verifies game::AssetDef (kind + typed stats) and its round-trip through a
// prefab-root property bag, including the full editor::Composite + io::PrefabText text path. Headless.
#include "maz/editor/Composite.hpp"
#include "maz/game/AssetDef.hpp"
#include "maz/io/PrefabText.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace maz;

static int g_fail = 0;
#define CHECK(c, m)                                                                                  \
    do {                                                                                             \
        if (!(c)) {                                                                                  \
            std::printf("FAIL: %s\n", (m));                                                          \
            ++g_fail;                                                                                \
        }                                                                                            \
    } while (0)

static bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }

int main() {
    // --- defaults + typed stat accessors ---
    {
        game::AssetDef d;
        CHECK(d.kind == game::AssetKind::Item, "default kind is Item");
        CHECK(near(d.statF("missing", 1.5f), 1.5f), "missing stat returns default");
        d.setStatF("damage", 12.5f);
        d.setStatI("count", 3);
        d.setStatB("stackable", true);
        CHECK(near(d.statF("damage"), 12.5f) && d.statI("count") == 3 && d.statB("stackable"),
              "typed stat set/get");
    }

    // --- kind name mapping ---
    {
        CHECK(std::string(game::assetKindName(game::AssetKind::Character)) == "character" &&
                  std::string(game::assetKindName(game::AssetKind::Item)) == "item",
              "kind -> name");
        CHECK(game::assetKindFromName("character") == game::AssetKind::Character &&
                  game::assetKindFromName("item") == game::AssetKind::Item &&
                  game::assetKindFromName("bogus") == game::AssetKind::Item,
              "name -> kind (unknown defaults to Item)");
    }

    // --- props round-trip (in memory) ---
    {
        game::AssetDef d;
        d.kind = game::AssetKind::Character;
        d.setStatF("hp", 100.0f);
        d.setStatI("level", 7);
        d.setStatB("hostile", false);
        const scene::PropBag bag = game::assetDefToProps(d);
        const game::AssetDef back = game::assetDefFromProps(bag);
        CHECK(back.kind == game::AssetKind::Character, "kind round-trips via props");
        CHECK(near(back.statF("hp"), 100.0f) && back.statI("level") == 7 && !back.statB("hostile"),
              "stats round-trip via props");
        CHECK(scene::findProp(back.stats, game::kKindKey()) == nullptr,
              "reserved kind key is not exposed as a stat");
    }

    // --- full A+B text path: scene + def -> prefab text -> def + scene ---
    {
        editor::Scene s;
        editor::Node torso;
        torso.name = "Torso";
        torso.meshId = 0;
        editor::Node head;
        head.name = "Head";
        head.meshId = 1;
        head.position = math::vec3(0.0f, 1.6f, 0.0f);
        s.nodes = {torso, head};

        game::AssetDef def;
        def.name = "Hero";
        def.kind = game::AssetKind::Character;
        def.setStatF("hp", 80.0f);
        def.setStatI("armor", 2);

        const std::string text =
            io::savePrefabText(editor::sceneToPrefab(s, def.name, game::assetDefToProps(def)));

        scene::Prefab loaded;
        CHECK(io::loadPrefabText(text, loaded), "asset prefab text parses");
        CHECK(loaded.root.name == "Hero", "asset name preserved on prefab root");

        const game::AssetDef backDef = game::assetDefFromProps(loaded.root.props);
        CHECK(backDef.kind == game::AssetKind::Character, "kind survives the text path");
        CHECK(near(backDef.statF("hp"), 80.0f) && backDef.statI("armor") == 2,
              "stats survive the text path");

        editor::Scene backScene;
        editor::prefabToScene(loaded, backScene);
        CHECK(backScene.nodes.size() == 2 && backScene.nodes[0] == torso && backScene.nodes[1] == head,
              "parts survive alongside the definition");
    }

    if (g_fail == 0) {
        std::printf("assetdef: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
