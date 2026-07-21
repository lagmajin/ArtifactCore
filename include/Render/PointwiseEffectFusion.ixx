module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

export module Artifact.Render.PointwiseEffectFusion;

export namespace ArtifactCore {

enum class PointwiseNodeKind : std::uint8_t {
    Exposure,
    Gamma,
    Contrast,
    Levels,
    Saturation,
    Tint,
    HueRotate,
    ColorTemperature,
    Clamp,
    Lut3D,
    Blend,
    AlphaConvert,
    Posterize,
    Threshold,
    NeighborhoodBlur,
    Neighborhood,
    Temporal,
    CpuBoundary,
};

enum class EffectExecutionDomain : std::uint8_t {
    Pointwise,
    Neighborhood,
    Temporal,
    CpuBoundary,
};

enum class PointwiseAlphaMode : std::uint8_t {
    Straight,
    Premultiplied,
};

enum class PointwiseBlendMode : std::uint8_t {
    Normal,
    Add,
    Multiply,
    Screen,
};

struct PointwiseEffectNode {
    PointwiseNodeKind kind = PointwiseNodeKind::Exposure;
    std::uint32_t parameterIndex = 0;
    PointwiseBlendMode blendMode = PointwiseBlendMode::Normal;
    bool enabled = true;
    bool requiresBackground = false;
    bool staticSpecialization = false;
};

struct PointwiseFusionSegment {
    std::size_t firstNode = 0;
    std::size_t nodeCount = 0;
    PointwiseAlphaMode inputAlpha = PointwiseAlphaMode::Premultiplied;
    PointwiseAlphaMode outputAlpha = PointwiseAlphaMode::Premultiplied;
    bool requiresBackground = false;
    bool requiresLut = false;
    bool isFused = false;
    std::string fallbackReason;
};

struct PointwiseCompileKey {
    std::string backend;
    std::string targetFormat;
    PointwiseAlphaMode alphaMode = PointwiseAlphaMode::Premultiplied;
    std::vector<PointwiseNodeKind> orderedNodeKinds;
    std::vector<bool> staticSpecializations;
    bool requiresBackground = false;
    bool requiresLut = false;

    std::string toString() const {
        std::ostringstream key;
        key << backend << '|' << targetFormat << '|'
            << static_cast<unsigned>(alphaMode) << '|'
            << (requiresBackground ? 'B' : '-')
            << (requiresLut ? 'L' : '-')
            << (requiresHistory ? 'H' : '-');
        for (std::size_t i = 0; i < orderedNodeKinds.size(); ++i) {
            key << '|' << static_cast<unsigned>(orderedNodeKinds[i])
                << (i < staticSpecializations.size() && staticSpecializations[i] ? 'S' : 'D');
        }
        return key.str();
    }
};

struct PointwiseGeneratedShader {
    PointwiseCompileKey key;
    std::string entryPoint = "PointwiseFusionCS";
    std::string source;
    std::string diagnosticName;
};

struct PointwiseComputePlan {
    PointwiseGeneratedShader shader;
    std::string sourceResource = "SourceTexture";
    std::string outputResource = "OutputTexture";
    std::string backgroundResource = "BackgroundTexture";
    std::string lutResource = "LutTexture";
    std::string historyResource = "HistoryTexture";
    std::string parameterBuffer = "PointwiseParameters";
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t groupSizeX = 16;
    std::uint32_t groupSizeY = 16;

    std::uint32_t dispatchX() const {
        return (width + groupSizeX - 1) / groupSizeX;
    }

    std::uint32_t dispatchY() const {
        return (height + groupSizeY - 1) / groupSizeY;
    }

    bool valid() const {
        return !shader.source.empty() && !shader.entryPoint.empty() &&
               width > 0 && height > 0 && groupSizeX > 0 && groupSizeY > 0;
    }
};

struct EffectDomainSegment {
    std::size_t firstNode = 0;
    std::size_t nodeCount = 0;
    EffectExecutionDomain domain = EffectExecutionDomain::CpuBoundary;
};

struct PointwiseStackValidation {
    bool valid = true;
    std::vector<std::string> errors;
};

class PointwiseEffectStack {
public:
    static constexpr std::size_t kParameterSlotCount = 64;

