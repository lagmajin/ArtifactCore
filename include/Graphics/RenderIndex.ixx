module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.RenderIndex;

export namespace ArtifactCore {

using RenderProxyId = std::uint64_t;

enum class RenderProxyKind : std::uint8_t {
    Unknown,
    Mesh,
    Image,
    Text,
    Volume,
    Light,
    Camera,
    Instances
};

enum class RenderDirty : std::uint32_t {
    None = 0,
    Transform = 1u << 0u,
    Geometry = 1u << 1u,
    Material = 1u << 2u,
    Visibility = 1u << 3u,
    Instances = 1u << 4u,
    All = (1u << 5u) - 1u
};

constexpr RenderDirty operator|(const RenderDirty lhs, const RenderDirty rhs) noexcept
{
    return static_cast<RenderDirty>(static_cast<std::uint32_t>(lhs) |
                                    static_cast<std::uint32_t>(rhs));
}

constexpr RenderDirty operator&(const RenderDirty lhs, const RenderDirty rhs) noexcept
{
    return static_cast<RenderDirty>(static_cast<std::uint32_t>(lhs) &
                                    static_cast<std::uint32_t>(rhs));
}

constexpr bool any(const RenderDirty value) noexcept
{
    return value != RenderDirty::None;
}

struct RenderBounds {
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
    bool valid = false;
};

struct RenderProxyDescriptor {
    RenderProxyId id = 0;
    RenderProxyId prototypeId = 0;
    RenderProxyKind kind = RenderProxyKind::Unknown;
    std::array<float, 16> worldTransform{1.0f, 0.0f, 0.0f, 0.0f,
                                         0.0f, 1.0f, 0.0f, 0.0f,
                                         0.0f, 0.0f, 1.0f, 0.0f,
                                         0.0f, 0.0f, 0.0f, 1.0f};
    RenderBounds bounds{};
    std::string geometryKey;
    std::string materialKey;
    std::uint32_t instanceOffset = 0;
    std::uint32_t instanceCount = 1;
    std::uint32_t visibilityMask = 0xffffffffu;
    bool visible = true;
};

struct RenderProxyRecord {
    RenderProxyDescriptor descriptor;
    RenderDirty dirty = RenderDirty::All;
    std::uint64_t revision = 1;
};

struct RenderIndexSnapshot {
    std::uint64_t generation = 0;
    std::vector<RenderProxyRecord> proxies;
};

class LIBRARY_DLL_API RenderIndex {
public:
    bool upsert(RenderProxyDescriptor descriptor, RenderDirty dirty = RenderDirty::All)
    {
        if (descriptor.id == 0) return false;
        auto [iterator, inserted] = proxies_.try_emplace(descriptor.id);
        auto& record = iterator->second;
        record.descriptor = std::move(descriptor);
        record.dirty = inserted ? RenderDirty::All : record.dirty | dirty;
        record.revision = inserted ? 1 : record.revision + 1;
        ++generation_;
        return true;
    }

    bool erase(const RenderProxyId id)
    {
        if (proxies_.erase(id) == 0) return false;
        ++generation_;
        return true;
    }

    bool markDirty(const RenderProxyId id, const RenderDirty dirty)
    {
        const auto iterator = proxies_.find(id);
        if (iterator == proxies_.end() || !any(dirty)) return false;
        iterator->second.dirty = iterator->second.dirty | dirty;
        ++iterator->second.revision;
        ++generation_;
        return true;
    }

    void clearDirty(const RenderProxyId id, const RenderDirty dirty = RenderDirty::All)
    {
        const auto iterator = proxies_.find(id);
        if (iterator == proxies_.end()) return;
        const auto retained = static_cast<std::uint32_t>(iterator->second.dirty) &
                              ~static_cast<std::uint32_t>(dirty);
        iterator->second.dirty = static_cast<RenderDirty>(retained);
    }

    const RenderProxyRecord* find(const RenderProxyId id) const noexcept
    {
        const auto iterator = proxies_.find(id);
        return iterator == proxies_.end() ? nullptr : &iterator->second;
    }

    RenderIndexSnapshot snapshot(const bool dirtyOnly = false) const
    {
        RenderIndexSnapshot result;
        result.generation = generation_;
        result.proxies.reserve(proxies_.size());
        for (const auto& [id, record] : proxies_) {
            (void)id;
            if (!dirtyOnly || any(record.dirty)) result.proxies.push_back(record);
        }
        return result;
    }

    std::size_t size() const noexcept { return proxies_.size(); }
    std::uint64_t generation() const noexcept { return generation_; }

private:
    std::unordered_map<RenderProxyId, RenderProxyRecord> proxies_;
    std::uint64_t generation_ = 0;
};

}
