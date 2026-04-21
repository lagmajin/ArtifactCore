module;

#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <mutex>

export module Data.DataCache;

import Data.DataTable;
import Data.DataSource;

export namespace ArtifactCore {

struct CacheEntry {
    DataSourcePtr source;
    int64_t loadedAtMs = 0;
    int64_t lastAccessMs = 0;
    int64_t fileModifiedMs = 0;
    int hitCount = 0;
};

class DataCache {
public:
    static DataCache& instance() {
        static DataCache inst;
        return inst;
    }

    void setMaxEntries(int max) { maxEntries_ = max; }
    int maxEntries() const { return maxEntries_; }

    void setTtlMs(int64_t ttl) { ttlMs_ = ttl; }
    int64_t ttlMs() const { return ttlMs_; }

    DataSourcePtr get(const std::string& uri) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = entries_.find(uri);
        if (it == entries_.end()) return nullptr;

        auto& entry = it->second;

        if (ttlMs_ > 0) {
            int64_t now = currentTimestampMs();
            if (now - entry.loadedAtMs > ttlMs_) {
                entries_.erase(it);
                return nullptr;
            }
        }

        entry.lastAccessMs = currentTimestampMs();
        entry.hitCount++;
        return entry.source;
    }

    void put(const std::string& uri, DataSourcePtr source, int64_t fileModifiedMs) {
        std::lock_guard<std::mutex> lock(mutex_);

        evictIfNeeded();

        CacheEntry entry;
        entry.source = std::move(source);
        entry.loadedAtMs = currentTimestampMs();
        entry.lastAccessMs = entry.loadedAtMs;
        entry.fileModifiedMs = fileModifiedMs;
        entry.hitCount = 0;

        entries_[uri] = std::move(entry);
    }

    void invalidate(const std::string& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.erase(uri);
    }

    void invalidateAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    bool isStale(const std::string& uri, int64_t currentFileModifiedMs) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(uri);
        if (it == entries_.end()) return true;
        return it->second.fileModifiedMs != currentFileModifiedMs;
    }

    int size() const { return static_cast<int>(entries_.size()); }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    DataCache() : maxEntries_(256), ttlMs_(300000) {}

    void evictIfNeeded() {
        while (static_cast<int>(entries_.size()) >= maxEntries_) {
            auto oldest = entries_.begin();
            for (auto it = entries_.begin(); it != entries_.end(); ++it) {
                if (it->second.lastAccessMs < oldest->second.lastAccessMs) {
                    oldest = it;
                }
            }
            entries_.erase(oldest);
        }
    }

    static int64_t currentTimestampMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    int maxEntries_;
    int64_t ttlMs_;
    std::unordered_map<std::string, CacheEntry> entries_;
    mutable std::mutex mutex_;
};

} // namespace ArtifactCore