    std::uint32_t addNode(
        PointwiseNodeKind kind,
        std::uint32_t parameterIndex = 0,
        PointwiseBlendMode blendMode = PointwiseBlendMode::Normal) {
        nodes_.push_back({kind, parameterIndex, blendMode});
        return static_cast<std::uint32_t>(nodes_.size() - 1);
    }

    bool setParameter(std::uint32_t slot, const std::array<float, 4>& value) {
        if (slot >= kParameterSlotCount) {
            return false;
        }
        parameters_[slot] = value;
        return true;
    }

    bool setParameter(std::uint32_t slot, float x, float y = 0.0f,
                      float z = 0.0f, float w = 0.0f) {
        return setParameter(slot, {x, y, z, w});
    }

    const std::vector<PointwiseEffectNode>& nodes() const { return nodes_; }
    const std::array<std::array<float, 4>, kParameterSlotCount>& parameters() const {
        return parameters_;
    }

    std::vector<EffectDomainSegment> domainSegments() const;
    PointwiseStackValidation validate() const;

    std::vector<PointwiseFusionSegment> segments(
        PointwiseAlphaMode initialAlpha = PointwiseAlphaMode::Premultiplied) const;

private:
    std::vector<PointwiseEffectNode> nodes_;
    std::array<std::array<float, 4>, kParameterSlotCount> parameters_{};
};

struct PointwiseFusionValidation {
    bool valid = true;
    std::vector<std::string> errors;
};

struct PointwiseParameterContract {
    std::size_t slotCount = 1;
    std::string semantic;
};

struct PointwiseNodeDescriptor {
    PointwiseParameterContract parameters;
    EffectExecutionDomain domain = EffectExecutionDomain::CpuBoundary;
    bool pointwise = false;
    bool requiresStraightAlpha = false;
    bool requiresBackground = false;
    bool requiresLut = false;
    bool requiresHistory = false;
};

class PointwiseEffectFusion {
public:
    static PointwiseNodeDescriptor descriptor(PointwiseNodeKind kind) {
        switch (kind) {
        case PointwiseNodeKind::Exposure:
        case PointwiseNodeKind::Gamma:
        case PointwiseNodeKind::Contrast:
        case PointwiseNodeKind::Saturation:
        case PointwiseNodeKind::Tint:
        case PointwiseNodeKind::HueRotate:
        case PointwiseNodeKind::ColorTemperature:
        case PointwiseNodeKind::Posterize:
        case PointwiseNodeKind::Threshold:
            return {{1, "node-specific scalar/vector parameters"}, EffectExecutionDomain::Pointwise, true, true, false, false};
        case PointwiseNodeKind::Levels:
            return {{2, "x=min, y=max"}, EffectExecutionDomain::Pointwise, true, true, false, false};
        case PointwiseNodeKind::Clamp:
            return {{2, "x=min, next-slot.x=max"}, EffectExecutionDomain::Pointwise, true, true, false, false};
        case PointwiseNodeKind::Lut3D:
            return {{1, "x=LUT blend amount"}, EffectExecutionDomain::Pointwise, true, true, false, true};
        case PointwiseNodeKind::Blend:
            return {{1, "x=blend opacity"}, EffectExecutionDomain::Pointwise, true, false, true, false};
        case PointwiseNodeKind::AlphaConvert:
            return {{1, "node-specific scalar/vector parameters"}, EffectExecutionDomain::Pointwise, true, false, false, false};
        case PointwiseNodeKind::NeighborhoodBlur:
        case PointwiseNodeKind::Neighborhood:
            return {{1, "node-specific scalar/vector parameters"}, EffectExecutionDomain::Neighborhood, false, false, false, false};
        case PointwiseNodeKind::Temporal:
            return {{1, "node-specific scalar/vector parameters"}, EffectExecutionDomain::Temporal, false, false, false, false};
        case PointwiseNodeKind::CpuBoundary:
            return {{1, "node-specific scalar/vector parameters"}, EffectExecutionDomain::CpuBoundary, false, false, false, false};
        }
        return {};
    }

    static PointwiseParameterContract parameterContract(PointwiseNodeKind kind) {
        return descriptor(kind).parameters;
    }

    static EffectExecutionDomain executionDomain(PointwiseNodeKind kind) {
        return descriptor(kind).domain;
    }

