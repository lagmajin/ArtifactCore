module;
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iterator>
#include <functional>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.RenderGraph;

export namespace ArtifactCore {

class RenderGraph;

struct RenderResourceHandle {
    std::uint32_t id = 0;
    constexpr explicit operator bool() const noexcept { return id != 0; }
    constexpr bool operator==(const RenderResourceHandle&) const noexcept = default;
};

struct RenderPassHandle {
    std::uint32_t id = 0;
    constexpr explicit operator bool() const noexcept { return id != 0; }
    constexpr bool operator==(const RenderPassHandle&) const noexcept = default;
};

enum class RenderResourceKind : std::uint8_t { Texture, Buffer };
enum class RenderResourceLifetime : std::uint8_t { Transient, Persistent, External };
enum class RenderPassQueue : std::uint8_t { Graphics, Compute, Copy };
enum class RenderDiagnosticPassState : std::uint8_t { Scheduled, Disabled, Blocked };

struct RenderResourceDescriptor {
    std::string name;
    RenderResourceKind kind = RenderResourceKind::Texture;
    RenderResourceLifetime lifetime = RenderResourceLifetime::Transient;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 1;
    // Backend-neutral format key; 0 means unknown and disables texture aliasing.
    std::uint32_t format = 0;
    std::uint64_t byteSize = 0;
};

struct RenderPassDescriptor {
    std::string name;
    RenderPassQueue queue = RenderPassQueue::Graphics;
    std::vector<RenderResourceHandle> reads;
    std::vector<RenderResourceHandle> writes;
    bool enabled = true;
    // Opt in only when the pass executor has no unsynchronized shared state.
    bool parallelSafe = false;
};

struct RenderResourceLifetimeRange {
    RenderResourceHandle resource;
    std::size_t firstPass = 0;
    std::size_t lastPass = 0;
    std::size_t allocationSlot = 0;
};

struct RenderAllocationSlotDescriptor {
    std::size_t index = 0;
    RenderResourceKind kind = RenderResourceKind::Texture;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 1;
    std::uint32_t format = 0;
    std::uint64_t byteSize = 0;
};

struct CompiledRenderGraph {
    bool valid = false;
    std::uint64_t graphIdentity = 0;
    std::uint64_t graphRevision = 0;
    std::string error;
    std::vector<RenderPassHandle> passOrder;
    // Level-synchronous execution groups: passes inside one level have no
    // dependency path between them and may run concurrently. Levels are in
    // execution order; concatenating them yields a valid topological order.
    std::vector<std::vector<RenderPassHandle>> executionLevels;
    // Mirrors passOrder so transient allocation can require strict
    // happens-before between resources that share a physical slot.
    std::vector<std::size_t> passOrderLevels;
    std::vector<RenderResourceLifetimeRange> lifetimes;
    std::size_t allocationSlotCount = 0;
    std::vector<RenderAllocationSlotDescriptor> allocationSlots;

    const RenderResourceLifetimeRange* lifetime(
        const RenderResourceHandle handle) const noexcept
    {
        for (const auto& range : lifetimes) {
            if (range.resource == handle) return &range;
        }
        return nullptr;
    }

    const RenderAllocationSlotDescriptor* allocationSlot(
        const std::size_t index) const noexcept
    {
        for (const auto& slot : allocationSlots) {
            if (slot.index == index) return &slot;
        }
        return nullptr;
    }
};

struct RenderGraphExecutionContext {
    RenderPassHandle pass;
    const RenderPassDescriptor& descriptor;
    const RenderGraph& graph;
    const CompiledRenderGraph& compiled;
};

using RenderPassExecutor = std::function<bool(const RenderGraphExecutionContext&)>;

struct RenderDiagnosticResourceRecord {
    RenderResourceHandle handle;
    RenderResourceDescriptor descriptor;
    std::size_t firstPass = 0;
    std::size_t lastPass = 0;
    std::size_t allocationSlot = 0;
    bool used = false;
};

struct RenderDiagnosticPassRecord {
    RenderPassHandle handle;
    RenderPassDescriptor descriptor;
    RenderDiagnosticPassState state = RenderDiagnosticPassState::Blocked;
    // Stable, backend-neutral explanation for the current scheduling state.
    std::string stateReason;
    std::size_t executionOrder = 0;
    std::size_t executionLevel = 0;
    std::uint64_t gpuDurationUs = 0;
    std::uint64_t gpuSampleExecutionId = 0;
    bool gpuTimingAvailable = false;
};

struct RenderGraphDiagnosticSnapshot {
    std::uint64_t executionId = 0;
    bool valid = false;
    std::string error;
    std::uint64_t estimatedResourceBytes = 0;
    std::uint64_t estimatedAliasedResourceBytes = 0;
    std::vector<RenderAllocationSlotDescriptor> allocationSlots;
    std::vector<RenderDiagnosticPassRecord> passes;
    std::vector<RenderDiagnosticResourceRecord> resources;
};

class LIBRARY_DLL_API RenderGraph {
public:
    RenderGraph() = default;

