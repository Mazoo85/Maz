#pragma once

#include <cmath>
#include <cstdint>

// maz::core::Fixed — a Q16.16 fixed-point number for DETERMINISTIC simulation. Floating point gives
// slightly different results on different CPUs/compilers/optimisation levels, which silently desyncs
// lockstep multiplayer, replays, and cross-platform physics. Fixed point is pure integer arithmetic:
// the same inputs give bit-identical outputs everywhere, so a lockstep netcode game (or a deterministic
// replay) can run the simulation in Fixed and trust every client agrees. Value = raw / 65536; 16 bits
// of integer range (±32767) and 16 bits of fraction (~1.5e-5 resolution). Multiply/divide use 64-bit
// intermediates so they don't overflow. Header-only, no floats on the runtime path (float is used only
// for authoring conversions). Godot has no fixed-point type, so this is a genuinely-useful utility.
namespace maz::core {

struct Fixed {
    static constexpr int kShift = 16;
    static constexpr std::int32_t kFracMask = 0xFFFF;
    std::int32_t raw = 0;

    constexpr Fixed() = default;

    static constexpr Fixed fromRaw(std::int32_t r) {
        Fixed f;
        f.raw = r;
        return f;
    }
    static constexpr Fixed fromInt(int v) {
        return fromRaw(static_cast<std::int32_t>(static_cast<std::int64_t>(v) << kShift));
    }
    // Authoring convenience (NOT for the deterministic runtime path — floats vary by platform).
    static Fixed fromFloat(double d) {
        return fromRaw(static_cast<std::int32_t>(std::llround(d * 65536.0)));
    }

    static constexpr Fixed zero() { return fromRaw(0); }
    static constexpr Fixed one() { return fromRaw(1 << kShift); }
    static constexpr Fixed half() { return fromRaw(1 << (kShift - 1)); }

    double toDouble() const { return static_cast<double>(raw) / 65536.0; }
    float toFloat() const { return static_cast<float>(toDouble()); }
    // Truncate toward zero (like an (int) cast).
    int toIntTrunc() const {
        return (raw >= 0)
                   ? (raw >> kShift)
                   : -static_cast<int>((-static_cast<std::int64_t>(raw)) >> kShift);
    }

    // Arithmetic (raw add/sub; 64-bit intermediates for mul/div).
    constexpr Fixed operator+(Fixed o) const { return fromRaw(raw + o.raw); }
    constexpr Fixed operator-(Fixed o) const { return fromRaw(raw - o.raw); }
    constexpr Fixed operator-() const { return fromRaw(-raw); }
    Fixed operator*(Fixed o) const {
        return fromRaw(static_cast<std::int32_t>(
            (static_cast<std::int64_t>(raw) * static_cast<std::int64_t>(o.raw)) >> kShift));
    }
    Fixed operator/(Fixed o) const {
        if (o.raw == 0) {
            return zero();
        }
        return fromRaw(static_cast<std::int32_t>(
            (static_cast<std::int64_t>(raw) << kShift) / static_cast<std::int64_t>(o.raw)));
    }
    Fixed& operator+=(Fixed o) { raw += o.raw; return *this; }
    Fixed& operator-=(Fixed o) { raw -= o.raw; return *this; }
    Fixed& operator*=(Fixed o) { *this = *this * o; return *this; }
    Fixed& operator/=(Fixed o) { *this = *this / o; return *this; }

    constexpr bool operator==(Fixed o) const { return raw == o.raw; }
    constexpr bool operator!=(Fixed o) const { return raw != o.raw; }
    constexpr bool operator<(Fixed o) const { return raw < o.raw; }
    constexpr bool operator<=(Fixed o) const { return raw <= o.raw; }
    constexpr bool operator>(Fixed o) const { return raw > o.raw; }
    constexpr bool operator>=(Fixed o) const { return raw >= o.raw; }

    constexpr Fixed abs() const { return fromRaw(raw < 0 ? -raw : raw); }
    // Largest integer <= value (toward -inf); arithmetic shift already floors.
    constexpr Fixed floor() const { return fromRaw(raw & ~kFracMask); }
    constexpr Fixed ceil() const { return fromRaw((raw + kFracMask) & ~kFracMask); }
    constexpr Fixed round() const { return fromRaw((raw + (1 << (kShift - 1))) & ~kFracMask); }
    // Fractional part in [0,1): value - floor(value).
    constexpr Fixed frac() const { return fromRaw(raw & kFracMask); }

    // Deterministic fixed-point square root (integer, no floats): sqrt(raw << 16) via bit-by-bit isqrt.
    Fixed sqrt() const {
        if (raw <= 0) {
            return zero();
        }
        std::uint64_t n = static_cast<std::uint64_t>(raw) << kShift;
        std::uint64_t result = 0;
        std::uint64_t bit = static_cast<std::uint64_t>(1) << 62;
        while (bit > n) {
            bit >>= 2;
        }
        while (bit != 0) {
            if (n >= result + bit) {
                n -= result + bit;
                result = (result >> 1) + bit;
            } else {
                result >>= 1;
            }
            bit >>= 2;
        }
        return fromRaw(static_cast<std::int32_t>(result));
    }
};

} // namespace maz::core
