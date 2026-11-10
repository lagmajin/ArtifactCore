module;

#include <string>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>

export module Data.DataCache;

import Data.DataTable;
import Data.DataSource;
import Core.ArtifactString;
import Container.Debug;
import Container.NameMap;

export namespace ArtifactCore {

struct CacheEntry {
    DataSourcePtr source;
    int64_t loadedAtMs = 0;
    int64_t lastAccessMs = 0;
    std::uint64_t lastAccessSequence = 0;
    int64_t fileModifiedMs = 0;
    int hitCount = 0;
};

class DataCache {
public:
    static DataCache& instance() {
        static DataCache inst;
        return inst;
    }

    void setMaxEntries(int max) { maxEntries_ = std::max(1, max); }
    int maxEntries() const { return maxEntries_; }

    void setTtlMs(int64_t ttl) { ttlMs_ = ttl; }
    int64_t ttlMs() const { return ttlMs_; }

    DataSourcePtr get(const String& uri) {
        std::lock_guard<std::mutex> lock(mutex_);

        const std::string uriStd = toStdString(uri);
        CacheEntry* const entry = entries_.findValue(uriStd);
        if (!entry) return nullptr;

        if (ttlMs_ > 0) {
            int64_t now = currentTimestampMs();
            if (now - entry->loadedAtMs > ttlMs_) {
                entries_.erase(uriStd);
                return nullptr;
            }
        }

        entry->lastAccessMs = currentTimestampMs();
        entry->lastAccessSequence = ++accessSequence_;
        entry->hitCount++;
        return entry->source;
    }

    void put(const String& uri, DataSourcePtr source, int64_t fileModifiedMs) {
        std::lock_guard<std::mutex> lock(mutex_);

        const std::string uriStd = toStdString(uri);
        if (!entries_.contains(uriStd)) {
            evictIfNeeded();
        }

        CacheEntry entry;
        entry.source = std::move(source);
        entry.loadedAtMs = currentTimestampMs();
        entry.lastAccessMs = entry.loadedAtMs;
        entry.lastAccessSequence = ++accessSequence_;
        entry.fileModifiedMs = fileModifiedMs;
        entry.hitCount = 0;

        entries_[uriStd] = std::move(entry);
    }

    void invalidate(const String& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.erase(toStdString(uri));
    }

    void invalidateAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    bool isStale(const String& uri, int64_t currentFileModifiedMs) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const CacheEntry* const entry = entries_.findValue(toStdString(uri));
        return !entry || entry->fileModifiedMs != currentFileModifiedMs;
    }

    int size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(entries_.size());
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    DataCache() : maxEntries_(256), ttlMs_(300000) {}

    void evictIfNeeded() {
        while (static_cast<int>(entries_.size()) >= maxEntries_) {
            std::string oldestKey;
            std::uint64_t oldestAccessSequence = std::numeric_limits<std::uint64_t>::max();
            bool found = false;
            entries_.eachKeyValue([&](const std::string& key, const CacheEntry& entry) {
                if (!found || entry.lastAccessSequence < oldestAccessSequence) {
                    oldestKey = key;
                    oldestAccessSequence = entry.lastAccessSequence;
                    found = true;
                }
            });
            if (!found) return;
            entries_.erase(oldestKey);
        }
    }

    static int64_t currentTimestampMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    int maxEntries_;
    int64_t ttlMs_;
    std::uint64_t accessSequence_ = 0;
    NameMap<std::string, CacheEntry> entries_{
        ContainerName{"Data.DataCache.entries"}};
    mutable std::mutex mutex_;
};

} // namespace ArtifactCore