    static bool isPointwise(const PointwiseEffectNode& node) {
        if (!node.enabled) {
            return false;
        }
        return descriptor(node.kind).pointwise;
    }

    static std::vector<PointwiseFusionSegment> segment(
        const std::vector<PointwiseEffectNode>& nodes,
        PointwiseAlphaMode initialAlpha = PointwiseAlphaMode::Premultiplied) {
        std::vector<PointwiseFusionSegment> segments;
        PointwiseAlphaMode alpha = initialAlpha;
        std::size_t index = 0;
        while (index < nodes.size()) {
            const std::size_t start = index;
            PointwiseFusionSegment segment;
            segment.firstNode = start;
            segment.inputAlpha = alpha;
            while (index < nodes.size() && isPointwise(nodes[index])) {
                const auto& node = nodes[index];
                const PointwiseNodeDescriptor metadata = descriptor(node.kind);
                segment.requiresBackground |= metadata.requiresBackground ||
                                              node.requiresBackground;
                segment.requiresLut |= metadata.requiresLut;
                if (node.kind == PointwiseNodeKind::AlphaConvert) {
                    alpha = alpha == PointwiseAlphaMode::Straight
                        ? PointwiseAlphaMode::Premultiplied
                        : PointwiseAlphaMode::Straight;
                }
                ++index;
            }
            if (index > start) {
                segment.nodeCount = index - start;
                segment.outputAlpha = alpha;
                segment.isFused = segment.nodeCount > 1;
                segments.push_back(segment);
            }
            if (index < nodes.size()) {
                PointwiseFusionSegment boundary;
                boundary.firstNode = index;
                boundary.nodeCount = 1;
                boundary.inputAlpha = alpha;
                boundary.outputAlpha = alpha;
                boundary.fallbackReason = boundaryReason(nodes[index].kind);
                segments.push_back(boundary);
                ++index;
            }
        }
        return segments;
    }

    static PointwiseCompileKey makeCompileKey(
        std::string_view backend,
        std::string_view targetFormat,
        const std::vector<PointwiseEffectNode>& nodes,
        const PointwiseFusionSegment& segment) {
        PointwiseCompileKey key;
        key.backend = backend;
        key.targetFormat = targetFormat;
        key.alphaMode = segment.inputAlpha;
        key.requiresBackground = segment.requiresBackground;
        key.requiresLut = segment.requiresLut;
        if (segment.firstNode > nodes.size()) {
            return key;
        }
        const std::size_t end = std::min(nodes.size(), segment.firstNode + segment.nodeCount);
        for (std::size_t i = segment.firstNode; i < end; ++i) {
            key.orderedNodeKinds.push_back(nodes[i].kind);
            key.staticSpecializations.push_back(nodes[i].staticSpecialization);
        }
        return key;
    }

    static PointwiseFusionValidation validateSegment(
        const std::vector<PointwiseEffectNode>& nodes,
        const PointwiseFusionSegment& segment) {
        PointwiseFusionValidation result;
        if (segment.nodeCount == 0 || segment.firstNode >= nodes.size() ||
            segment.nodeCount > nodes.size() - segment.firstNode) {
            result.valid = false;
            result.errors.push_back("segment range is outside the effect stack");
            return result;
        }
        const std::size_t end = segment.firstNode + segment.nodeCount;
        for (std::size_t i = segment.firstNode; i < end; ++i) {
            const auto& node = nodes[i];
            if (!isPointwise(node)) {
                result.valid = false;
                result.errors.push_back("node " + std::to_string(i) + " is not pointwise");
                continue;
            }
            const PointwiseNodeDescriptor metadata = descriptor(node.kind);
            const PointwiseParameterContract contract = metadata.parameters;
            if (node.parameterIndex >= 64 || contract.slotCount > 64 - node.parameterIndex) {
                result.valid = false;
                result.errors.push_back("node " + std::to_string(i) + " exceeds the parameter buffer (needs "
                                        + std::to_string(contract.slotCount) + " slot(s): "
                                        + contract.semantic + ")");
            }
            if (metadata.requiresBackground && !segment.requiresBackground) {
                result.valid = false;
                result.errors.push_back("blend node requires a background SRV");
            }
            if (metadata.requiresLut && !segment.requiresLut) {
                result.valid = false;
                result.errors.push_back("LUT node requires a 3D LUT SRV");
            }
        }
        return result;
    }

