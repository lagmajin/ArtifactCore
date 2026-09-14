module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include "../Define/DllExportMacro.hpp"

export module Graphics.RenderPipelineFoundation;

import Graphics.GIResources;
import Graphics.PointwiseFusion;
import Graphics.RenderGraph;
import Graphics.SurfaceColorContract;
import Graphics.TemporalHistory;

export namespace ArtifactCore {

enum class RenderBackendKind : std::uint8_t {
    Auto,
    DiligentGPU,
    Software,
};

enum class RenderQuality : std::uint8_t {
    Draft,
    Preview,
    Final,
};

enum class RenderCapability : std::uint32_t {
    None = 0,
    Raster = 1u << 0u,
    Compute = 1u << 1u,
    Float16Targets = 1u << 2u,
    Float32Targets = 1u << 3u,
    HDR = 1u << 4u,
    TemporalHistory = 1u << 5u,
};

using RenderCapabilityMask = std::uint32_t;

constexpr RenderCapabilityMask capabilityMask(
    const RenderCapability capability) noexcept
{
    return static_cast<RenderCapabilityMask>(capability);
}

constexpr RenderCapabilityMask operator|(
    const RenderCapability lhs, const RenderCapability rhs) noexcept
{
    return capabilityMask(lhs) | capabilityMask(rhs);
}

constexpr RenderCapabilityMask operator|(
    const RenderCapabilityMask lhs, const RenderCapability rhs) noexcept
{
    return lhs | capabilityMask(rhs);
}

enum class RenderFallbackReason : std::uint8_t {
    None,
    InvalidSnapshot,
    ColorContractIncomplete,
    ResolutionExceeded,
    MissingRaster,
    MissingCompute,
    MissingFloat16Targets,
    MissingFloat32Targets,
    MissingHDR,
    MissingTemporalHistory,
};

struct RenderCacheKey {
    static constexpr std::uint32_t SchemaVersion = 1;

    std::uint32_t schemaVersion = SchemaVersion;
    std::uint64_t compositionId = 0;
    std::uint64_t viewId = 0;
    std::uint64_t sceneRevision = 0;
    std::uint64_t renderIndexGeneration = 0;
    std::uint64_t settingsRevision = 0;
    std::int64_t frameIndex = 0;
    std::int64_t frameRateNumerator = 30;
    std::int64_t frameRateDenominator = 1;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    RenderQuality quality = RenderQuality::Preview;
    RenderBackendKind backend = RenderBackendKind::Auto;
    SurfacePixelStorage storage = SurfacePixelStorage::Unknown;
    SurfaceChannelOrder channelOrder = SurfaceChannelOrder::Unknown;
    SurfaceColorPrimaries primaries = SurfaceColorPrimaries::Unknown;
    TransferFunction transfer = TransferFunction::Linear;
    SurfaceAlphaMode alphaMode = SurfaceAlphaMode::Unknown;
    SurfaceColorRange range = SurfaceColorRange::Unknown;
    bool transferKnown = false;
    bool requiresCompute = false;
    bool requiresHDR = false;
    bool usesTemporalHistory = false;

    friend constexpr bool operator==(const RenderCacheKey&, const RenderCacheKey&)
        noexcept = default;

    std::uint64_t stableHash() const noexcept
    {
        std::uint64_t hash = 1469598103934665603ull;
        const auto combine = [&hash](const std::uint64_t value) noexcept {
            hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
        };
        combine(schemaVersion);
        combine(compositionId);
        combine(viewId);
        combine(sceneRevision);
        combine(renderIndexGeneration);
        combine(settingsRevision);
        combine(static_cast<std::uint64_t>(frameIndex));
        combine(static_cast<std::uint64_t>(frameRateNumerator));
        combine(static_cast<std::uint64_t>(frameRateDenominator));
        combine(width);
        combine(height);
        combine(static_cast<std::uint8_t>(quality));
        combine(static_cast<std::uint8_t>(backend));
        combine(static_cast<std::uint8_t>(storage));
        combine(static_cast<std::uint8_t>(channelOrder));
        combine(static_cast<std::uint8_t>(primaries));
        combine(static_cast<std::uint8_t>(transfer));
        combine(static_cast<std::uint8_t>(alphaMode));
        combine(static_cast<std::uint8_t>(range));
        combine(transferKnown ? 1u : 0u);
        combine(requiresCompute ? 1u : 0u);
        combine(requiresHDR ? 1u : 0u);
        combine(usesTemporalHistory ? 1u : 0u);
        return hash;
    }
};

struct RenderCacheKeyHash {
    std::size_t operator()(const RenderCacheKey& key) const noexcept
    {
        return static_cast<std::size_t>(key.stableHash());
    }
};

struct RenderInputSnapshot {
    static constexpr std::uint32_t SchemaVersion = RenderCacheKey::SchemaVersion;

