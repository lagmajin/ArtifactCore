module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
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
    Lut3D,
    Blend,
    AlphaConvert,
    Posterize,
    Threshold,
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
            << (requiresLut ? 'L' : '-');
        for (std::size_t i = 0; i < orderedNodeKinds.size(); ++i) {
            key << '|' << static_cast<unsigned>(orderedNodeKinds[i])
                << (i < staticSpecializations.size() && staticSpecializations[i] ? 'S' : 'D');
        }
        return key.str();
    }
};

struct PointwiseGeneratedShader {
    PointwiseCompileKey key;
    std::string entryPoint = "PointwiseFusionPS";
    std::string source;
    std::string diagnosticName;
};

class PointwiseEffectFusion {
public:
    static bool isPointwise(const PointwiseEffectNode& node) {
        if (!node.enabled) {
            return false;
        }
        switch (node.kind) {
        case PointwiseNodeKind::Exposure:
        case PointwiseNodeKind::Gamma:
        case PointwiseNodeKind::Contrast:
        case PointwiseNodeKind::Levels:
        case PointwiseNodeKind::Saturation:
        case PointwiseNodeKind::Tint:
        case PointwiseNodeKind::Lut3D:
        case PointwiseNodeKind::Blend:
        case PointwiseNodeKind::AlphaConvert:
        case PointwiseNodeKind::Posterize:
        case PointwiseNodeKind::Threshold:
            return true;
        case PointwiseNodeKind::Neighborhood:
        case PointwiseNodeKind::Temporal:
        case PointwiseNodeKind::CpuBoundary:
            return false;
        }
        return false;
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
                segment.requiresBackground |= node.kind == PointwiseNodeKind::Blend ||
                                              node.requiresBackground;
                segment.requiresLut |= node.kind == PointwiseNodeKind::Lut3D;
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
        const std::size_t end = segment.firstNode + segment.nodeCount;
        for (std::size_t i = segment.firstNode; i < end; ++i) {
            key.orderedNodeKinds.push_back(nodes[i].kind);
            key.staticSpecializations.push_back(nodes[i].staticSpecialization);
        }
        return key;
    }

    static PointwiseGeneratedShader generatePixelShader(
        std::string_view backend,
        std::string_view targetFormat,
        const std::vector<PointwiseEffectNode>& nodes,
        const PointwiseFusionSegment& segment) {
        PointwiseGeneratedShader result;
        result.key = makeCompileKey(backend, targetFormat, nodes, segment);
        result.diagnosticName = "PointwiseFusion_" + stableId(result.key.toString());
        std::ostringstream hlsl;
        hlsl << "// Generated by ArtifactCore PointwiseEffectFusion.\n";
        hlsl << "// " << result.diagnosticName << "\n";
        hlsl << "Texture2D SourceTexture : register(t0);\n";
        unsigned textureRegister = 1;
        if (segment.requiresBackground) {
            hlsl << "Texture2D BackgroundTexture : register(t" << textureRegister++ << ");\n";
        }
        if (segment.requiresLut) {
            hlsl << "Texture3D LutTexture : register(t" << textureRegister++ << ");\n";
        }
        hlsl << "SamplerState LinearSampler : register(s0);\n";
        hlsl << "cbuffer PointwiseParameters : register(b0) { float4 Parameters[64]; };\n";
        hlsl << "float3 ToStraight(float4 c) { return c.rgb / max(c.a, 1e-6); }\n";
        hlsl << "float3 ToPremultiplied(float3 c, float a) { return c * a; }\n";
        hlsl << "float4 PointwiseFusionPS(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {\n";
        hlsl << "  float4 color = SourceTexture.Sample(LinearSampler, uv);\n";
        PointwiseAlphaMode alpha = segment.inputAlpha;
        const std::size_t end = segment.firstNode + segment.nodeCount;
        for (std::size_t i = segment.firstNode; i < end; ++i) {
            appendNodeHlsl(hlsl, nodes[i], alpha);
        }
        hlsl << "  return color;\n}\n";
        result.source = hlsl.str();
        return result;
    }

private:
    static std::string boundaryReason(PointwiseNodeKind kind) {
        switch (kind) {
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
        case PointwiseNodeKind::Lut3D:
            hlsl << "  color.rgb = lerp(color.rgb, LutTexture.SampleLevel(LinearSampler, saturate(color.rgb), 0).rgb, " << p << ".x);\n";
            break;
        case PointwiseNodeKind::Blend:
            hlsl << "  { float4 background = BackgroundTexture.Sample(LinearSampler, uv);\n";
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
                hlsl << "    color.rgb = lerp(background.rgb, color.rgb, color.a);\n";
                break;
            }
            hlsl << "    color.a = max(color.a, background.a); }\n";
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
        case PointwiseNodeKind::Neighborhood:
        case PointwiseNodeKind::Temporal:
        case PointwiseNodeKind::CpuBoundary:
            break;
        }
    }
};

} // namespace ArtifactCore
