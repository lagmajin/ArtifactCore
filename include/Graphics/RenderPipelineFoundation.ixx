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
import Graphics.TemporalHistory;

export namespace ArtifactCore {

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