    RenderGraph(const RenderGraph& other)
        : resources_(other.resources_),
          passes_(other.passes_),
          nextResourceId_(other.nextResourceId_),
          nextPassId_(other.nextPassId_),
          revision_(other.revision_),
          graphIdentity_(nextGraphIdentity()) {}

    RenderGraph& operator=(const RenderGraph& other)
    {
        if (this == &other) return *this;
        RenderGraph copy(other);
        *this = std::move(copy);
        return *this;
    }

    RenderGraph(RenderGraph&& other) noexcept
        : resources_(std::move(other.resources_)),
          passes_(std::move(other.passes_)),
          nextResourceId_(other.nextResourceId_),
          nextPassId_(other.nextPassId_),
          revision_(other.revision_),
          graphIdentity_(std::exchange(other.graphIdentity_,
                                       nextGraphIdentity())) {}

    RenderGraph& operator=(RenderGraph&& other) noexcept
    {
        if (this == &other) return *this;
        resources_ = std::move(other.resources_);
        passes_ = std::move(other.passes_);
        nextResourceId_ = other.nextResourceId_;
        nextPassId_ = other.nextPassId_;
        revision_ = other.revision_;
        graphIdentity_ = std::exchange(other.graphIdentity_,
                                       nextGraphIdentity());
        return *this;
    }

    RenderResourceHandle addResource(RenderResourceDescriptor descriptor)
    {
        const RenderResourceHandle handle{nextResourceId_++};
        resources_.push_back({handle, std::move(descriptor)});
        markMutated();
        return handle;
    }

    RenderPassHandle addPass(RenderPassDescriptor descriptor)
    {
        const RenderPassHandle handle{nextPassId_++};
        passes_.push_back({handle, std::move(descriptor)});
        markMutated();
        return handle;
    }

    const RenderResourceDescriptor* resource(const RenderResourceHandle handle) const noexcept
    {
        if (!handle) return nullptr;
        const auto index = static_cast<std::size_t>(handle.id - 1);
        if (index >= resources_.size() || resources_[index].handle != handle) return nullptr;
        return &resources_[index].descriptor;
    }

    const RenderPassDescriptor* pass(const RenderPassHandle handle) const noexcept
    {
        if (!handle) return nullptr;
        const auto index = static_cast<std::size_t>(handle.id - 1);
        if (index >= passes_.size() || passes_[index].handle != handle) return nullptr;
        return &passes_[index].descriptor;
    }