    static PointwiseGeneratedShader generateComputeShader(
        std::string_view backend,
        std::string_view targetFormat,
        const std::vector<PointwiseEffectNode>& nodes,
        const PointwiseFusionSegment& segment) {
        PointwiseGeneratedShader result;
        result.key = makeCompileKey(backend, targetFormat, nodes, segment);
        const PointwiseFusionValidation validation = validateSegment(nodes, segment);
        if (!validation.valid) {
            result.diagnosticName = "PointwiseFusion_Invalid";
            for (const auto& error : validation.errors) {
                result.diagnosticName += "_" + stableId(error);
            }
            return result;
        }
        result.diagnosticName = "PointwiseFusion_" + stableId(result.key.toString());
        std::ostringstream hlsl;
        hlsl << "// Generated by ArtifactCore PointwiseEffectFusion.\n";
        hlsl << "// " << result.diagnosticName << "\n";
        hlsl << "Texture2D<float4> SourceTexture : register(t0);\n";
        unsigned textureRegister = 1;
        if (segment.requiresBackground) {
            hlsl << "Texture2D<float4> BackgroundTexture : register(t" << textureRegister++ << ");\n";
        }
        if (segment.requiresLut) {
            hlsl << "Texture3D<float4> LutTexture : register(t" << textureRegister++ << ");\n";
        }
        if (segment.requiresLut) {
            hlsl << "SamplerState LinearSampler : register(s0);\n";
        }
        hlsl << "cbuffer PointwiseParameters : register(b0) { float4 Parameters[64]; };\n";
        hlsl << "float3 ToStraight(float4 c) { return c.rgb / max(c.a, 1e-6); }\n";
        hlsl << "float3 ToPremultiplied(float3 c, float a) { return c * a; }\n";
        hlsl << "RWTexture2D<float4> OutputTexture : register(u0);\n";
        hlsl << "[numthreads(16, 16, 1)]\n";
        hlsl << "void PointwiseFusionCS(uint3 dispatchId : SV_DispatchThreadID) {\n";
        hlsl << "  uint width, height; OutputTexture.GetDimensions(width, height);\n";
        hlsl << "  if (dispatchId.x >= width || dispatchId.y >= height) return;\n";
        hlsl << "  uint2 pixel = dispatchId.xy;\n";
        hlsl << "  float4 color = SourceTexture.Load(int3(pixel, 0));\n";
        PointwiseAlphaMode alpha = segment.inputAlpha;
        const std::size_t end = segment.firstNode + segment.nodeCount;
        for (std::size_t i = segment.firstNode; i < end; ++i) {
            if (requiresStraightColor(nodes[i].kind) && alpha == PointwiseAlphaMode::Premultiplied) {
                hlsl << "  color.rgb = ToStraight(color);\n";
                alpha = PointwiseAlphaMode::Straight;
            }
            appendNodeHlsl(hlsl, nodes[i], alpha);
        }
        if (alpha != segment.outputAlpha) {
            if (segment.outputAlpha == PointwiseAlphaMode::Premultiplied) {
                hlsl << "  color.rgb = ToPremultiplied(color.rgb, color.a);\n";
            } else {
                hlsl << "  color.rgb = ToStraight(color);\n";
            }
        }
        hlsl << "  OutputTexture[pixel] = color;\n}\n";
        result.source = hlsl.str();
        return result;
    }

    static PointwiseComputePlan makeComputePlan(
        std::string_view backend,
        std::string_view targetFormat,
        const std::vector<PointwiseEffectNode>& nodes,
        const PointwiseFusionSegment& segment,
        std::uint32_t width,
        std::uint32_t height) {
        PointwiseComputePlan plan;
        plan.shader = generateComputeShader(backend, targetFormat, nodes, segment);
        plan.width = width;
        plan.height = height;
        return plan;
    }

