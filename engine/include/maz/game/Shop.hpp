#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "maz/game/Inventory.hpp"

// maz::game shop / merchant economy — the vendor counter behind "buy" and "sell." A Shop is a catalogue of
// items, each with a gold price and an optional stock count, that trades against a player's gold wallet and
// maz::game::Inventory. `buy` charges gold, hands over the item, and draws down stock; `sell` takes the
// item back and pays out a fraction of its price (the sell margin, the classic "shops pay less than they
// charge"). Every trade is atomic and fully guarded — it checks gold, stock, inventory room, and ownership
// up front and returns a typed TradeResult, leaving the wallet and bag untouched on any failure. The
// player's gold lives outside the shop (passed by reference), so one wallet can visit many shops. Built on
// the shared Inventory item model. Godot ships no shop/economy system — games hand-roll it every time — so
// this is a beyond-Godot gameplay utility extending the RPG suite. Header-only, std-only, deterministic.
namespace maz::game {

struct ShopItem {
    int id = -1;         // item id (matches Inventory ItemStack ids)
    long long price = 0; // gold cost to BUY one unit
    int stock = -1;      // units in stock; -1 means unlimited
};

enum class TradeResult {
    Ok,
    NotForSale,       // item not in this shop's catalogue
    OutOfStock,       // shop lacks the requested quantity
    NotEnoughGold,    // player can't afford it
    NoInventoryRoom,  // player's inventory has no room for the goods
    NotEnoughItems,   // player doesn't own enough to sell
};

class Shop {
public:
    Shop() = default;
    // `sellMargin` is the fraction of an item's buy price the shop pays when buying FROM the player
    // (0.5 == half). Clamped to [0, 1].
    explicit Shop(double sellMargin) : m_sellMargin(clamp01(sellMargin)) {}

    void addItem(int id, long long price, int stock = -1) {
        ShopItem* it = find(id);
        if (it != nullptr) {
            it->price = price;
            it->stock = stock;
            return;
        }
        m_items.push_back(ShopItem{id, price, stock});
    }

    double sellMargin() const { return m_sellMargin; }
    void setSellMargin(double m) { m_sellMargin = clamp01(m); }

    std::size_t itemCount() const { return m_items.size(); }
    const std::vector<ShopItem>& items() const { return m_items; }
    bool sells(int id) const { return find(id) != nullptr; }

    // Gold to buy one unit, or -1 if not for sale.
    long long buyPrice(int id) const {
        const ShopItem* it = find(id);
        return it == nullptr ? -1 : it->price;
    }
    // Gold the shop pays for one unit (buyPrice * sellMargin, rounded), or -1 if not for sale.
    long long sellPrice(int id) const {
        const ShopItem* it = find(id);
        if (it == nullptr) return -1;
        return static_cast<long long>(std::llround(static_cast<double>(it->price) * m_sellMargin));
    }

    // Units in stock (-1 == unlimited), or 0 if not for sale.
    int stockOf(int id) const {
        const ShopItem* it = find(id);
        return it == nullptr ? 0 : it->stock;
    }
    // Add to a (finite) stock count. No effect on unlimited stock or an unknown item.
    void restock(int id, int amount) {
        ShopItem* it = find(id);
        if (it != nullptr && it->stock >= 0 && amount > 0) it->stock += amount;
    }

    // Player buys `qty` of `itemId`: pays gold, receives the goods, stock drops. All-or-nothing.
    TradeResult buy(int itemId, int qty, long long& playerGold, Inventory& inv) {
        if (qty <= 0) return TradeResult::Ok;
        ShopItem* it = find(itemId);
        if (it == nullptr) return TradeResult::NotForSale;
        if (it->stock >= 0 && it->stock < qty) return TradeResult::OutOfStock;
        const long long total = it->price * qty;
        if (playerGold < total) return TradeResult::NotEnoughGold;
        if (inv.freeSpaceFor(itemId) < qty) return TradeResult::NoInventoryRoom;
        playerGold -= total;
        inv.addItem(itemId, qty); // guaranteed to fit (checked above)
        if (it->stock >= 0) it->stock -= qty;
        return TradeResult::Ok;
    }

    // Player sells `qty` of `itemId`: gives up the goods, receives gold. The shop must price the item
    // (be in its catalogue). Finite stock grows by what was sold. All-or-nothing.
    TradeResult sell(int itemId, int qty, long long& playerGold, Inventory& inv) {
        if (qty <= 0) return TradeResult::Ok;
        ShopItem* it = find(itemId);
        if (it == nullptr) return TradeResult::NotForSale;
        if (inv.count(itemId) < qty) return TradeResult::NotEnoughItems;
        const long long unit = static_cast<long long>(std::llround(static_cast<double>(it->price) * m_sellMargin));
        inv.removeItem(itemId, qty);
        playerGold += unit * qty;
        if (it->stock >= 0) it->stock += qty;
        return TradeResult::Ok;
    }

    void clear() { m_items.clear(); }

private:
    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

    ShopItem* find(int id) {
        for (ShopItem& it : m_items)
            if (it.id == id) return &it;
        return nullptr;
    }
    const ShopItem* find(int id) const {
        for (const ShopItem& it : m_items)
            if (it.id == id) return &it;
        return nullptr;
    }

    std::vector<ShopItem> m_items;
    double m_sellMargin = 0.5;
};

} // namespace maz::game
