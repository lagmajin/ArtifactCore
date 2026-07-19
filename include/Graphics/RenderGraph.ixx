module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.RenderGraph;

export namespace ArtifactCore {

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
    std::uint32_t format = 0;
    std::uint64_t byteSize = 0;
};

struct RenderPassDescriptor {
    std::string name;
    RenderPassQueue queue = RenderPassQueue::Graphics;
    std::vector<RenderResourceHandle> reads;
    std::vector<RenderResourceHandle> writes;
    bool enabled = true;
};

struct RenderResourceLifetimeRange {
    RenderResourceHandle resource;
    std::size_t firstPass = 0;
    std::size_t lastPass = 0;
};

struct CompiledRenderGraph {
    bool valid = false;
    std::string error;
    std::vector<RenderPassHandle> passOrder;
    std::vector<RenderResourceLifetimeRange> lifetimes;
};

struct RenderDiagnosticResourceRecord {
    RenderResourceHandle handle;
    RenderResourceDescriptor descriptor;
    std::size_t firstPass = 0;
    std::size_t lastPass = 0;
    bool used = false;
};

struct RenderDiagnosticPassRecord {
    RenderPassHandle handle;
    RenderPassDescriptor descriptor;
    RenderDiagnosticPassState state = RenderDiagnosticPassState::Blocked;
    std::size_t executionOrder = 0;
    std::uint64_t gpuDurationUs = 0;
    std::uint64_t gpuSampleExecutionId = 0;
    bool gpuTimingAvailable = false;
};

struct RenderGraphDiagnosticSnapshot {
    std::uint64_t executionId = 0;
    bool valid = false;
    std::string error;
    std::uint64_t estimatedResourceBytes = 0;
    std::vector<RenderDiagnosticPassRecord> passes;
    std::vector<RenderDiagnosticResourceRecord> resources;
};

class LIBRARY_DLL_API RenderGraph {
public:
    RenderResourceHandle addResource(RenderResourceDescriptor descriptor)
    {
        const RenderResourceHandle handle{nextResourceId_++};
        resources_.push_back({handle, std::move(descriptor)});
        return handle;
    }

    RenderPassHandle addPass(RenderPassDescriptor descriptor)
    {
        const RenderPassHandle handle{nextPassId_++};
        passes_.push_back({handle, std::move(descriptor)});
        return handle;
    }

    const RenderResourceDescriptor* resource(const RenderResourceHandle handle) const noexcept
    {
        for (const auto& item : resources_) if (item.handle == handle) return &item.descriptor;
        return nullptr;
    }

    const RenderPassDescriptor* pass(const RenderPassHandle handle) const noexcept
    {
        for (const auto& item : passes_) if (item.handle == handle) return &item.descriptor;
        return nullptr;
    }

    CompiledRenderGraph compile() const
    {
        CompiledRenderGraph result;
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
        for (const auto index : enabled) if (indegree[index] == 0) ready.push(index);
        while (!ready.empty()) {
            const auto index = ready.front();
            ready.pop();
            result.passOrder.push_back(passes_[index].handle);
            for (const auto target : edges[index]) if (--indegree[target] == 0) ready.push(target);
        }
        if (result.passOrder.size() != enabled.size()) return failure("render graph contains a cycle");

        for (const auto& resourceItem : resources_) {
            std::size_t first = std::numeric_limits<std::size_t>::max();
            std::size_t last = 0;
            for (std::size_t order = 0; order < result.passOrder.size(); ++order) {
                const auto* descriptor = pass(result.passOrder[order]);
                const auto used = [&resourceItem](const auto& handles) {
                    return std::find(handles.begin(), handles.end(), resourceItem.handle) != handles.end();
                };
                if (used(descriptor->reads) || used(descriptor->writes)) {
                    first = std::min(first, order);
                    last = order;
                }
            }
            if (first != std::numeric_limits<std::size_t>::max()) {
                result.lifetimes.push_back({resourceItem.handle, first, last});
            }
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
        snapshot.valid = compiled.valid;
        snapshot.error = compiled.error;
        snapshot.passes.reserve(passes_.size());
        snapshot.resources.reserve(resources_.size());

        for (const auto& item : passes_) {
            RenderDiagnosticPassRecord record;
            record.handle = item.handle;
            record.descriptor = item.descriptor;
            if (!item.descriptor.enabled) {
                record.state = RenderDiagnosticPassState::Disabled;
            } else {
                const auto position = std::find(compiled.passOrder.begin(),
                                                compiled.passOrder.end(),
                                                item.handle);
                if (position != compiled.passOrder.end()) {
                    record.state = RenderDiagnosticPassState::Scheduled;
                    record.executionOrder = static_cast<std::size_t>(
                        std::distance(compiled.passOrder.begin(), position));
                }
            }
            snapshot.passes.push_back(std::move(record));
        }

        for (const auto& item : resources_) {
            RenderDiagnosticResourceRecord record;
            record.handle = item.handle;
            record.descriptor = item.descriptor;
            const auto lifetime = std::find_if(
                compiled.lifetimes.begin(),
                compiled.lifetimes.end(),
                [&item](const RenderResourceLifetimeRange& range) {
                    return range.resource == item.handle;
                });
            if (lifetime != compiled.lifetimes.end()) {
                record.firstPass = lifetime->firstPass;
                record.lastPass = lifetime->lastPass;
                record.used = true;
                const auto remaining = std::numeric_limits<std::uint64_t>::max()
                    - snapshot.estimatedResourceBytes;
                snapshot.estimatedResourceBytes += std::min(item.descriptor.byteSize,
                                                            remaining);
            }
            snapshot.resources.push_back(std::move(record));
        }
        return snapshot;
    }

    RenderGraphDiagnosticSnapshot compileDiagnosticSnapshot(
        const std::uint64_t executionId = 0) const
    {
        return diagnosticSnapshot(compile(), executionId);
    }

    void clear()
    {
        resources_.clear();
        passes_.clear();
        nextResourceId_ = 1;
        nextPassId_ = 1;
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

    std::vector<ResourceItem> resources_;
    std::vector<PassItem> passes_;
    std::uint32_t nextResourceId_ = 1;
    std::uint32_t nextPassId_ = 1;
};

}