    CompiledRenderGraph compile() const
    {
        CompiledRenderGraph result;
        result.graphIdentity = graphIdentity_;
        result.graphRevision = revision_;
        std::vector<std::size_t> enabled;
        for (std::size_t index = 0; index < passes_.size(); ++index) {
            if (passes_[index].descriptor.enabled) enabled.push_back(index);
        }

        std::vector<std::vector<std::size_t>> edges(passes_.size());
        std::vector<std::size_t> indegree(passes_.size(), 0);
        std::unordered_map<std::uint32_t, std::size_t> lastWriter;
        std::unordered_map<std::uint32_t, std::vector<std::size_t>> readersSinceWrite;

        const auto addEdge = [&edges, &indegree](const std::size_t from, const std::size_t to) {
            if (from == to) return;
            const auto& outgoing = edges[from];
            if (std::find(outgoing.begin(), outgoing.end(), to) != outgoing.end()) return;
            edges[from].push_back(to);
            ++indegree[to];
        };

        for (const auto passIndex : enabled) {
            const auto& descriptor = passes_[passIndex].descriptor;
            for (const auto handle : descriptor.reads) {
                const auto* resourceDescriptor = resource(handle);
                if (!resourceDescriptor) return failure("pass reads an unknown resource");
                const auto writer = lastWriter.find(handle.id);
                if (writer != lastWriter.end()) {
                    addEdge(writer->second, passIndex);
                } else if (resourceDescriptor->lifetime == RenderResourceLifetime::Transient) {
                    return failure("transient resource is read before it is written");
                }
                readersSinceWrite[handle.id].push_back(passIndex);
            }
            for (const auto handle : descriptor.writes) {
                if (!resource(handle)) return failure("pass writes an unknown resource");
                const auto writer = lastWriter.find(handle.id);
                if (writer != lastWriter.end()) addEdge(writer->second, passIndex);
                const auto readers = readersSinceWrite.find(handle.id);
                if (readers != readersSinceWrite.end()) {
                    for (const auto reader : readers->second) addEdge(reader, passIndex);
                    readers->second.clear();
                }
                lastWriter[handle.id] = passIndex;
            }
        }

        std::queue<std::size_t> ready;
        // Level of each pass: 1 + max level of its predecessors. Propagated
        // while the Kahn order is produced; a target is relaxed only by
        // already-popped predecessors, so its level is final when queued.
        std::vector<std::size_t> passLevel(passes_.size(), 0);
        for (const auto index : enabled) if (indegree[index] == 0) ready.push(index);
        while (!ready.empty()) {
            const auto index = ready.front();
            ready.pop();
            result.passOrder.push_back(passes_[index].handle);
            for (const auto target : edges[index]) {
                if (passLevel[target] < passLevel[index] + 1) {
                    passLevel[target] = passLevel[index] + 1;
                }
                if (--indegree[target] == 0) ready.push(target);
            }
        }
        if (result.passOrder.size() != enabled.size()) return failure("render graph contains a cycle");

        // Group the topological order into concurrently-runnable levels.
        std::size_t maxLevel = 0;
        for (const auto index : enabled) {
            if (passLevel[index] > maxLevel) maxLevel = passLevel[index];
        }
        result.executionLevels.resize(enabled.empty() ? 0 : maxLevel + 1);
        for (const auto handle : result.passOrder) {
            const auto index = static_cast<std::size_t>(handle.id - 1);
            result.executionLevels[passLevel[index]].push_back(handle);
            result.passOrderLevels.push_back(passLevel[index]);
        }

        struct ResourceUseRange {
            std::size_t firstPass = std::numeric_limits<std::size_t>::max();
            std::size_t lastPass = 0;
            std::size_t firstLevel = std::numeric_limits<std::size_t>::max();
            std::size_t lastLevel = 0;
        };
        std::vector<ResourceUseRange> resourceUseRanges(resources_.size());
        for (std::size_t order = 0; order < result.passOrder.size(); ++order) {
            const auto* descriptor = pass(result.passOrder[order]);
            const auto recordUse = [&](const RenderResourceHandle handle) {
                const auto index = static_cast<std::size_t>(handle.id - 1);
                auto& range = resourceUseRanges[index];
                range.firstPass = std::min(range.firstPass, order);
                range.lastPass = order;
                range.firstLevel = std::min(range.firstLevel, result.passOrderLevels[order]);
                range.lastLevel = std::max(range.lastLevel, result.passOrderLevels[order]);
            };
            for (const auto handle : descriptor->reads) recordUse(handle);
            for (const auto handle : descriptor->writes) recordUse(handle);
        }
        for (std::size_t index = 0; index < resources_.size(); ++index) {
            const auto& range = resourceUseRanges[index];
            if (range.firstPass != std::numeric_limits<std::size_t>::max()) {
                result.lifetimes.push_back({resources_[index].handle,
                                            range.firstPass, range.lastPass});
            }
        }
        // Assign reusable physical slots to non-overlapping transient
        // resources. External and persistent resources retain dedicated slots.
        std::sort(result.lifetimes.begin(), result.lifetimes.end(),
                  [this](const auto& lhs, const auto& rhs) {
                      return lhs.firstPass < rhs.firstPass;
                  });
        struct Slot {
            std::size_t lastExecutionLevel = 0;
            bool occupied = false;
            bool reusable = false;
            RenderResourceKind kind = RenderResourceKind::Texture;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t depth = 1;
            std::uint32_t format = 0;
        };
        std::vector<Slot> slots;
        for (auto& lifetime : result.lifetimes) {
            const auto* descriptor = resource(lifetime.resource);
            const auto& levelRange = resourceUseRanges[lifetime.resource.id - 1];
            const std::size_t firstLevel = levelRange.firstLevel;
            const std::size_t lastLevel = levelRange.lastLevel;
            // Unknown texture formats cannot be proven compatible for aliasing.
            // Keep them in dedicated slots until the caller supplies a format.
            const bool reusable = descriptor &&
                descriptor->lifetime == RenderResourceLifetime::Transient &&
                (descriptor->kind != RenderResourceKind::Texture ||
                 descriptor->format != 0);
            if (!reusable) {
                lifetime.allocationSlot = slots.size();
                slots.push_back({
                    lastLevel,
                    true,
                    false,
                    descriptor ? descriptor->kind : RenderResourceKind::Texture,
                    descriptor ? descriptor->width : 0,
                    descriptor ? descriptor->height : 0,
                    descriptor ? descriptor->depth : 1,
                    descriptor ? descriptor->format : 0});
                continue;
            }
            auto slot = std::find_if(slots.begin(), slots.end(),
                [&lifetime, descriptor, firstLevel](const Slot& candidate) {
                    return candidate.occupied && candidate.reusable &&
                           candidate.lastExecutionLevel < firstLevel &&
                           candidate.kind == descriptor->kind &&
                           candidate.width == descriptor->width &&
                           candidate.height == descriptor->height &&
                           candidate.depth == descriptor->depth &&
                           candidate.format == descriptor->format;
                });
            if (slot == slots.end()) {
                lifetime.allocationSlot = slots.size();
                slots.push_back({lastLevel, true, true,
                                 descriptor->kind, descriptor->width,
                                 descriptor->height, descriptor->depth,
                                 descriptor->format});
            } else {
                lifetime.allocationSlot =
                    static_cast<std::size_t>(std::distance(slots.begin(), slot));
                slot->lastExecutionLevel = lastLevel;
            }
        }
        result.allocationSlotCount = slots.size();
        result.allocationSlots.reserve(slots.size());
        for (std::size_t index = 0; index < slots.size(); ++index) {
            const auto& slot = slots[index];
            RenderAllocationSlotDescriptor descriptor;
            descriptor.index = index;
            descriptor.kind = slot.kind;
            descriptor.width = slot.width;
            descriptor.height = slot.height;
            descriptor.depth = slot.depth;
            descriptor.format = slot.format;
            for (const auto& lifetime : result.lifetimes) {
                if (lifetime.allocationSlot != index) continue;
                const auto* resourceDescriptor = resource(lifetime.resource);
                if (resourceDescriptor) {
                    descriptor.byteSize = std::max(
                        descriptor.byteSize, resourceDescriptor->byteSize);
                }
            }
            result.allocationSlots.push_back(descriptor);
        }
        result.valid = true;
        return result;
    }

