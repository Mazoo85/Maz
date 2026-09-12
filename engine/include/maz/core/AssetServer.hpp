#pragma once

#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "maz/core/Jobs.hpp"

// maz::core::AssetServer — an asynchronous, reference-counted asset loader, Maz's answer to Godot's
// ResourceLoader (load_threaded_request / load_threaded_get_status / load_threaded_get) plus the
// .import reimport pipeline, in one header.
//
// The problem it solves: a game must not stall the frame decoding a texture or parsing a model. So
// loading is split in two, exactly like Godot:
//   1. request(path)  — enqueues a decode job on a background JobSystem thread and returns an
//                        AssetId immediately. The frame keeps running.
//   2. poll()         — called once per frame on the game thread; it harvests any jobs that
//   finished
//                        on workers, moves their results into the cache, and fires onLoaded
//                        callbacks. This is where a real engine would do the GPU upload — on the
//                        main thread, off the worker — which is why finalization is a separate
//                        step.
// status(id) reports Queued / Loading / Loaded / Failed; progress() gives a loaded/total pair for a
// loading screen. Identical paths dedupe to one entry with a reference count, so requesting the
// same texture from a hundred places loads it once (the ResourceCache guarantee, now async).
//
// Reimport / hot reload: each entry remembers the "stamp" of its source (a file mtime or content
// hash you supply). reimportChanged() re-decodes every entry whose stamp moved and bumps its
// version(), so a renderer can notice `version` changed and re-upload the new bytes — the runtime
// half of Godot's reimport. All CPU, thread-based, no GPU: templated on your decoded payload type
// T, so it unit-tests deterministically by driving request()->poll() to completion.
namespace maz::core {

// Load state of one asset, mirroring Godot's ResourceLoader::ThreadLoadStatus.
enum class AssetStatus { Queued, Loading, Loaded, Failed };

// Opaque handle to a requested asset. Stable for the asset's lifetime; comparable and hashable.
struct AssetId {
    uint32_t value = 0; // 0 == invalid
    bool valid() const { return value != 0; }
    bool operator==(const AssetId& o) const { return value == o.value; }
    bool operator!=(const AssetId& o) const { return value != o.value; }
};

template <typename T> class AssetServer {
  public:
    // loader(path) decodes the asset on a worker thread and returns the payload; throwing marks the
    // asset Failed. stamp(path) (optional) returns a change token for reimport detection — a file
    // mtime or content hash; the default treats sources as immutable (stamp 0).
    using Loader = std::function<T(const std::string&)>;
    using Stamp = std::function<uint64_t(const std::string&)>;
    using OnLoaded = std::function<void(AssetId, const std::string&, T&)>;

    AssetServer(JobSystem& jobs, Loader loader, Stamp stamp = {})
        : m_jobs(jobs), m_loader(std::move(loader)), m_stamp(std::move(stamp)) {}

    // Fire a load callback whenever an asset (or a reimport) finalizes in poll(). Optional.
    void setOnLoaded(OnLoaded cb) { m_onLoaded = std::move(cb); }

    // Request `path` asynchronously. Identical paths dedupe: an already-known path returns its
    // existing id and bumps its reference count instead of decoding again. Returns immediately.
    AssetId request(const std::string& path) {
        auto it = m_byPath.find(path);
        if (it != m_byPath.end()) {
            Entry& e = m_entries[it->second - 1];
            ++e.refs;
            return AssetId{it->second};
        }
        const uint32_t id = static_cast<uint32_t>(m_entries.size() + 1);
        Entry e;
        e.path = path;
        e.status = AssetStatus::Queued;
        e.refs = 1;
        e.stamp = m_stamp ? m_stamp(path) : 0u;
        e.future = launch(path);
        e.status = AssetStatus::Loading;
        m_entries.push_back(std::move(e));
        m_byPath.emplace(path, id);
        ++m_pending;
        return AssetId{id};
    }

    // Drop one reference; when the last reference goes the entry is cleared (its slot is retired
    // but the id space is never reused, so stale ids stay invalid rather than aliasing). Returns
    // true if this call evicted the asset.
    bool release(AssetId id) {
        Entry* e = get(id);
        if (!e || e->refs == 0) {
            return false;
        }
        if (--e->refs == 0) {
            if (e->status == AssetStatus::Loading) {
                // Let the in-flight job finish so the worker isn't left dangling, then discard.
                if (e->future.valid()) {
                    e->future.wait();
                }
                --m_pending;
            }
            m_byPath.erase(e->path);
            *e = Entry{}; // retire the slot
            e->status = AssetStatus::Failed;
            return true;
        }
        return false;
    }

    // Harvest finished worker jobs on the game thread: move results into the cache, run callbacks,
    // and (for a real engine) the place to do GPU uploads. Returns how many assets finalized this
    // call. Cheap to call every frame; only touches entries with a ready future.
    int poll() {
        int finalized = 0;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            Entry& e = m_entries[i];
            if (e.status != AssetStatus::Loading || !e.future.valid()) {
                continue;
            }
            if (e.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                continue;
            }
            const bool reload = e.value.has_value(); // a reimport replacing existing bytes
            try {
                e.value = e.future.get();
                e.status = AssetStatus::Loaded;
                ++e.version;
                if (m_onLoaded) {
                    m_onLoaded(AssetId{static_cast<uint32_t>(i + 1)}, e.path, *e.value);
                }
            } catch (...) {
                e.status = AssetStatus::Failed;
                e.value.reset();
            }
            if (!reload) {
                --m_pending;
            }
            ++finalized;
        }
        return finalized;
    }

