module;
#include <algorithm>
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

struct PointwiseFusionValidation {
    bool valid = true;
    std::vector<std::string> errors;
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
            if (node.parameterIndex >= 64) {
                result.valid = false;
                result.errors.push_back("node " + std::to_string(i) + " exceeds the parameter buffer");
            }
            if (node.kind == PointwiseNodeKind::Levels && node.parameterIndex >= 63) {
                result.valid = false;
                result.errors.push_back("levels node " + std::to_string(i) + " needs two parameter slots");
            }
            if (node.kind == PointwiseNodeKind::Blend && !segment.requiresBackground) {
                result.valid = false;
                result.errors.push_back("blend node requires a background SRV");
            }
            if (node.kind == PointwiseNodeKind::Lut3D && !segment.requiresLut) {
                result.valid = false;
                result.errors.push_back("LUT node requires a 3D LUT SRV");
            }
        }
        return result;
    }

    static PointwiseGeneratedShader generatePixelShader(
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
        hlsl << "  return color;\n}\n";
        result.source = hlsl.str();
        return result;
    }

private:
    static bool requiresStraightColor(PointwiseNodeKind kind) {
        switch (kind) {
        case PointwiseNodeKind::Exposure:
        case PointwiseNodeKind::Gamma:
        case PointwiseNodeKind::Contrast:
        case PointwiseNodeKind::Levels:
        case PointwiseNodeKind::Saturation:
        case PointwiseNodeKind::Tint:
        case PointwiseNodeKind::Lut3D:
        case PointwiseNodeKind::Posterize:
        case PointwiseNodeKind::Threshold:
            return true;
        case PointwiseNodeKind::Blend:
        case PointwiseNodeKind::AlphaConvert:
        case PointwiseNodeKind::Neighborhood:
        case PointwiseNodeKind::Temporal:
        case PointwiseNodeKind::CpuBoundary:
            return false;
        }
        return false;
    }

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
        case PointwiseNodeKind::Neighborhood:
        case PointwiseNodeKind::Temporal:
        case PointwiseNodeKind::CpuBoundary:
            break;
        }
    }
};

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
        auto generated = PointwiseEffectFusion::generatePixelShader(
            backend, targetFormat, nodes, segment);
        auto [it, inserted] = entries_.emplace(std::move(keyText), std::move(generated));
        (void)inserted;
        return it->second;
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