    RenderGraphDiagnosticSnapshot diagnosticSnapshot(
        const CompiledRenderGraph& compiled,
        const std::uint64_t executionId = 0) const
    {
        RenderGraphDiagnosticSnapshot snapshot;
        snapshot.executionId = executionId;
        const bool stale = compiled.valid &&
            (compiled.graphIdentity != graphIdentity_ ||
             compiled.graphRevision != revision_);
        snapshot.valid = compiled.valid && !stale;
        snapshot.error = stale ? "compiled render graph is stale" : compiled.error;
        if (!stale) snapshot.allocationSlots = compiled.allocationSlots;
        snapshot.passes.reserve(passes_.size());
        snapshot.resources.reserve(resources_.size());

        for (const auto& item : passes_) {
            RenderDiagnosticPassRecord record;
            record.handle = item.handle;
            record.descriptor = item.descriptor;
            if (!item.descriptor.enabled) {
                record.state = RenderDiagnosticPassState::Disabled;
                record.stateReason = "disabled-by-descriptor";
            } else if (stale) {
                record.state = RenderDiagnosticPassState::Blocked;
                record.stateReason = "compiled-graph-stale";
            } else {
                const auto position = std::find(compiled.passOrder.begin(),
                                                compiled.passOrder.end(),
                                                item.handle);
                if (position != compiled.passOrder.end()) {
                    record.state = RenderDiagnosticPassState::Scheduled;
                    record.stateReason = "scheduled-by-compiled-order";
                    record.executionOrder = static_cast<std::size_t>(
                        std::distance(compiled.passOrder.begin(), position));
                    if (record.executionOrder < compiled.passOrderLevels.size()) {
                        record.executionLevel =
                            compiled.passOrderLevels[record.executionOrder];
                    }
                } else {
                    record.stateReason = compiled.valid
                        ? "not-present-in-compiled-order"
                        : "compile-invalid";
                }
            }
            snapshot.passes.push_back(std::move(record));
        }

        for (const auto& item : resources_) {
            RenderDiagnosticResourceRecord record;
            record.handle = item.handle;
            record.descriptor = item.descriptor;
            const auto lifetime = stale ? compiled.lifetimes.end() : std::find_if(
                compiled.lifetimes.begin(), compiled.lifetimes.end(),
                [&item](const RenderResourceLifetimeRange& range) {
                    return range.resource == item.handle;
                });
            if (!stale && lifetime != compiled.lifetimes.end()) {
                record.firstPass = lifetime->firstPass;
                record.lastPass = lifetime->lastPass;
                record.allocationSlot = lifetime->allocationSlot;
                record.used = true;
                const auto remaining = std::numeric_limits<std::uint64_t>::max()
                    - snapshot.estimatedResourceBytes;
                snapshot.estimatedResourceBytes += std::min(item.descriptor.byteSize,
                                                            remaining);
            }
            snapshot.resources.push_back(std::move(record));
        }
        std::unordered_map<std::size_t, std::uint64_t> slotBytes;
        for (const auto& record : snapshot.resources) {
            if (!record.used) continue;
            auto& slotSize = slotBytes[record.allocationSlot];
            slotSize = std::max(slotSize, record.descriptor.byteSize);
        }
        for (const auto& [slot, bytes] : slotBytes) {
            (void)slot;
            const auto remaining = std::numeric_limits<std::uint64_t>::max() -
                                   snapshot.estimatedAliasedResourceBytes;
            snapshot.estimatedAliasedResourceBytes += std::min(bytes, remaining);
        }
        return snapshot;
    }

