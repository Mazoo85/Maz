// Maz Engine — "ECONOMY" (game::Inventory, game::LootTable, game::Crafting, game::Shop — the loop
// between killing something and carrying the result, toward Godot's lack of one)
// Four modules that are only useful in sequence, shown as one run through that sequence. LEFT: a slotted
// inventory filling up — stacks merging to the cap, a partial add reporting what would not fit, and a
// removal spanning two slots. MIDDLE: a weighted loot table rolled 200 times from a seeded core::Pcg32,
// with the observed frequencies printed next to the weights they were drawn from, so you can see the
// distribution land where it was asked to. RIGHT: a recipe turning three drops into one tool, then a shop
// buying and selling against the same purse and the same inventory, including the two refusals worth
// having — out of stock, and not enough gold. One seed, no input, so the numbers hold still for a golden.
// --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of the four are in maz/Engine.hpp: that umbrella carries 153 of the engine's 692 headers.
#include "maz/core/Pcg32.hpp"
#include "maz/game/Crafting.hpp"
#include "maz/game/Inventory.hpp"
#include "maz/game/LootTable.hpp"
#include "maz/game/Shop.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace maz;

namespace {

// Item ids. Plain ints in the engine's API; named here so the demo reads.
constexpr int kHide = 1;
constexpr int kBone = 2;
constexpr int kFang = 3;
constexpr int kBlade = 4;
constexpr int kPotion = 5;

const char* itemName(int id) {
    switch (id) {
        case kHide: return "hide";
        case kBone: return "bone";
        case kFang: return "fang";
        case kBlade: return "blade";
        case kPotion: return "potion";
        default: return "-";
    }
}

std::string num(double v, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

const char* tradeName(game::TradeResult r) {
    switch (r) {
        case game::TradeResult::Ok: return "Ok";
        case game::TradeResult::NotForSale: return "NotForSale";
        case game::TradeResult::OutOfStock: return "OutOfStock";
        case game::TradeResult::NotEnoughGold: return "NotEnoughGold";
        case game::TradeResult::NoInventoryRoom: return "NoInventoryRoom";
        case game::TradeResult::NotEnoughItems: return "NotEnoughItems";
    }
    return "?";
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ECONOMY starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Economy";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // ---- 1. A slotted inventory, and what it refuses --------------------------------------------------
    // Four slots, ten to a stack. Small on purpose: the interesting behaviour is at the edges.
    game::Inventory bag(4, 10);
    const int leftoverFirst = bag.addItem(kHide, 14);   // fills one stack, starts a second
    const int leftoverFull = bag.addItem(kBone, 35);    // more than the three free slots can hold
    const int removed = bag.removeItem(kHide, 12);      // spans both hide stacks
    const int hideLeft = bag.count(kHide);
    const int boneLeft = bag.count(kBone);

    std::vector<std::string> slotLines;
    for (std::size_t i = 0; i < bag.slotCount(); ++i) {
        const game::ItemStack& s = bag.slot(i);
        slotLines.push_back("slot " + std::to_string(i) + "   " +
                            (s.empty() ? std::string("(empty)")
                                       : std::string(itemName(s.id)) + " x" + std::to_string(s.count)));
    }

    // ---- 2. A weighted loot table, rolled enough times to see the shape --------------------------------
    game::LootTable table;
    table.addEntry(kHide, 50.0, 1, 3);
    table.addEntry(kBone, 30.0, 1, 2);
    table.addEntry(kFang, 15.0, 1, 1);
    table.addEntry(-1, 5.0);            // a "no drop" outcome, which a real table needs
    const double weightTotal = table.totalWeight();

    core::Pcg32 rng(1234u, 5678u);      // fixed seed: the same 200 rolls every run
    std::map<int, int> rolled;
    int empties = 0;
    const int rolls = 200;
    for (const game::LootDrop& d : table.rollMany(rng, rolls)) {
        if (d.empty()) {
            empties++;
        } else {
            rolled[d.id] += d.count;
        }
    }

    std::vector<std::string> lootLines;
    for (const game::LootEntry& e : table.entries()) {
        const double asked = 100.0 * e.weight / weightTotal;
        if (e.id < 0) {
            const double got = 100.0 * static_cast<double>(empties) / static_cast<double>(rolls);
            lootLines.push_back("no drop   weight " + num(asked, 1) + "%   rolled " + num(got, 1) + "%");
        } else {
            // Counting drops rather than rolls, since an entry can drop 1-3 at a time.
            lootLines.push_back(std::string(itemName(e.id)) + "      weight " + num(asked, 1) +
                                "%   got " + std::to_string(rolled[e.id]) + " items");
        }
    }

    // ---- 3. Crafting: three drops become one tool ------------------------------------------------------
    game::Inventory workbench(6, 20);
    workbench.addItem(kHide, 4);
    workbench.addItem(kBone, 4);
    workbench.addItem(kFang, 1);

    game::RecipeBook book;
    book.addRecipe(kBlade, 1, {game::ItemStack{kBone, 2}, game::ItemStack{kFang, 1}});
    book.addRecipe(kPotion, 2, {game::ItemStack{kHide, 3}});

    const std::vector<int> craftableBefore = book.craftable(workbench);
    const bool madeBlade = game::craft(book.recipe(0), workbench);
    const bool bladeAgain = game::craft(book.recipe(0), workbench);   // the fang is spent
    const std::vector<int> craftableAfter = book.craftable(workbench);

    // ---- 4. A shop, trading against that same purse and bag --------------------------------------------
    game::Shop shop(0.40);              // it buys back at 40% of what it sells for
    shop.addItem(kPotion, 25, 3);       // three in stock
    shop.addItem(kBlade, 200, -1);      // unlimited

    long long gold = 120;
    const long long potionBuy = shop.buyPrice(kPotion);
    const long long potionSell = shop.sellPrice(kPotion);

    const game::TradeResult buyTwo = shop.buy(kPotion, 2, gold, workbench);
    const long long goldAfterBuy = gold;
    const game::TradeResult buyFour = shop.buy(kPotion, 4, gold, workbench);   // only 1 left
    const game::TradeResult buyBlade = shop.buy(kBlade, 1, gold, workbench);   // 200 > purse
    const game::TradeResult sellBlade = shop.sell(kBlade, 1, gold, workbench);
    const long long goldAfterSell = gold;
    const int potionsHeld = workbench.count(kPotion);

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kNo{1.0f, 0.52f, 0.46f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.30f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ECONOMY", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "Inventory + LootTable + Crafting + Shop, one run through the loop", kDim,
                          0.34f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 230.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1: the bag ----
            float y = 106.0f;
            font.drawText(*renderer, 24.0f, y, "game::Inventory  -  4 SLOTS OF 10", kHead, 0.36f);
            y += 36.0f;
            for (const std::string& line : slotLines) {
                font.drawText(*renderer, 24.0f, y, line.c_str(), kText, sz);
                y += 26.0f;
            }
            y += 10.0f;
            row(24.0f, y, "add 14 hide, left over", std::to_string(leftoverFirst), kVal); y += 26.0f;
            row(24.0f, y, "add 35 bone, left over", std::to_string(leftoverFull), kNo); y += 26.0f;
            row(24.0f, y, "remove 12 hide, removed", std::to_string(removed), kVal); y += 26.0f;
            row(24.0f, y, "hide / bone held", std::to_string(hideLeft) + " / " +
                              std::to_string(boneLeft), kVal); y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          "add returns what would NOT fit — a full bag is a refusal, not a loss",
                          kDim, 0.26f);

            // ---- column 2: the loot table ----
            y = 106.0f;
            font.drawText(*renderer, 470.0f, y, "game::LootTable  -  200 ROLLS", kHead, 0.36f);
            y += 36.0f;
            for (const std::string& line : lootLines) {
                font.drawText(*renderer, 470.0f, y, line.c_str(), kText, sz);
                y += 26.0f;
            }
            y += 10.0f;
            row(470.0f, y, "total weight", num(weightTotal, 0), kVal); y += 26.0f;
            row(470.0f, y, "empty rolls", std::to_string(empties), kVal); y += 30.0f;
            font.drawText(*renderer, 470.0f, y,
                          "one seeded core::Pcg32, so these 200 rolls are the same every run", kDim,
                          0.26f);

            y += 46.0f;
            font.drawText(*renderer, 470.0f, y, "game::Crafting", kHead, 0.36f);
            y += 34.0f;
            row(470.0f, y, "craftable at the start", std::to_string(craftableBefore.size()), kVal);
            y += 26.0f;
            row(470.0f, y, "craft a blade", madeBlade ? "yes" : "no", madeBlade ? kOk : kNo);
            y += 26.0f;
            row(470.0f, y, "craft another", bladeAgain ? "yes" : "no  (fang spent)",
                bladeAgain ? kOk : kNo);
            y += 26.0f;
            row(470.0f, y, "craftable now", std::to_string(craftableAfter.size()), kVal);

            // ---- column 3: the shop ----
            y = 106.0f;
            font.drawText(*renderer, 930.0f, y, "game::Shop  -  120 GOLD", kHead, 0.36f);
            y += 36.0f;
            row(930.0f, y, "potion buy / sell", std::to_string(potionBuy) + " / " +
                               std::to_string(potionSell), kVal); y += 26.0f;
            font.drawText(*renderer, 930.0f, y, "a 40% margin: it buys back for less than it sells",
                          kDim, 0.26f);
            y += 34.0f;
            row(930.0f, y, "buy 2 potions", tradeName(buyTwo), buyTwo == game::TradeResult::Ok ? kOk : kNo);
            y += 26.0f;
            row(930.0f, y, "gold left", std::to_string(goldAfterBuy), kVal); y += 26.0f;
            row(930.0f, y, "buy 4 more", tradeName(buyFour), kNo); y += 26.0f;
            row(930.0f, y, "buy a blade", tradeName(buyBlade), kNo); y += 26.0f;
            row(930.0f, y, "sell the blade", tradeName(sellBlade),
                sellBlade == game::TradeResult::Ok ? kOk : kNo); y += 26.0f;
            row(930.0f, y, "gold after selling", std::to_string(goldAfterSell), kVal); y += 26.0f;
            row(930.0f, y, "potions held", std::to_string(potionsHeld), kVal); y += 30.0f;
            font.drawText(*renderer, 930.0f, y,
                          "every refusal is a named reason, not a false — the UI can say why", kDim,
                          0.26f);

            font.drawText(*renderer, 24.0f, 664.0f,
                          "Kill, loot, craft, trade: four modules that are each only half a feature "
                          "on their own. One seed, no input, so the picture is the test.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.28f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ECONOMY shutting down with %d gold (renderer %s)", static_cast<int>(goldAfterSell),
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