    static PointwiseGeneratedShader generateNeighborhoodBlurShader(
        std::string_view backend,
        std::string_view targetFormat) {
        PointwiseGeneratedShader result;
        result.key.backend = std::string(backend);
        result.key.targetFormat = std::string(targetFormat);
        result.key.orderedNodeKinds = {PointwiseNodeKind::NeighborhoodBlur};
        result.entryPoint = "NeighborhoodBlurCS";
        result.diagnosticName = "NeighborhoodBlurCS";
        result.source = R"hlsl(
Texture2D<float4> SourceTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);
cbuffer NeighborhoodParameters : register(b0) { float4 Parameters[64]; };

[numthreads(16, 16, 1)]
void NeighborhoodBlurCS(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    OutputTexture.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;

    int2 pixel = int2(dispatchId.xy);
    float4 sum = 0.0;
    float weight = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            int2 samplePixel = clamp(pixel + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            float sampleWeight = (x == 0 && y == 0) ? 2.0 : 1.0;
            sum += SourceTexture.Load(int3(samplePixel, 0)) * sampleWeight;
            weight += sampleWeight;
        }
    }
    float strength = saturate(Parameters[0].x);
    float4 blurred = sum / max(weight, 1e-5);
    OutputTexture[dispatchId.xy] = lerp(SourceTexture.Load(int3(pixel, 0)), blurred, strength);
}
)hlsl";
        return result;
    }

    static PointwiseComputePlan makeNeighborhoodBlurPlan(
        std::string_view backend,
        std::string_view targetFormat,
        std::uint32_t width,
        std::uint32_t height) {
        PointwiseComputePlan plan;
        plan.shader = generateNeighborhoodBlurShader(backend, targetFormat);
        plan.parameterBuffer = "NeighborhoodParameters";
        plan.width = width;
        plan.height = height;
        return plan;
    }

    static PointwiseGeneratedShader generateTemporalBlendShader(
        std::string_view backend,
        std::string_view targetFormat) {
        PointwiseGeneratedShader result;
        result.key.backend = std::string(backend);
        result.key.targetFormat = std::string(targetFormat);
        result.key.orderedNodeKinds = {PointwiseNodeKind::Temporal};
        result.key.requiresHistory = true;
        result.entryPoint = "TemporalBlendCS";
        result.diagnosticName = "TemporalBlendCS";
        result.source = R"hlsl(
Texture2D<float4> SourceTexture : register(t0);
Texture2D<float4> HistoryTexture : register(t1);
RWTexture2D<float4> OutputTexture : register(u0);
cbuffer TemporalParameters : register(b0) { float4 Parameters[64]; };

[numthreads(16, 16, 1)]
void TemporalBlendCS(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    OutputTexture.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;
    int3 pixel = int3(dispatchId.xy, 0);
    float historyAmount = saturate(Parameters[0].x);
    OutputTexture[dispatchId.xy] = lerp(
        SourceTexture.Load(pixel), HistoryTexture.Load(pixel), historyAmount);
}
)hlsl";
        return result;
    }

    static PointwiseComputePlan makeTemporalBlendPlan(
        std::string_view backend,
        std::string_view targetFormat,
        std::uint32_t width,
        std::uint32_t height) {
        PointwiseComputePlan plan;
        plan.shader = generateTemporalBlendShader(backend, targetFormat);
        plan.parameterBuffer = "TemporalParameters";
        plan.width = width;
        plan.height = height;
        return plan;
    }