    RenderGraphDiagnosticSnapshot compileDiagnosticSnapshot(
        const std::uint64_t executionId = 0) const
    {
        return diagnosticSnapshot(compile(), executionId);
    }

    // Execute only the compiled order. Resource allocation and GPU state
    // transitions remain the responsibility of the renderer backend.
    template <typename Executor>
    bool execute(const CompiledRenderGraph& compiled,
                 Executor&& executor,
                 std::string* error = nullptr) const
    {
        if (error) error->clear();
        if (!compiled.valid) {
            if (error) *error = compiled.error;
            return false;
        }
        if (compiled.graphIdentity != graphIdentity_ ||
            compiled.graphRevision != revision_) {
            if (error) *error = "compiled render graph is stale";
            return false;
        }
        const auto descriptorsConflict = [](const RenderPassDescriptor& lhs,
                                            const RenderPassDescriptor& rhs) {
            const auto overlaps = [](const auto& first, const auto& second) {
                for (const auto handle : first) {
                    if (std::find(second.begin(), second.end(), handle) !=
                        second.end()) {
                        return true;
                    }
                }
                return false;
            };
            return overlaps(lhs.writes, rhs.reads) ||
                   overlaps(lhs.writes, rhs.writes) ||
                   overlaps(lhs.reads, rhs.writes);
        };
        const auto validatePassOrder = [&]() {
            const std::size_t enabledPassCount = static_cast<std::size_t>(
                std::count_if(passes_.begin(), passes_.end(),
                    [](const PassItem& item) {
                        return item.descriptor.enabled;
                    }));
            if (compiled.passOrder.size() != enabledPassCount) {
                if (error) *error =
                    "compiled render graph pass order is incomplete";
                return false;
            }
            for (std::size_t index = 0;
                 index < compiled.passOrder.size(); ++index) {
                const auto handle = compiled.passOrder[index];
                const auto* descriptor = pass(handle);
                if (!descriptor || !descriptor->enabled) {
                    if (error) *error =
                        "compiled render graph references an invalid pass";
                    return false;
                }
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (compiled.passOrder[prior] == handle) {
                        if (error) *error =
                            "compiled render graph pass order has duplicates";
                        return false;
                    }
                }
                for (std::size_t later = index + 1;
                     later < compiled.passOrder.size(); ++later) {
                    const auto laterHandle = compiled.passOrder[later];
                    const auto* laterDescriptor = pass(laterHandle);
                    if (laterDescriptor &&
                        descriptorsConflict(*descriptor, *laterDescriptor) &&
                        handle.id > laterHandle.id) {
                        if (error) *error =
                            "compiled render graph pass order violates a resource dependency";
                        return false;
                    }
                }
            }
            return true;
        };
        if (!validatePassOrder()) return false;
        if constexpr (requires { static_cast<bool>(executor); }) {
            if (!static_cast<bool>(executor)) {
                if (error) *error = "render graph executor is empty";
                return false;
            }
        }
        for (const auto handle : compiled.passOrder) {
            const auto* descriptor = pass(handle);
            if (!descriptor || !descriptor->enabled) {
                if (error) *error = "compiled render graph references an invalid pass";
                return false;
            }
            bool succeeded = false;
            try {
                succeeded = executor(RenderGraphExecutionContext{
                    handle, *descriptor, *this, compiled});
            } catch (...) {
                if (error) *error = "render pass executor failed";
                return false;
            }
            if (!succeeded) {
                if (error) *error = "render pass executor failed";
                return false;
            }
        }
        return true;
    }

