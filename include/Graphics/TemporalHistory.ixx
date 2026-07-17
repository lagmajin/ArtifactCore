module;
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.TemporalHistory;

export namespace ArtifactCore {

struct TemporalHistoryKey {
    std::uint64_t compositionId = 0;
    std::uint64_t viewId = 0;
    std::string passId;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t quality = 0;

    bool operator==(const TemporalHistoryKey&) const noexcept = default;
};

struct TemporalHistoryKeyHash {
    std::size_t operator()(const TemporalHistoryKey& key) const noexcept
    {
        std::size_t seed = std::hash<std::uint64_t>{}(key.compositionId);
        const auto combine = [&seed](const std::size_t value) {
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        };
        combine(std::hash<std::uint64_t>{}(key.viewId));
        combine(std::hash<std::string>{}(key.passId));
        combine(std::hash<std::uint32_t>{}(key.width));
        combine(std::hash<std::uint32_t>{}(key.height));
        combine(std::hash<std::uint32_t>{}(key.quality));
        return seed;
    }
};

enum class TemporalInvalidationReason : std::uint8_t {
    None,
    CameraCut,
    TimeDiscontinuity,
    ResolutionChanged,
    QualityChanged,
    SceneChanged,
    Explicit
};

struct TemporalHistoryEntry {
    std::uint64_t resourceToken = 0;
    std::int64_t frame = 0;
    std::uint64_t revision = 0;
    TemporalInvalidationReason invalidationReason = TemporalInvalidationReason::None;
    bool valid = false;
};

class LIBRARY_DLL_API TemporalHistoryRegistry {
public:
    const TemporalHistoryEntry* find(const TemporalHistoryKey& key) const noexcept
    {
        const auto iterator = entries_.find(key);
        return iterator == entries_.end() ? nullptr : &iterator->second;
    }

    TemporalHistoryEntry& store(const TemporalHistoryKey& key,
                                const std::uint64_t resourceToken,
                                const std::int64_t frame)
    {
        auto& entry = entries_[key];
        entry.resourceToken = resourceToken;
        entry.frame = frame;
        entry.revision = ++revision_;
        entry.invalidationReason = TemporalInvalidationReason::None;
        entry.valid = resourceToken != 0;
        return entry;
    }

    bool invalidate(const TemporalHistoryKey& key,
                    const TemporalInvalidationReason reason = TemporalInvalidationReason::Explicit)
    {
        const auto iterator = entries_.find(key);
        if (iterator == entries_.end()) return false;
        invalidateEntry(iterator->second, reason);
        return true;
    }

    std::size_t invalidateView(const std::uint64_t compositionId,
                               const std::uint64_t viewId,
                               const TemporalInvalidationReason reason)
    {
        return invalidateMatching([=](const TemporalHistoryKey& key) {
            return key.compositionId == compositionId && key.viewId == viewId;
        }, reason);
    }

    std::size_t invalidateComposition(const std::uint64_t compositionId,
                                      const TemporalInvalidationReason reason)
    {
        return invalidateMatching([=](const TemporalHistoryKey& key) {
            return key.compositionId == compositionId;
        }, reason);
    }

    void invalidateAll(const TemporalInvalidationReason reason)
    {
        for (auto& [key, entry] : entries_) {
            (void)key;
            invalidateEntry(entry, reason);
        }
    }

    bool isReusable(const TemporalHistoryKey& key, const std::int64_t currentFrame) const noexcept
    {
        const auto* entry = find(key);
        return entry && entry->valid && entry->frame + 1 == currentFrame;
    }

    std::vector<TemporalHistoryKey> validKeys() const
    {
        std::vector<TemporalHistoryKey> result;
        result.reserve(entries_.size());
        for (const auto& [key, entry] : entries_) if (entry.valid) result.push_back(key);
        return result;
    }

    void eraseInvalid()
    {
        for (auto iterator = entries_.begin(); iterator != entries_.end();) {
            if (!iterator->second.valid) iterator = entries_.erase(iterator);
            else ++iterator;
        }
    }

    std::size_t size() const noexcept { return entries_.size(); }
    std::uint64_t revision() const noexcept { return revision_; }

private:
    template <typename Predicate>
    std::size_t invalidateMatching(Predicate predicate,
                                   const TemporalInvalidationReason reason)
    {
        std::size_t count = 0;
        for (auto& [key, entry] : entries_) {
            if (!predicate(key)) continue;
            invalidateEntry(entry, reason);
            ++count;
        }
        return count;
    }

    void invalidateEntry(TemporalHistoryEntry& entry,
                         const TemporalInvalidationReason reason)
    {
        entry.valid = false;
        entry.invalidationReason = reason;
        entry.revision = ++revision_;
    }

    std::unordered_map<TemporalHistoryKey, TemporalHistoryEntry, TemporalHistoryKeyHash> entries_;
    std::uint64_t revision_ = 0;
};

}
