// examples/plugins/counter_plugin.cpp — a minimal Maz extension shared library (GDExtension analog). Built
// as a MODULE (.so/.dll/.dylib) and loaded at runtime by maz::ext::loadExtensionLibrary WITHOUT recompiling
// the engine. It exports the two required C symbols and registers one class, "Counter", with an "add"
// method that accumulates an integer and returns the running total — enough to prove the whole ABI path
// (open -> version negotiate -> entry -> registerClass/Method -> instantiate -> call -> destroy).
#include "maz/ext/Extension.hpp"

#include <cstdint>

using maz::ext::ExtensionRegistry;
using maz::ext::ExtVariant;

namespace {

struct Counter {
    std::int64_t value = 0;
};

void* counterCreate() { return new Counter(); }
void counterDestroy(void* self) { delete static_cast<Counter*>(self); }

// add(amount:int) -> int : add `amount` to the running total and return the new total.
ExtVariant counterAdd(void* self, const ExtVariant* args, int argc) {
    Counter* c = static_cast<Counter*>(self);
    if (argc >= 1 && args != nullptr && args[0].type == ExtVariant::Type::Int) {
        c->value += args[0].i;
    }
    return ExtVariant::fromInt(c->value);
}

} // namespace

// The two symbols a Maz extension library must export (C linkage, stable names).
extern "C" void maz_extension_abi_version(int* major, int* minor) {
    if (major != nullptr) *major = maz::ext::kAbiMajor;
    if (minor != nullptr) *minor = maz::ext::kAbiMinor;
}

extern "C" bool maz_extension_entry(int /*major*/, int /*minor*/, ExtensionRegistry& reg) {
    if (!reg.registerClass("Counter", counterCreate, counterDestroy)) return false;
    if (!reg.registerMethod("Counter", "add", counterAdd)) return false;
    return true;
}