private:
    static bool requiresStraightColor(PointwiseNodeKind kind) {
        return descriptor(kind).requiresStraightAlpha;
    }

    static std::string boundaryReason(PointwiseNodeKind kind) {
        switch (kind) {
        case PointwiseNodeKind::NeighborhoodBlur:
        case PointwiseNodeKind::Neighborhood: return "neighborhood sampling requires a pass boundary";
        case PointwiseNodeKind::Temporal: return "temporal history requires a pass boundary";
        case PointwiseNodeKind::CpuBoundary: return "CPU boundary requires a pass boundary";
        default: return "node is not pointwise";
        }
    }

    static std::string stableId(std::string_view input) {
        std::uint64_t hash = 1469598103934665603ull;
        for (const char c : input) {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ull;
        }
        std::ostringstream result;
        result << std::hex << hash;
        return result.str();
    }

    static void appendNodeHlsl(
        std::ostringstream& hlsl,
        const PointwiseEffectNode& node,
        PointwiseAlphaMode& alpha) {
        const std::string p = "Parameters[" + std::to_string(node.parameterIndex) + "]";
        switch (node.kind) {
        case PointwiseNodeKind::Exposure:
            hlsl << "  color.rgb *= exp2(" << p << ".x);\n";
            break;
        case PointwiseNodeKind::Gamma:
            hlsl << "  color.rgb = pow(max(color.rgb, 0.0), 1.0 / max(" << p << ".xxx, 1e-4));\n";
            break;
        case PointwiseNodeKind::Contrast:
            hlsl << "  color.rgb = (color.rgb - 0.5) * " << p << ".x + 0.5;\n";
            break;
        case PointwiseNodeKind::Levels:
            hlsl << "  color.rgb = saturate((color.rgb - " << p << ".xxx) / max(Parameters["
                 << node.parameterIndex + 1 << "].xxx - " << p << ".xxx, 1e-4));\n";
            break;
        case PointwiseNodeKind::Saturation:
            hlsl << "  { float luma = dot(color.rgb, float3(0.2126, 0.7152, 0.0722)); color.rgb = lerp(float3(luma, luma, luma), color.rgb, " << p << ".x); }\n";
            break;
        case PointwiseNodeKind::Tint:
            hlsl << "  color.rgb *= " << p << ".rgb;\n";
            break;
        case PointwiseNodeKind::HueRotate:
            hlsl << "  { float angle = " << p << ".x; float s = sin(angle); float c = cos(angle);\n";
            hlsl << "    float3x3 hue = float3x3(c + (1.0 - c) / 3.0, (1.0 - c) / 3.0 - s / 1.7320508, (1.0 - c) / 3.0 + s / 1.7320508,\n";
            hlsl << "      (1.0 - c) / 3.0 + s / 1.7320508, c + (1.0 - c) / 3.0, (1.0 - c) / 3.0 - s / 1.7320508,\n";
            hlsl << "      (1.0 - c) / 3.0 - s / 1.7320508, (1.0 - c) / 3.0 + s / 1.7320508, c + (1.0 - c) / 3.0);\n";
            hlsl << "    color.rgb = mul(hue, color.rgb); }\n";
            break;
        case PointwiseNodeKind::ColorTemperature:
            hlsl << "  { float temperature = " << p << ".x; float tint = " << p << ".y;\n";
            hlsl << "    color.rgb *= float3(1.0 + temperature * 0.10, 1.0 + tint * 0.04, 1.0 - temperature * 0.10); }\n";
            break;
        case PointwiseNodeKind::Clamp:
            hlsl << "  color.rgb = clamp(color.rgb, " << p << ".xxx, Parameters["
                 << node.parameterIndex + 1 << "].xxx);\n";
            break;
        case PointwiseNodeKind::Lut3D:
            hlsl << "  color.rgb = lerp(color.rgb, LutTexture.SampleLevel(LinearSampler, saturate(color.rgb), 0).rgb, " << p << ".x);\n";
            break;
        case PointwiseNodeKind::Blend:
            hlsl << "  { float4 background = BackgroundTexture.Load(int3(pixel, 0));\n";
            switch (node.blendMode) {
            case PointwiseBlendMode::Add:
                hlsl << "    color.rgb += background.rgb;\n";
                break;
            case PointwiseBlendMode::Multiply:
                hlsl << "    color.rgb *= background.rgb;\n";
                break;
            case PointwiseBlendMode::Screen:
                hlsl << "    color.rgb = 1.0 - (1.0 - color.rgb) * (1.0 - background.rgb);\n";
                break;
            case PointwiseBlendMode::Normal:
                if (alpha == PointwiseAlphaMode::Premultiplied) {
                    hlsl << "    color.rgb = color.rgb + background.rgb * (1.0 - color.a);\n";
                    hlsl << "    color.a = color.a + background.a * (1.0 - color.a);\n";
                } else {
                    hlsl << "    float outputAlpha = color.a + background.a * (1.0 - color.a);\n";
                    hlsl << "    color.rgb = (color.rgb * color.a + background.rgb * background.a * (1.0 - color.a)) / max(outputAlpha, 1e-6);\n";
                    hlsl << "    color.a = outputAlpha;\n";
                }
                break;
            }
            if (node.blendMode != PointwiseBlendMode::Normal) {
                hlsl << "    color.a = max(color.a, background.a);\n";
            }
            hlsl << "  }\n";
            break;
        case PointwiseNodeKind::AlphaConvert:
            if (alpha == PointwiseAlphaMode::Premultiplied) {
                hlsl << "  color.rgb = ToStraight(color);\n";
                alpha = PointwiseAlphaMode::Straight;
            } else {
                hlsl << "  color.rgb = ToPremultiplied(color.rgb, color.a);\n";
                alpha = PointwiseAlphaMode::Premultiplied;
            }
            break;
        case PointwiseNodeKind::Posterize:
            hlsl << "  color.rgb = floor(saturate(color.rgb) * max(" << p << ".x - 1.0, 1.0) + 0.5) / max(" << p << ".x - 1.0, 1.0);\n";
            break;
        case PointwiseNodeKind::Threshold:
            hlsl << "  { float value = step(" << p << ".x, dot(color.rgb, float3(0.2126, 0.7152, 0.0722))); color.rgb = float3(value, value, value); }\n";
            break;
        case PointwiseNodeKind::NeighborhoodBlur:
        case PointwiseNodeKind::Neighborhood:
        case PointwiseNodeKind::Temporal:
        case PointwiseNodeKind::CpuBoundary:
            break;
        }
    }
};