    // Level-synchronous parallel execution over compiled.executionLevels.
    // Passes inside one level have no dependency path between them and may
    // run concurrently only when each pass explicitly opts in; all other
    // passes run inline in topological order. The launcher is duck-typed and
    // needs only `async(F) -> std::future<bool>` (Core.TaskSystem satisfies
    // it), so this header gains no threading dependency. The executor target
    // must be safe for concurrent calls for opted-in passes. At most launcher.concurrency() tasks
    // (when available), and never more than the fixed local batch, are queued
    // at once. All launched work is joined before failure is returned. If task
    // launch itself fails, execution falls back to serial for the current and
    // remaining passes after the pending batch has completed successfully.
    template <typename Executor, typename Launcher>
    bool executeParallel(const CompiledRenderGraph& compiled,
                         Executor&& executor,
                         Launcher& launcher,
                         std::string* error = nullptr) const
    {
        if (error) error->clear();
        if (!compiled.valid) {
            if (error) *error = compiled.error;
            return false;
        }
        if (compiled.graphIdentity != graphIdentity_ ||
            compiled.graphRevision != revision_) {
            if (error) *error = "compiled render graph is stale";
            return false;
        }
        const auto descriptorsConflict = [](const RenderPassDescriptor& lhs,
                                            const RenderPassDescriptor& rhs) {
            const auto overlaps = [](const auto& first, const auto& second) {
                for (const auto handle : first) {
                    if (std::find(second.begin(), second.end(), handle) !=
                        second.end()) {
                        return true;
                    }
                }
                return false;
            };
            return overlaps(lhs.writes, rhs.reads) ||
                   overlaps(lhs.writes, rhs.writes) ||
                   overlaps(lhs.reads, rhs.writes);
        };
        const auto validatePassOrder = [&]() {
            const std::size_t enabledPassCount = static_cast<std::size_t>(
                std::count_if(passes_.begin(), passes_.end(),
                    [](const PassItem& item) {
                        return item.descriptor.enabled;
                    }));
            if (compiled.passOrder.size() != enabledPassCount) {
                if (error) *error =
                    "compiled render graph pass order is incomplete";
                return false;
            }
            for (std::size_t index = 0;
                 index < compiled.passOrder.size(); ++index) {
                const auto handle = compiled.passOrder[index];
                const auto* descriptor = pass(handle);
                if (!descriptor || !descriptor->enabled) {
                    if (error) *error =
                        "compiled render graph references an invalid pass";
                    return false;
                }
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (compiled.passOrder[prior] == handle) {
                        if (error) *error =
                            "compiled render graph pass order has duplicates";
                        return false;
                    }
                }
                for (std::size_t later = index + 1;
                     later < compiled.passOrder.size(); ++later) {
                    const auto laterHandle = compiled.passOrder[later];
                    const auto* laterDescriptor = pass(laterHandle);
                    if (laterDescriptor &&
                        descriptorsConflict(*descriptor, *laterDescriptor) &&
                        handle.id > laterHandle.id) {
                        if (error) *error =
                            "compiled render graph pass order violates a resource dependency";
                        return false;
                    }
                }
            }
            return true;
        };
        const auto validateExecutionLevels = [&]() {
            if (compiled.passOrderLevels.size() != compiled.passOrder.size()) {
                if (error) *error =
                    "compiled render graph execution levels are incomplete";
                return false;
            }
            std::size_t orderIndex = 0;
            for (std::size_t levelIndex = 0;
                 levelIndex < compiled.executionLevels.size(); ++levelIndex) {
                const auto& level = compiled.executionLevels[levelIndex];
                if (level.empty()) {
                    if (error) *error =
                        "compiled render graph contains an empty execution level";
                    return false;
                }
                for (const auto handle : level) {
                    if (orderIndex >= compiled.passOrder.size() ||
                        compiled.passOrder[orderIndex] != handle ||
                        compiled.passOrderLevels[orderIndex] != levelIndex) {
                        if (error) *error =
                            "compiled render graph execution levels are inconsistent";
                        return false;
                    }
                    ++orderIndex;
                }
                for (std::size_t left = 0; left < level.size(); ++left) {
                    const auto* leftDescriptor = pass(level[left]);
                    for (std::size_t right = left + 1;
                         right < level.size(); ++right) {
                        const auto* rightDescriptor = pass(level[right]);
                        if (leftDescriptor && rightDescriptor &&
                            descriptorsConflict(*leftDescriptor,
                                                *rightDescriptor)) {
                            if (error) *error =
                                "compiled render graph execution level contains dependent passes";
                            return false;
                        }
                    }
                }
            }
            if (orderIndex != compiled.passOrder.size()) {
                if (error) *error =
                    "compiled render graph execution levels are incomplete";
                return false;
            }
            return true;
        };
        if (!validatePassOrder() || !validateExecutionLevels()) {
            return false;
        }
        if constexpr (requires { static_cast<bool>(executor); }) {
            if (!static_cast<bool>(executor)) {
                if (error) *error = "render graph executor is empty";
                return false;
            }
        }
        const auto runOne = [&](const RenderPassHandle handle,
                                std::string* localError) -> bool {
            const auto* descriptor = pass(handle);
            if (!descriptor || !descriptor->enabled) {
                if (localError) {
                    *localError =
                        "compiled render graph references an invalid pass";
                }
                return false;
            }
            try {
                if (!executor(RenderGraphExecutionContext{handle, *descriptor,
                                                          *this, compiled})) {
                    if (localError) *localError = "render pass executor failed";
                    return false;
                }
            } catch (...) {
                if (localError) *localError = "render pass executor failed";
                return false;
            }
            return true;
        };
        bool parallelDispatchAvailable = true;
        for (const auto& level : compiled.executionLevels) {
            if (level.empty()) {
                continue;
            }
            if (level.size() == 1) {
                if (!runOne(level.front(), error)) return false;
                continue;
            }
            constexpr std::size_t kMaxQueuedPasses = 64;
            // An unknown launcher may create one thread per async() call.
            // Keep execution serial unless it reports a bounded worker count.
            std::size_t concurrency = 1;
            if constexpr (requires { launcher.concurrency(); }) {
                const auto reportedConcurrency = launcher.concurrency();
                concurrency = reportedConcurrency > 0
                    ? std::min<std::size_t>(
                          kMaxQueuedPasses,
                          static_cast<std::size_t>(reportedConcurrency))
                    : 1;
            }

            std::array<std::future<bool>, kMaxQueuedPasses> pending;
            std::size_t pendingCount = 0;
            const auto joinPending = [&]() {
                bool succeeded = true;
                for (std::size_t index = 0; index < pendingCount; ++index) {
                    try {
                        succeeded = pending[index].get() && succeeded;
                    } catch (...) {
                        succeeded = false;
                    }
                }
                pendingCount = 0;
                if (!succeeded && error) {
                    *error = "render pass executor failed";
                }
                return succeeded;
            };

            for (const auto handle : level) {
                const auto* descriptor = pass(handle);
                if (!descriptor || !descriptor->enabled) {
                    if (!joinPending()) return false;
                    if (error) {
                        *error = "compiled render graph references an invalid pass";
                    }
                    return false;
                }

                if (!descriptor->parallelSafe || concurrency == 1 ||
                    !parallelDispatchAvailable) {
                    if (!joinPending() || !runOne(handle, error)) return false;
                    continue;
                }

                try {
                    auto future = launcher.async([&, handle] {
                        return runOne(handle, nullptr);
                    });
                    pending[pendingCount++] = std::move(future);
                } catch (...) {
                    if (!joinPending()) return false;
                    parallelDispatchAvailable = false;
                    if (!runOne(handle, error)) return false;
                    continue;
                }
                if (pendingCount == concurrency && !joinPending()) {
                    return false;
                }
            }
            if (!joinPending()) {
                return false;
            }
        }
        return true;
    }

    void clear()
    {
        resources_.clear();
        passes_.clear();
        nextResourceId_ = 1;
        nextPassId_ = 1;
        markMutated();
    }

