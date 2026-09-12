#pragma once

#include "maz/ext/Extension.hpp"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// maz::ext dynamic extension loader — the real shared-library ("[DESK]") half of the GDExtension-style ABI
// in Extension.hpp. That header models the registry + version negotiation an in-process plugin uses; THIS
// one actually opens a compiled `.so`/`.dll`/`.dylib` at runtime, resolves the plugin's exported entry
// symbols, negotiates the ABI version, and runs the plugin against a host registry — exactly how Godot loads
// a GDExtension without recompiling the engine. A Maz extension library exports two C symbols:
//   extern "C" void maz_extension_abi_version(int* major, int* minor);   // the ABI it was built against
//   extern "C" bool maz_extension_entry(int major, int minor, maz::ext::ExtensionRegistry& reg);
// The loader reads the version, rejects an incompatible plugin up front (reusing `abiCompatible`), and only
// then calls the entry point so the plugin can register its classes/methods.
//
// This is genuinely end-to-end verifiable on this box: the test compiles a real sample plugin into a shared
// library, loads it through here, and instantiates + calls a class the plugin registered — no mock. POSIX
// uses dlopen/dlsym; Windows uses LoadLibrary/GetProcAddress (compiled-blind here, exercised on Windows).
namespace maz::ext {

// The C symbols every Maz extension shared library must export.
inline constexpr const char* kAbiVersionSymbol = "maz_extension_abi_version";
inline constexpr const char* kEntrySymbol = "maz_extension_entry";

using ExtAbiVersionFn = void (*)(int* major, int* minor);

// RAII handle to an opened shared library. Move-only; the library stays mapped (and its function pointers
// valid) until this handle is destroyed or close()d — so keep it alive as long as the plugin is in use.
class DynamicLibrary {
public:
    DynamicLibrary() = default;
    ~DynamicLibrary() { close(); }

    DynamicLibrary(DynamicLibrary&& o) noexcept : m_handle(o.m_handle) { o.m_handle = nullptr; }
    DynamicLibrary& operator=(DynamicLibrary&& o) noexcept {
        if (this != &o) {
            close();
            m_handle = o.m_handle;
            o.m_handle = nullptr;
        }
        return *this;
    }
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // Open a shared library by path. Returns an invalid handle (and sets *err) on failure.
    static DynamicLibrary open(const std::string& path, std::string* err = nullptr) {
        DynamicLibrary lib;
#if defined(_WIN32)
        HMODULE h = LoadLibraryA(path.c_str());
        if (h == nullptr) {
            if (err) *err = "LoadLibrary failed for " + path;
            return lib;
        }
        lib.m_handle = reinterpret_cast<void*>(h);
#else
        void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (h == nullptr) {
            if (err) {
                const char* e = dlerror();
                *err = e ? std::string(e) : ("dlopen failed for " + path);
            }
            return lib;
        }
        lib.m_handle = h;
#endif
        return lib;
    }

    // Resolve an exported symbol as a raw pointer (nullptr if absent).
    void* symbolRaw(const char* name) const {
        if (m_handle == nullptr) return nullptr;
#if defined(_WIN32)
        return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(m_handle), name));
#else
        return dlsym(m_handle, name);
#endif
    }

    // Resolve an exported symbol as a typed function pointer.
    template <class Fn>
    Fn symbol(const char* name) const {
        return reinterpret_cast<Fn>(symbolRaw(name));
    }

    bool valid() const { return m_handle != nullptr; }

    void close() {
        if (m_handle != nullptr) {
#if defined(_WIN32)
            FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
#else
            dlclose(m_handle);
#endif
            m_handle = nullptr;
        }
    }

private:
    void* m_handle = nullptr;
};

// Open a Maz extension shared library, negotiate its ABI version, and run its entry point against `reg`.
// On success returns an OPEN DynamicLibrary — keep it alive while the plugin's registered callbacks are in
// use. On any failure (open error, missing symbols, incompatible ABI, entry returned false) it returns an
// invalid handle and sets *err.
inline DynamicLibrary loadExtensionLibrary(const std::string& path, ExtensionRegistry& reg,
                                           std::string* err = nullptr) {
    DynamicLibrary lib = DynamicLibrary::open(path, err);
    if (!lib.valid()) return lib;

    const ExtAbiVersionFn verFn = lib.symbol<ExtAbiVersionFn>(kAbiVersionSymbol);
    const ExtEntryFn entry = lib.symbol<ExtEntryFn>(kEntrySymbol);
    if (verFn == nullptr || entry == nullptr) {
        if (err) *err = "extension is missing required symbols (" + std::string(kAbiVersionSymbol) + " / " +
                        std::string(kEntrySymbol) + ")";
        lib.close();
        return lib;
    }

    int major = -1, minor = -1;
    verFn(&major, &minor);
    if (!abiCompatible(major, minor)) {
        if (err) *err = "extension ABI " + std::to_string(major) + "." + std::to_string(minor) +
                        " is incompatible with host " + std::to_string(kAbiMajor) + "." +
                        std::to_string(kAbiMinor);
        lib.close();
        return lib;
    }
    if (!entry(major, minor, reg)) {
        if (err) *err = "extension entry point returned failure";
        lib.close();
        return lib;
    }
    return lib;
}

} // namespace maz::ext
