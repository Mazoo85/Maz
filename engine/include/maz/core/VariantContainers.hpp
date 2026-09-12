#pragma once

#include "maz/core/Variant.hpp"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// maz::core Array / Dictionary — Godot's container Variants. Array is an ordered, index-addressed list
// of Variants (append/insert/remove/find/slice/reverse); Dictionary is an ORDERED string->Variant map
// (Godot dictionaries preserve insertion order) with has/get/set/erase/keys/values/merge. These are the
// data-driven backbone Godot uses everywhere: config trees, save games, script args, JSON-shaped data.
// Header-only, deterministic, unit-tested. (Elements are the M293 Variant subset; keys are strings —
// Godot's arbitrary-Variant keys and nested container Variants are out of scope here.)
namespace maz::core {

class Array {
public:
    Array() = default;
    Array(std::initializer_list<Variant> init) : m_items(init) {}

    std::size_t size() const { return m_items.size(); }
    bool isEmpty() const { return m_items.empty(); }
    void clear() { m_items.clear(); }

    void append(const Variant& v) { m_items.push_back(v); }
    void pushBack(const Variant& v) { m_items.push_back(v); }
    void pushFront(const Variant& v) { m_items.insert(m_items.begin(), v); }

    // Insert before index `pos` (clamped to [0, size]).
    void insert(std::size_t pos, const Variant& v) {
        if (pos > m_items.size()) {
            pos = m_items.size();
        }
        m_items.insert(m_items.begin() + static_cast<std::ptrdiff_t>(pos), v);
    }
    // Remove the element at `pos` (no-op if out of range).
    void removeAt(std::size_t pos) {
        if (pos < m_items.size()) {
            m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(pos));
        }
    }
    // Remove and return the last element (Nil if empty).
    Variant popBack() {
        if (m_items.empty()) {
            return Variant();
        }
        Variant v = m_items.back();
        m_items.pop_back();
        return v;
    }

    bool has(const Variant& v) const { return find(v) >= 0; }
    // Index of the first element equal to `v` at or after `from`, or -1.
    std::int64_t find(const Variant& v, std::size_t from = 0) const {
        for (std::size_t i = from; i < m_items.size(); ++i) {
            if (m_items[i] == v) {
                return static_cast<std::int64_t>(i);
            }
        }
        return -1;
    }

    Variant& operator[](std::size_t i) { return m_items[i]; }
    const Variant& operator[](std::size_t i) const { return m_items[i]; }
    const Variant& front() const { return m_items.front(); }
    const Variant& back() const { return m_items.back(); }

    void reverse() {
        for (std::size_t i = 0, j = m_items.size(); i + 1 < j; ++i, --j) {
            std::swap(m_items[i], m_items[j - 1]);
        }
    }

    // Half-open [from, to) slice, clamped; from > to yields an empty Array.
    Array slice(std::size_t from, std::size_t to) const {
        Array out;
        if (from > m_items.size()) {
            from = m_items.size();
        }
        if (to > m_items.size()) {
            to = m_items.size();
        }
        for (std::size_t i = from; i < to; ++i) {
            out.append(m_items[i]);
        }
        return out;
    }

    const std::vector<Variant>& data() const { return m_items; }

private:
    std::vector<Variant> m_items;
};

class Dictionary {
public:
    std::size_t size() const { return m_order.size(); }
    bool isEmpty() const { return m_order.empty(); }
    void clear() {
        m_order.clear();
        m_map.clear();
    }

    bool has(const std::string& key) const { return m_map.find(key) != m_map.end(); }

    // Value for `key`, or `fallback` when absent (Godot's Dictionary.get).
    Variant get(const std::string& key, const Variant& fallback = Variant()) const {
        const auto it = m_map.find(key);
        return it == m_map.end() ? fallback : it->second;
    }

    void set(const std::string& key, const Variant& value) {
        const auto it = m_map.find(key);
        if (it == m_map.end()) {
            m_order.push_back(key);
        }
        m_map[key] = value;
    }

    // Insert a Nil default (and record order) on first access to a missing key, like Godot's dict[key].
    Variant& operator[](const std::string& key) {
        const auto it = m_map.find(key);
        if (it == m_map.end()) {
            m_order.push_back(key);
            return m_map[key];
        }
        return it->second;
    }

    bool erase(const std::string& key) {
        const auto it = m_map.find(key);
        if (it == m_map.end()) {
            return false;
        }
        m_map.erase(it);
        for (std::size_t i = 0; i < m_order.size(); ++i) {
            if (m_order[i] == key) {
                m_order.erase(m_order.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
        return true;
    }

    // Keys in insertion order (Godot dictionaries are ordered).
    std::vector<std::string> keys() const { return m_order; }
    std::vector<Variant> values() const {
        std::vector<Variant> out;
        out.reserve(m_order.size());
        for (const auto& k : m_order) {
            out.push_back(m_map.at(k));
        }
        return out;
    }

    // Copy entries from `other`; when `overwrite` is false, existing keys are kept (Godot's merge).
    void merge(const Dictionary& other, bool overwrite = true) {
        for (const auto& k : other.m_order) {
            if (overwrite || !has(k)) {
                set(k, other.m_map.at(k));
            }
        }
    }

private:
    std::vector<std::string> m_order;
    std::unordered_map<std::string, Variant> m_map;
};

} // namespace maz::core
