#pragma once

#include "maz/platform/PlatformBackend.hpp" // platform::SoftKeyboardType

// maz::platform soft-keyboard helper — a name for each on-screen keyboard layout kind, for logging/config.
// The show/hide/visibility control lives on PlatformBackend (no-op on desktop, the OS IME on mobile); this is
// just the pure label. Header-only.
namespace maz::platform {

inline const char* softKeyboardTypeName(SoftKeyboardType t) {
    switch (t) {
        case SoftKeyboardType::Default: return "default";
        case SoftKeyboardType::Number: return "number";
        case SoftKeyboardType::Email: return "email";
        case SoftKeyboardType::Phone: return "phone";
        case SoftKeyboardType::Url: return "url";
    }
    return "unknown";
}

} // namespace maz::platform