    // Convenience: request + drive to completion (polls in a tight loop). For load-at-startup paths
    // and tests. Returns a pointer to the decoded value, or nullptr on failure.
    const T* blockingGet(const std::string& path) {
        AssetId id = request(path);
        Entry* e = get(id);
        if (!e) {
            return nullptr;
        }
        while (e->status == AssetStatus::Loading) {
            if (e->future.valid()) {
                e->future.wait();
            }
            poll();
        }
        return e->status == AssetStatus::Loaded && e->value ? &*e->value : nullptr;
    }

    AssetStatus status(AssetId id) const {
        const Entry* e = get(id);
        return e ? e->status : AssetStatus::Failed;
    }

    // The decoded payload once Loaded, else nullptr. Pointer stays valid until the id is evicted or
    // reimported (reimport replaces the value in place, so re-fetch after a version() change).
    const T* tryGet(AssetId id) const {
        const Entry* e = get(id);
        return (e && e->status == AssetStatus::Loaded && e->value) ? &*e->value : nullptr;
    }

    // A monotonically increasing per-asset counter, bumped on every (re)load. A consumer that
    // caches a GPU handle re-uploads when this changes — the signal that a reimport delivered new
    // bytes.
    uint32_t version(AssetId id) const {
        const Entry* e = get(id);
        return e ? e->version : 0u;
    }

    // Reference count for an asset (0 if evicted/unknown).
    uint32_t refCount(AssetId id) const {
        const Entry* e = get(id);
        return e ? e->refs : 0u;
    }

    // Loading progress for a splash/loading screen: {loaded, total} over currently-live assets.
    struct Progress {
        int loaded = 0;
        int total = 0;
    };
    Progress progress() const {
        Progress p;
        for (const Entry& e : m_entries) {
            if (e.refs == 0) {
                continue;
            }
            ++p.total;
            if (e.status == AssetStatus::Loaded) {
                ++p.loaded;
            }
        }
        return p;
    }

    bool allLoaded() const {
        Progress p = progress();
        return p.total == p.loaded;
    }
    int pending() const { return m_pending; }

    // Re-decode every live asset whose source stamp changed since it was loaded (the runtime half
    // of Godot's reimport). Schedules background reload jobs and returns how many were scheduled;
    // the new bytes land in a later poll(), bumping version(). No-op when no stamp function was
    // given.
    int reimportChanged() {
        if (!m_stamp) {
            return 0;
        }
        int scheduled = 0;
        for (Entry& e : m_entries) {
            if (e.refs == 0 || e.status == AssetStatus::Loading) {
                continue;
            }
            const uint64_t now = m_stamp(e.path);
            if (now != e.stamp) {
                e.stamp = now;
                e.future = launch(e.path);
                e.status = AssetStatus::Loading;
                ++scheduled;
            }
        }
        return scheduled;
    }

    // Force a reload of one asset regardless of stamp (e.g. an editor "Reimport" button).
    bool reimport(AssetId id) {
        Entry* e = get(id);
        if (!e || e->refs == 0 || e->status == AssetStatus::Loading) {
            return false;
        }
        e->stamp = m_stamp ? m_stamp(e->path) : e->stamp;
        e->future = launch(e->path);
        e->status = AssetStatus::Loading;
        return true;
    }

    size_t liveCount() const {
        size_t n = 0;
        for (const Entry& e : m_entries) {
            if (e.refs != 0) {
                ++n;
            }
        }
        return n;
    }

  private:
    struct Entry {
        std::string path;
        AssetStatus status = AssetStatus::Failed;
        uint32_t refs = 0;
        uint32_t version = 0;
        uint64_t stamp = 0;
        std::optional<T> value;
        std::future<T> future;
    };

    std::future<T> launch(const std::string& path) {
        Loader loader = m_loader;
        return m_jobs.submit([loader, path]() -> T { return loader(path); });
    }

    Entry* get(AssetId id) {
        if (id.value == 0 || id.value > m_entries.size()) {
            return nullptr;
        }
        return &m_entries[id.value - 1];
    }
    const Entry* get(AssetId id) const {
        if (id.value == 0 || id.value > m_entries.size()) {
            return nullptr;
        }
        return &m_entries[id.value - 1];
    }

    JobSystem& m_jobs;
    Loader m_loader;
    Stamp m_stamp;
    OnLoaded m_onLoaded;
    std::vector<Entry> m_entries;
    std::unordered_map<std::string, uint32_t> m_byPath;
    int m_pending = 0;
};

} // namespace maz::core
