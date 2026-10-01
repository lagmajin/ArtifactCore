module;
#include <utility>

export module Core.ArtifactDict;

import Core.ArtifactHashMap;
import Core.ArtifactOptional;

export namespace ArtifactCore {

/// Safe dictionary — get() returns Optional, forcing missing-key handling.
/// AI cannot silently ignore missing keys.
template <typename K, typename V>
class ArtifactDict {
public:
    void set(const K& key, const V& value) { map_.insert(key, value); }
    void set(const K& key, V&& value) { map_.insert(key, std::move(value)); }

    /// Returns value if key exists, nullopt otherwise.
    /// Forces caller to handle missing case. No silent default-construction.
    ArtifactOptional<V> get(const K& key) const {
        auto it = map_.find(key);
        if (it != map_.end()) return ArtifactOptional<V>(it->second);
        return ArtifactNullopt;
    }

    /// Convenience: returns value or fallback. Explicit about the fallback path.
    V getOr(const K& key, const V& fallback) const {
        auto it = map_.find(key);
        return (it != map_.end()) ? it->second : fallback;
    }

    /// Returns true and writes value if key exists. Safer than get() for value types.
    bool tryGet(const K& key, V& outValue) const {
        auto it = map_.find(key);
        if (it != map_.end()) { outValue = it->second; return true; }
        return false;
    }

    bool contains(const K& key) const { return map_.contains(key); }
    int size() const { return static_cast<int>(map_.size()); }
    bool isEmpty() const { return map_.empty(); }
    void remove(const K& key) { map_.erase(key); }
    void removeAll() { map_.clear(); }

private:
    ArtifactHashMap<K, V> map_;
};

/// Unordered variant — same safe API, backed by ArtifactHashMap.
template <typename K, typename V>
class ArtifactOrderedDict {
public:
    void set(const K& key, const V& value) { map_.insert(key, value); }
    ArtifactOptional<V> get(const K& key) const {
        auto it = map_.find(key);
        if (it != map_.end()) return ArtifactOptional<V>(it->second);
        return ArtifactNullopt;
    }
    V getOr(const K& key, const V& fallback) const {
        auto it = map_.find(key);
        return (it != map_.end()) ? it->second : fallback;
    }
    bool tryGet(const K& key, V& outValue) const {
        auto it = map_.find(key);
        if (it != map_.end()) { outValue = it->second; return true; }
        return false;
    }
    bool contains(const K& key) const { return map_.contains(key); }
    int size() const { return static_cast<int>(map_.size()); }
    bool isEmpty() const { return map_.empty(); }
    void remove(const K& key) { map_.erase(key); }
    void removeAll() { map_.clear(); }
private:
    ArtifactHashMap<K, V> map_;
};

} // namespace ArtifactCore