private:
    struct ResourceItem { RenderResourceHandle handle; RenderResourceDescriptor descriptor; };
    struct PassItem { RenderPassHandle handle; RenderPassDescriptor descriptor; };

    static CompiledRenderGraph failure(std::string error)
    {
        CompiledRenderGraph result;
        result.error = std::move(error);
        return result;
    }

    bool validateCompiledPassOrder(const CompiledRenderGraph& compiled,
                                   std::string* error) const
    {
        const std::size_t enabledPassCount = static_cast<std::size_t>(
            std::count_if(passes_.begin(), passes_.end(), [](const PassItem& item) {
                return item.descriptor.enabled;
            }));
        if (compiled.passOrder.size() != enabledPassCount) {
            if (error) *error = "compiled render graph pass order is incomplete";
            return false;
        }
        for (std::size_t index = 0; index < compiled.passOrder.size(); ++index) {
            const auto handle = compiled.passOrder[index];
            const auto* descriptor = pass(handle);
            if (!descriptor || !descriptor->enabled) {
                if (error) {
                    *error = "compiled render graph references an invalid pass";
                }
                return false;
            }
            for (std::size_t prior = 0; prior < index; ++prior) {
                if (compiled.passOrder[prior] == handle) {
                    if (error) *error = "compiled render graph pass order has duplicates";
                    return false;
                }
            }
            for (std::size_t later = index + 1;
                 later < compiled.passOrder.size(); ++later) {
                const auto laterHandle = compiled.passOrder[later];
                const auto* laterDescriptor = pass(laterHandle);
                if (!laterDescriptor ||
                    !passDescriptorsConflict(*descriptor, *laterDescriptor)) {
                    continue;
                }
                if (handle.id > laterHandle.id) {
                    if (error) {
                        *error = "compiled render graph pass order violates a resource dependency";
                    }
                    return false;
                }
            }
        }
        return true;
    }

    bool validateCompiledExecutionLevels(
        const CompiledRenderGraph& compiled, std::string* error) const
    {
        if (compiled.passOrderLevels.size() != compiled.passOrder.size()) {
            if (error) *error = "compiled render graph execution levels are incomplete";
            return false;
        }
        std::size_t orderIndex = 0;
        for (std::size_t levelIndex = 0;
             levelIndex < compiled.executionLevels.size(); ++levelIndex) {
            const auto& level = compiled.executionLevels[levelIndex];
            if (level.empty()) {
                if (error) *error = "compiled render graph contains an empty execution level";
                return false;
            }
            for (const auto handle : level) {
                if (orderIndex >= compiled.passOrder.size() ||
                    compiled.passOrder[orderIndex] != handle ||
                    compiled.passOrderLevels[orderIndex] != levelIndex) {
                    if (error) *error = "compiled render graph execution levels are inconsistent";
                    return false;
                }
                ++orderIndex;
            }
            for (std::size_t left = 0; left < level.size(); ++left) {
                const auto* leftDescriptor = pass(level[left]);
                for (std::size_t right = left + 1;
                     right < level.size(); ++right) {
                    const auto* rightDescriptor = pass(level[right]);
                    if (leftDescriptor && rightDescriptor &&
                        passDescriptorsConflict(*leftDescriptor,
                                               *rightDescriptor)) {
                        if (error) {
                            *error = "compiled render graph execution level contains dependent passes";
                        }
                        return false;
                    }
                }
            }
        }
        if (orderIndex != compiled.passOrder.size()) {
            if (error) *error = "compiled render graph execution levels are incomplete";
            return false;
        }
        return true;
    }

    static bool passDescriptorsConflict(
        const RenderPassDescriptor& lhs,
        const RenderPassDescriptor& rhs) noexcept
    {
        const auto overlaps = [](const auto& first, const auto& second) {
            for (const auto handle : first) {
                if (std::find(second.begin(), second.end(), handle) !=
                    second.end()) {
                    return true;
                }
            }
            return false;
        };
        return overlaps(lhs.writes, rhs.reads) ||
               overlaps(lhs.writes, rhs.writes) ||
               overlaps(lhs.reads, rhs.writes);
    }

    static std::uint64_t nextGraphIdentity() noexcept
    {
        static std::atomic<std::uint64_t> nextIdentity{1};
        std::uint64_t identity = nextIdentity.fetch_add(
            1, std::memory_order_relaxed);
        if (identity == 0) {
            identity = nextIdentity.fetch_add(1, std::memory_order_relaxed);
        }
        return identity;
    }

    void markMutated() noexcept
    {
        ++revision_;
        if (revision_ == 0) revision_ = 1;
    }

    std::vector<ResourceItem> resources_;
    std::vector<PassItem> passes_;
    std::uint32_t nextResourceId_ = 1;
    std::uint32_t nextPassId_ = 1;
    std::uint64_t revision_ = 1;
    std::uint64_t graphIdentity_ = nextGraphIdentity();
};

}