inline std::vector<PointwiseFusionSegment> PointwiseEffectStack::segments(
    PointwiseAlphaMode initialAlpha) const {
    return PointwiseEffectFusion::segment(nodes_, initialAlpha);
}

inline std::vector<EffectDomainSegment> PointwiseEffectStack::domainSegments() const {
    std::vector<EffectDomainSegment> result;
    std::size_t index = 0;
    while (index < nodes_.size()) {
        const EffectExecutionDomain domain =
            PointwiseEffectFusion::executionDomain(nodes_[index].kind);
        const std::size_t start = index++;
        while (index < nodes_.size() &&
               PointwiseEffectFusion::executionDomain(nodes_[index].kind) == domain) {
            ++index;
        }
        result.push_back({start, index - start, domain});
    }
    return result;
}

inline PointwiseStackValidation PointwiseEffectStack::validate() const {
    PointwiseStackValidation result;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        const auto metadata = PointwiseEffectFusion::descriptor(node.kind);
        if (node.parameterIndex >= kParameterSlotCount ||
            metadata.parameters.slotCount > kParameterSlotCount - node.parameterIndex) {
            result.valid = false;
            result.errors.push_back(
                "node " + std::to_string(i) + " exceeds parameter contract: " +
                metadata.parameters.semantic);
        }
        if (metadata.domain == EffectExecutionDomain::Pointwise && !metadata.pointwise) {
            result.valid = false;
            result.errors.push_back(
                "node " + std::to_string(i) + " has an inconsistent pointwise descriptor");
        }
    }
    return result;
}

struct PointwiseFusionDiagnostics {
    std::size_t fusedSegmentCount = 0;
    std::size_t fallbackSegmentCount = 0;
    std::size_t fusedNodeCount = 0;
    std::vector<std::string> messages;
};

class PointwiseShaderCache {
public:
    const PointwiseGeneratedShader* find(const PointwiseCompileKey& key) const {
        const auto it = entries_.find(key.toString());
        return it == entries_.end() ? nullptr : &it->second;
    }

    const PointwiseGeneratedShader& getOrGenerate(
        std::string_view backend,
        std::string_view targetFormat,
        const std::vector<PointwiseEffectNode>& nodes,
        const PointwiseFusionSegment& segment) {
        const PointwiseCompileKey key =
            PointwiseEffectFusion::makeCompileKey(backend, targetFormat, nodes, segment);
        const std::string keyText = key.toString();
        if (const auto it = entries_.find(keyText); it != entries_.end()) {
            ++hitCount_;
            return it->second;
        }
        ++missCount_;
        auto generated = PointwiseEffectFusion::generateComputeShader(
            backend, targetFormat, nodes, segment);
        auto [it, inserted] = entries_.emplace(std::move(keyText), std::move(generated));
        (void)inserted;
        return it->second;
    }