    std::uint32_t schemaVersion = SchemaVersion;
    std::uint64_t compositionId = 0;
    std::uint64_t viewId = 0;
    std::uint64_t sceneRevision = 0;
    std::uint64_t renderIndexGeneration = 0;
    std::uint64_t settingsRevision = 0;
    std::int64_t frameIndex = 0;
    std::int64_t frameRateNumerator = 30;
    std::int64_t frameRateDenominator = 1;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    RenderQuality quality = RenderQuality::Preview;
    RenderBackendKind requestedBackend = RenderBackendKind::Auto;
    SurfaceColorDescriptor color =
        SurfaceColorDescriptor::canonicalLinearPremultiplied();
    bool requiresCompute = false;
    bool requiresHDR = false;
    bool usesTemporalHistory = false;

    bool hasValidFrameContract() const noexcept
    {
        return schemaVersion == SchemaVersion && compositionId != 0 &&
               viewId != 0 && width != 0 && height != 0 &&
               frameRateNumerator > 0 && frameRateDenominator > 0;
    }

    bool isValid() const noexcept
    {
        return hasValidFrameContract() && color.isFullySpecified();
    }

    RenderCapabilityMask requiredCapabilities() const noexcept
    {
        RenderCapabilityMask result = capabilityMask(RenderCapability::Raster);
        if (requiresCompute) result |= capabilityMask(RenderCapability::Compute);
        if (color.storage == SurfacePixelStorage::RGBA16Float) {
            result |= capabilityMask(RenderCapability::Float16Targets);
        }
        if (color.storage == SurfacePixelStorage::RGBA32Float) {
            result |= capabilityMask(RenderCapability::Float32Targets);
        }
        if (requiresHDR) result |= capabilityMask(RenderCapability::HDR);
        if (usesTemporalHistory) {
            result |= capabilityMask(RenderCapability::TemporalHistory);
        }
        return result;
    }

    RenderCacheKey cacheKey(
        const RenderBackendKind resolvedBackend = RenderBackendKind::Auto) const
        noexcept
    {
        RenderCacheKey result;
        result.schemaVersion = schemaVersion;
        result.compositionId = compositionId;
        result.viewId = viewId;
        result.sceneRevision = sceneRevision;
        result.renderIndexGeneration = renderIndexGeneration;
        result.settingsRevision = settingsRevision;
        result.frameIndex = frameIndex;
        result.frameRateNumerator = frameRateNumerator;
        result.frameRateDenominator = frameRateDenominator;
        result.width = width;
        result.height = height;
        result.quality = quality;
        result.backend = resolvedBackend == RenderBackendKind::Auto
            ? requestedBackend : resolvedBackend;
        result.storage = color.storage;
        result.channelOrder = color.channelOrder;
        result.primaries = color.primaries;
        result.transfer = color.transfer;
        result.alphaMode = color.alphaMode;
        result.range = color.range;
        result.transferKnown = color.transferKnown;
        result.requiresCompute = requiresCompute;
        result.requiresHDR = requiresHDR;
        result.usesTemporalHistory = usesTemporalHistory;
        return result;
    }
};

struct RenderBackendCapabilities {
    RenderBackendKind backend = RenderBackendKind::Auto;
    RenderCapabilityMask supported = 0;
    std::uint32_t maxTextureDimension = 0;

    bool supports(const RenderCapability capability) const noexcept
    {
        return (supported & capabilityMask(capability)) != 0;
    }

