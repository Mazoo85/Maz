#pragma once

#include <cstdint>
#include <string>
#include <vector>

// maz::ext GDExtension-style C ABI — Godot's GDExtension: a stable, function-pointer interface that lets
// third parties add engine classes and methods from a separate shared library WITHOUT recompiling the
// engine. The ABI surface uses only C-compatible types (a tagged variant + plain function pointers), so
// a plugin compiled against a matching ABI version can register classes, be instantiated, and have its
// methods dispatched by name. This models the registry + dispatcher + version negotiation that make that
// work; the actual dlopen()/LoadLibrary of a .so/.dll is the [DESK] loader on top. Header-only and
// deterministic (a mock in-process "extension" stands in for a real library), so it unit-tests exactly.
namespace maz::ext {

// ABI version. A plugin declares the version it was built against; the host accepts it only if the major
// matches and the plugin's minor is <= the host's (backward-compatible additions bump the minor).
inline constexpr int kAbiMajor = 1;
inline constexpr int kAbiMinor = 0;
inline bool abiCompatible(int pluginMajor, int pluginMinor) {
    return pluginMajor == kAbiMajor && pluginMinor <= kAbiMinor;
}

// The ABI value type: a small tagged variant passed across the boundary (no std types in the payload
// except a borrowed C string, whose storage the caller owns).
struct ExtVariant {
    enum class Type { Nil, Bool, Int, Float, Str };
    Type type = Type::Nil;
    bool b = false;
    int64_t i = 0;
    double f = 0.0;
    const char* s = nullptr;

    static ExtVariant nil() { return ExtVariant{}; }
    static ExtVariant fromBool(bool v) {
        ExtVariant o;
        o.type = Type::Bool;
        o.b = v;
        return o;
    }
    static ExtVariant fromInt(int64_t v) {
        ExtVariant o;
        o.type = Type::Int;
        o.i = v;
        return o;
    }
    static ExtVariant fromFloat(double v) {
        ExtVariant o;
        o.type = Type::Float;
        o.f = v;
        return o;
    }
    static ExtVariant fromStr(const char* v) {
        ExtVariant o;
        o.type = Type::Str;
        o.s = v;
        return o;
    }
};

// C-ABI function-pointer signatures (no captures — exactly what a shared library exports).
using ExtCreateFn = void* (*)();
using ExtDestroyFn = void (*)(void* self);
using ExtMethodFn = ExtVariant (*)(void* self, const ExtVariant* args, int argc);

// The registry a host hands to plugins, and dispatches calls through. One instance owns all registered
// classes; a plugin's entry point calls registerClass / registerMethod on it.
class ExtensionRegistry {
  public:
    // Register a class with its lifecycle callbacks. Fails on a duplicate or empty name, or null create.
    bool registerClass(const std::string& name, ExtCreateFn create, ExtDestroyFn destroyFn) {
        if (name.empty() || create == nullptr || findClass(name) != nullptr) {
            return false;
        }
        m_classes.push_back(ClassInfo{name, create, destroyFn, {}});
        return true;
    }
    // Register a named method on an already-registered class. Fails if the class is unknown, the name is
    // empty/duplicate, or the fn is null.
    bool registerMethod(const std::string& className, const std::string& methodName, ExtMethodFn fn) {
        ClassInfo* c = findClass(className);
        if (c == nullptr || methodName.empty() || fn == nullptr || c->find(methodName) != nullptr) {
            return false;
        }
        c->methods.push_back({methodName, fn});
        return true;
    }

    bool hasClass(const std::string& name) const { return findClass(name) != nullptr; }
    bool hasMethod(const std::string& className, const std::string& methodName) const {
        const ClassInfo* c = findClass(className);
        return c != nullptr && c->find(methodName) != nullptr;
    }
    std::size_t classCount() const { return m_classes.size(); }
    std::size_t methodCount(const std::string& className) const {
        const ClassInfo* c = findClass(className);
        return c ? c->methods.size() : 0;
    }

    // Create an instance of a registered class (returns the plugin's opaque object, or nullptr).
    void* instantiate(const std::string& className) const {
        const ClassInfo* c = findClass(className);
        return c ? c->create() : nullptr;
    }
    // Destroy an instance previously created by instantiate().
    void destroy(const std::string& className, void* self) const {
        const ClassInfo* c = findClass(className);
        if (c && c->destroy && self) {
            c->destroy(self);
        }
    }

    // Dispatch a method by name. Sets *ok (if given) to false when the class/method is unknown, in which
    // case a Nil variant is returned.
    ExtVariant call(const std::string& className, void* self, const std::string& methodName,
                    const ExtVariant* args, int argc, bool* ok = nullptr) const {
        const ClassInfo* c = findClass(className);
        const ExtMethodFn fn = c ? c->find(methodName) : nullptr;
        if (fn == nullptr) {
            if (ok) {
                *ok = false;
            }
            return ExtVariant::nil();
        }
        if (ok) {
            *ok = true;
        }
        return fn(self, args, argc);
    }

    bool unregisterClass(const std::string& name) {
        for (std::size_t i = 0; i < m_classes.size(); ++i) {
            if (m_classes[i].name == name) {
                m_classes.erase(m_classes.begin() + static_cast<std::ptrdiff_t>(i));
                return true;
            }
        }
        return false;
    }

  private:
    struct Method {
        std::string name;
        ExtMethodFn fn;
    };
    struct ClassInfo {
        std::string name;
        ExtCreateFn create;
        ExtDestroyFn destroy;
        std::vector<Method> methods;
        ExtMethodFn find(const std::string& m) const {
            for (const Method& mm : methods) {
                if (mm.name == m) {
                    return mm.fn;
                }
            }
            return nullptr;
        }
    };
    ClassInfo* findClass(const std::string& name) {
        for (ClassInfo& c : m_classes) {
            if (c.name == name) {
                return &c;
            }
        }
        return nullptr;
    }
    const ClassInfo* findClass(const std::string& name) const {
        for (const ClassInfo& c : m_classes) {
            if (c.name == name) {
                return &c;
            }
        }
        return nullptr;
    }

    std::vector<ClassInfo> m_classes;
};

// A plugin's entry point has this shape: given its ABI version and the host registry, it registers its
// classes and returns true on success. A real loader would resolve this symbol from a shared library.
using ExtEntryFn = bool (*)(int pluginMajor, int pluginMinor, ExtensionRegistry& reg);

// Host side: invoke a plugin entry point, rejecting it up front if its ABI version is incompatible.
inline bool loadExtension(ExtEntryFn entry, int pluginMajor, int pluginMinor,
                          ExtensionRegistry& reg) {
    if (entry == nullptr || !abiCompatible(pluginMajor, pluginMinor)) {
        return false;
    }
    return entry(pluginMajor, pluginMinor, reg);
}

} // namespace maz::ext