    PointwiseComputePlan makeComputePlan(
        std::string_view backend,
        std::string_view targetFormat,
        const std::vector<PointwiseEffectNode>& nodes,
        const PointwiseFusionSegment& segment,
        std::uint32_t width,
        std::uint32_t height) {
        PointwiseComputePlan plan;
        const PointwiseCompileKey key =
            PointwiseEffectFusion::makeCompileKey(backend, targetFormat, nodes, segment);
        const std::string keyText = key.toString();
        if (const auto it = entries_.find(keyText); it != entries_.end()) {
            ++hitCount_;
            plan.shader = it->second;
        } else {
            ++missCount_;
            auto generated = PointwiseEffectFusion::generateComputeShader(
                backend, targetFormat, nodes, segment);
            auto [it, inserted] = entries_.emplace(keyText, std::move(generated));
            (void)inserted;
            plan.shader = it->second;
        }
        plan.width = width;
        plan.height = height;
        return plan;
    }

    PointwiseComputePlan makeComputePlan(
        std::string_view backend,
        std::string_view targetFormat,
        const PointwiseEffectStack& stack,
        const PointwiseFusionSegment& segment,
        std::uint32_t width,
        std::uint32_t height) {
        return makeComputePlan(
            backend, targetFormat, stack.nodes(), segment, width, height);
    }

    PointwiseComputePlan makeNeighborhoodBlurPlan(
        std::string_view backend,
        std::string_view targetFormat,
        std::uint32_t width,
        std::uint32_t height) {
        PointwiseComputePlan plan;
        const std::string keyText = std::string(backend) + "|" +
            std::string(targetFormat) + "|NeighborhoodBlur";
        if (const auto it = entries_.find(keyText); it != entries_.end()) {
            ++hitCount_;
            plan.shader = it->second;
        } else {
            ++missCount_;
            auto generated = PointwiseEffectFusion::generateNeighborhoodBlurShader(
                backend, targetFormat);
            auto [it, inserted] = entries_.emplace(keyText, std::move(generated));
            (void)inserted;
            plan.shader = it->second;
        }
        plan.parameterBuffer = "NeighborhoodParameters";
        plan.width = width;
        plan.height = height;
        return plan;
    }

    PointwiseComputePlan makeTemporalBlendPlan(
        std::string_view backend,
        std::string_view targetFormat,
        std::uint32_t width,
        std::uint32_t height) {
        PointwiseComputePlan plan;
        const std::string keyText = std::string(backend) + "|" +
            std::string(targetFormat) + "|TemporalBlend";
        if (const auto it = entries_.find(keyText); it != entries_.end()) {
            ++hitCount_;
            plan.shader = it->second;
        } else {
            ++missCount_;
            auto generated = PointwiseEffectFusion::generateTemporalBlendShader(
                backend, targetFormat);
            auto [it, inserted] = entries_.emplace(keyText, std::move(generated));
            (void)inserted;
            plan.shader = it->second;
        }
        plan.parameterBuffer = "TemporalParameters";
        plan.width = width;
        plan.height = height;
        return plan;
    }

    void clear() {
        entries_.clear();
        hitCount_ = 0;
        missCount_ = 0;
    }

    std::size_t entryCount() const { return entries_.size(); }
    std::uint64_t hitCount() const { return hitCount_; }
    std::uint64_t missCount() const { return missCount_; }

private:
    std::unordered_map<std::string, PointwiseGeneratedShader> entries_;
    std::uint64_t hitCount_ = 0;
    std::uint64_t missCount_ = 0;
};

inline PointwiseFusionDiagnostics describePointwiseFusion(
    const std::vector<PointwiseEffectNode>& nodes,
    PointwiseAlphaMode initialAlpha = PointwiseAlphaMode::Premultiplied) {
    PointwiseFusionDiagnostics result;
    for (const auto& segment : PointwiseEffectFusion::segment(nodes, initialAlpha)) {
        if (segment.isFused) {
            ++result.fusedSegmentCount;
            result.fusedNodeCount += segment.nodeCount;
            result.messages.push_back(
                "fused nodes " + std::to_string(segment.firstNode) + "-" +
                std::to_string(segment.firstNode + segment.nodeCount - 1));
        } else {
            ++result.fallbackSegmentCount;
            result.messages.push_back(
                "pass boundary at node " + std::to_string(segment.firstNode) + ": " +
                (segment.fallbackReason.empty() ? "single pointwise node" : segment.fallbackReason));
        }
    }
    return result;
}

} // namespace ArtifactCore
