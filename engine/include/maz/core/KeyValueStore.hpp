#pragma once

#include <string>
#include <unordered_map>

namespace maz::core {

// A tiny persistent key/value store backed by a line-based `key=value` text file. Standard-
// library only (no SDL/JSON dependency) — the caller supplies the full file path (see
// platform::prefPath for a cross-platform writable location). Handy for settings, high scores,
// and simple progress.
class KeyValueStore {
public:
    // Load from `path`. A missing file is not an error (starts empty); the path is remembered
    // for save().
    bool load(const std::string& path);
    // Write the current contents back to the loaded path.
    bool save() const;

    bool has(const std::string& key) const;
    int getInt(const std::string& key, int fallback = 0) const;
    float getFloat(const std::string& key, float fallback = 0.0f) const;
    std::string getString(const std::string& key, const std::string& fallback = "") const;

    void set(const std::string& key, int value);
    void set(const std::string& key, float value);
    void setString(const std::string& key, const std::string& value);

private:
    std::string m_path;
    std::unordered_map<std::string, std::string> m_values;
};

} // namespace maz::core
