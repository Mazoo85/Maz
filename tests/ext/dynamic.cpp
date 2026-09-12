// tests/ext/dynamic.cpp — end-to-end verification of the dynamic extension loader (maz::ext::DynamicLibrary
// / loadExtensionLibrary). This is NOT a mock: the build compiles examples/plugins/counter_plugin.cpp into a
// real shared library (a CMake MODULE) and passes its path in as MAZ_COUNTER_PLUGIN_PATH; the test loads
// that .so at runtime, then instantiates and calls a "Counter" class the plugin registered across the C ABI.
// Also checks graceful failure on a missing file and rejection of an incompatible ABI major version.
#include "maz/ext/DynamicLibrary.hpp"

#include <cstdio>
#include <string>

#ifndef MAZ_COUNTER_PLUGIN_PATH
#error "MAZ_COUNTER_PLUGIN_PATH must be defined by the build to the sample plugin shared library"
#endif

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::ext;

int main() {
    // --- Happy path: load a REAL shared library and drive a class it registered. ---
    ExtensionRegistry reg;
    std::string err;
    DynamicLibrary lib = loadExtensionLibrary(MAZ_COUNTER_PLUGIN_PATH, reg, &err);
    CHECK(lib.valid(), "loaded the real sample plugin .so");
    if (!lib.valid()) std::printf("  err=%s\n", err.c_str());
    CHECK(reg.hasClass("Counter"), "plugin registered the Counter class");
    CHECK(reg.hasMethod("Counter", "add"), "plugin registered the add method");

    void* obj = reg.instantiate("Counter");
    CHECK(obj != nullptr, "instantiated Counter across the ABI");
    ExtVariant a5 = ExtVariant::fromInt(5), a7 = ExtVariant::fromInt(7);
    bool ok = false;
    ExtVariant r1 = reg.call("Counter", obj, "add", &a5, 1, &ok);
    CHECK(ok && r1.type == ExtVariant::Type::Int && r1.i == 5, "add(5) -> 5");
    ExtVariant r2 = reg.call("Counter", obj, "add", &a7, 1, &ok);
    CHECK(ok && r2.i == 12, "add(7) -> 12 (plugin object keeps state)");
    reg.destroy("Counter", obj);

    // --- A missing library must fail gracefully with an error and register nothing. ---
    ExtensionRegistry reg2;
    std::string err2;
    DynamicLibrary bad = loadExtensionLibrary("/no/such/maz_plugin.so", reg2, &err2);
    CHECK(!bad.valid() && !err2.empty(), "missing library fails gracefully with a message");
    CHECK(reg2.classCount() == 0, "nothing registered from a failed load");

    // --- An incompatible ABI major must be rejected through the real exported entry symbol. ---
    ExtEntryFn entry = lib.symbol<ExtEntryFn>(kEntrySymbol);
    CHECK(entry != nullptr, "resolved the entry symbol from the library");
    ExtensionRegistry reg3;
    const bool loaded = loadExtension(entry, kAbiMajor + 1, kAbiMinor, reg3); // wrong major
    CHECK(!loaded && reg3.classCount() == 0, "incompatible ABI major is rejected");

    if (g_fail == 0) {
        std::printf("ext_dynamic: OK — real .so load, cross-ABI class call, graceful fail, ABI gate.\n");
        return 0;
    }
    std::printf("ext_dynamic: %d failure(s).\n", g_fail);
    return 1;
}