    RenderFallbackReason failureReason(
        const RenderInputSnapshot& snapshot) const noexcept
    {
        if (!snapshot.hasValidFrameContract()) {
            return RenderFallbackReason::InvalidSnapshot;
        }
        if (!snapshot.color.isFullySpecified()) {
            return RenderFallbackReason::ColorContractIncomplete;
        }
        if (maxTextureDimension != 0 &&
            (snapshot.width > maxTextureDimension ||
             snapshot.height > maxTextureDimension)) {
            return RenderFallbackReason::ResolutionExceeded;
        }
        if (!supports(RenderCapability::Raster)) {
            return RenderFallbackReason::MissingRaster;
        }
        if (snapshot.requiresCompute && !supports(RenderCapability::Compute)) {
            return RenderFallbackReason::MissingCompute;
        }
        if (snapshot.color.storage == SurfacePixelStorage::RGBA16Float &&
            !supports(RenderCapability::Float16Targets)) {
            return RenderFallbackReason::MissingFloat16Targets;
        }
        if (snapshot.color.storage == SurfacePixelStorage::RGBA32Float &&
            !supports(RenderCapability::Float32Targets)) {
            return RenderFallbackReason::MissingFloat32Targets;
        }
        if (snapshot.requiresHDR && !supports(RenderCapability::HDR)) {
            return RenderFallbackReason::MissingHDR;
        }
        if (snapshot.usesTemporalHistory &&
            !supports(RenderCapability::TemporalHistory)) {
            return RenderFallbackReason::MissingTemporalHistory;
        }
        return RenderFallbackReason::None;
    }

    bool canRender(const RenderInputSnapshot& snapshot) const noexcept
    {
        return failureReason(snapshot) == RenderFallbackReason::None;
    }
};

struct RenderBackendSelection {
    RenderBackendKind requested = RenderBackendKind::Auto;
    RenderBackendKind selected = RenderBackendKind::Auto;
    RenderFallbackReason fallbackReason = RenderFallbackReason::None;

    bool resolved() const noexcept { return selected != RenderBackendKind::Auto; }
    bool usedFallback() const noexcept {
        return resolved() && requested != RenderBackendKind::Auto &&
               requested != selected;
    }

    RenderCacheKey cacheKey(const RenderInputSnapshot& snapshot) const noexcept
    {
        return snapshot.cacheKey(selected);
    }
};

class LIBRARY_DLL_API RenderPipelineContract {
public:
    static RenderBackendSelection selectBackend(
        const RenderInputSnapshot& snapshot,
        const RenderBackendCapabilities& gpu,
        const RenderBackendCapabilities& software) noexcept
    {
        RenderBackendSelection result;
        result.requested = snapshot.requestedBackend;
        if (!snapshot.isValid()) {
            result.fallbackReason = RenderFallbackReason::InvalidSnapshot;
            return result;
        }

        const auto gpuFailure = gpu.failureReason(snapshot);
        const auto softwareFailure = software.failureReason(snapshot);
        if (snapshot.requestedBackend == RenderBackendKind::Software) {
            if (softwareFailure == RenderFallbackReason::None) {
                result.selected = RenderBackendKind::Software;
            } else {
                result.fallbackReason = softwareFailure;
            }
            return result;
        }

        if (gpuFailure == RenderFallbackReason::None) {
            result.selected = RenderBackendKind::DiligentGPU;
            return result;
        }
        if (softwareFailure == RenderFallbackReason::None) {
            result.selected = RenderBackendKind::Software;
            result.fallbackReason = gpuFailure;
            return result;
        }
        result.fallbackReason = gpuFailure;
        return result;
    }
};

struct GIRenderGraphBuildResult {
    static constexpr std::size_t ResourceCount = 6;
    std::array<RenderResourceHandle, ResourceCount> resources{};
    std::size_t passCount = 0;

    RenderResourceHandle resource(const GIResourceKind kind) const noexcept
    {
        return resources[static_cast<std::size_t>(kind)];
    }
};

class LIBRARY_DLL_API GIRenderGraphAdapter {
public:
    static GIRenderGraphBuildResult append(RenderGraph& graph,
                                           const GIFrameContext& context)
    {
        GIRenderGraphBuildResult result;
        if (context.plan().empty() || !context.resources().valid()) return result;

        for (std::size_t index = 0; index < result.ResourceCount; ++index) {
            const auto kind = static_cast<GIResourceKind>(index);
            const bool fullResolution = kind == GIResourceKind::Depth ||
                                        kind == GIResourceKind::Motion ||
                                        kind == GIResourceKind::DirectLighting;
            RenderResourceDescriptor descriptor;
            descriptor.name = resourceName(kind);
            descriptor.kind = RenderResourceKind::Texture;
            descriptor.lifetime = resourceLifetime(kind);
            descriptor.width = fullResolution ? context.resources().width() : context.workingWidth();
            descriptor.height = fullResolution ? context.resources().height() : context.workingHeight();
            descriptor.depth = 1;
            descriptor.format = context.resources().descriptor(kind).channels;
            result.resources[index] = graph.addResource(std::move(descriptor));
        }

        for (const auto& giPass : context.plan().passes()) {
            RenderPassDescriptor pass;
            pass.name = passName(giPass.kind);
            pass.queue = RenderPassQueue::Compute;
            pass.reads.push_back(result.resource(giPass.input));
            pass.writes.push_back(result.resource(giPass.output));
            graph.addPass(std::move(pass));
            ++result.passCount;
        }
        return result;
    }

private:
    static RenderResourceLifetime resourceLifetime(const GIResourceKind kind) noexcept
    {
        switch (kind) {
            case GIResourceKind::Depth:
            case GIResourceKind::Motion:
            case GIResourceKind::DirectLighting:
                return RenderResourceLifetime::External;
            case GIResourceKind::History:
                return RenderResourceLifetime::Persistent;
            default:
                return RenderResourceLifetime::Transient;
        }
    }

    static std::string resourceName(const GIResourceKind kind)
    {
        switch (kind) {
            case GIResourceKind::Depth: return "GI.Depth";
            case GIResourceKind::Normal: return "GI.Normal";
            case GIResourceKind::Motion: return "GI.Motion";
            case GIResourceKind::DirectLighting: return "GI.DirectLighting";
            case GIResourceKind::IndirectLighting: return "GI.IndirectLighting";
            case GIResourceKind::History: return "GI.History";
        }
        return "GI.Unknown";
    }

    static std::string passName(const GIPassKind kind)
    {
        switch (kind) {
            case GIPassKind::Reconstruct: return "GI.Reconstruct";
            case GIPassKind::ScreenSpaceGather: return "GI.ScreenSpaceGather";
            case GIPassKind::DepthPyramid: return "GI.DepthPyramid";
            case GIPassKind::BilateralDenoise: return "GI.BilateralDenoise";
            case GIPassKind::TemporalResolve: return "GI.TemporalResolve";
            case GIPassKind::Composite: return "GI.Composite";
        }
        return "GI.Unknown";
    }
};

class LIBRARY_DLL_API PointwiseRenderGraphAdapter {
public:
    static RenderPassHandle append(RenderGraph& renderGraph,
                                   const PointwiseFusionGraph& fusionGraph,
                                   const RenderResourceHandle input,
                                   const RenderResourceHandle output,
                                   std::string passName = "Pointwise.Fused")
    {
        if (!fusionGraph.isValid() || fusionGraph.operationCount() == 0 ||
            !renderGraph.resource(input) || !renderGraph.resource(output)) {
            return {};
        }
        RenderPassDescriptor pass;
        pass.name = std::move(passName);
        pass.queue = RenderPassQueue::Compute;
        pass.reads.push_back(input);
        pass.writes.push_back(output);
        return renderGraph.addPass(std::move(pass));
    }
};

class LIBRARY_DLL_API GITemporalHistoryAdapter {
public:
    static TemporalHistoryKey key(const std::uint64_t compositionId,
                                  const std::uint64_t viewId,
                                  const GIFrameContext& context)
    {
        TemporalHistoryKey result;
        result.compositionId = compositionId;
        result.viewId = viewId;
        result.passId = "GI.TemporalResolve";
        result.width = context.workingWidth();
        result.height = context.workingHeight();
        result.quality = static_cast<std::uint32_t>(context.settings().mode);
        return result;
    }

    static bool canReuse(const TemporalHistoryRegistry& registry,
                         const std::uint64_t compositionId,
                         const std::uint64_t viewId,
                         const GIFrameContext& context,
                         const std::int64_t frame)
    {
        return context.historyReady() &&
               registry.isReusable(key(compositionId, viewId, context), frame);
    }
};

}
