module;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.ShaderNode.Core;

export namespace Artifact {
namespace ShaderNode {

    enum class PinType {
        Float,
        Vector2,
        Vector3,
        Vector4,
        Texture2D,
        // BSDF-bundle link (Blender Shader socket equivalent). Codegen treats
        // this as a bundle-prefix reference, not a value variable.
        Shader
    };

    class ShaderNodeBase;

    class Pin {
    public:
        std::string name;
        PinType type;
        ShaderNodeBase* owner;
        bool isInput;
        std::string getDefaultValueString() const;
    };

    class Link {
    public:
        Pin* fromPin;
        Pin* toPin;
    };

    // ノードの状態
    export enum class NodeState {
        Clean,
        Dirty,
        Evaluating,
        Cached
    };

    // デバッグ表示用のノード情報
    export struct NodeDebugInfo {
        std::string id;
        std::string name;
        NodeState state;
        std::string lastDirtyReason;
        float lastExecutionTimeMs;
        std::vector<std::string> dependencies; // 入力元のID
    };

    class ShaderNodeBase {
    public:
        std::string id;
        std::string name;
        std::vector<std::unique_ptr<Pin>> inputs;
        std::vector<std::unique_ptr<Pin>> outputs;
        
        NodeState state = NodeState::Dirty;
        std::string lastDirtyReason = "Initial";
        float lastExecutionTimeMs = 0.0f;

        virtual ~ShaderNodeBase() = default;
        virtual std::string generateHLSL() const = 0;

        // デバッグ情報の取得
        NodeDebugInfo getDebugInfo() const;
        
        // 状態更新
        void markDirty(const std::string& reason);
        
    protected:
        void addInput(const std::string& name, PinType type);
        void addOutput(const std::string& name, PinType type);
    };

    // A simple color output node
    class ColorNode : public ShaderNodeBase {
    public:
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
        ColorNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // The final output node
    class OutputNode : public ShaderNodeBase {
    public:
        OutputNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Math Add node
    class AddNode : public ShaderNodeBase {
    public:
        AddNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // --- Blender-style material graph (realtime lowering) ---
    // Closures do not exist in the fixed MeshRenderer lighting path, so a
    // BSDF link compiles to a bundle of PBR parameter variables named
    // "<sanitizedId>_baseColor|_metallic|_roughness|_emission|_alpha"
    // "|_normal|_occlusion".
    // compileMaterialGraph() emits two splices: helperHlsl (global scope,
    // function definitions) and blockHlsl (PSMain body after the texture
    // samples, where In, g_* textures/samplers and cbuffers are in scope).
    // blockHlsl MUST define: float4 graphBaseColor; float graphMetallic;
    // float graphRoughness; float3 graphEmission; float graphAlpha;
    // float3 graphNormalSample; float graphOcclusion.
    // Only In.UV may be referenced by graph code (phase 1). Unconnected
    // Normal/Occlusion fall back to the sampled values (fixed-path parity).
    std::string sanitizeShaderId(const std::string& id);

    class ValueNode : public ShaderNodeBase {
    public:
        float value = 0.0f;
        ValueNode(const std::string& nodeId, float v = 0.0f);
        std::string generateHLSL() const override;
    };

    class RGBNode : public ShaderNodeBase {
    public:
        float r = 0.5f, g = 0.5f, b = 0.5f, a = 1.0f;
        RGBNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Phase 1 exposes UV only (Blender TexCoord has more sockets).
    class TexCoordNode : public ShaderNodeBase {
    public:
        TexCoordNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Geometry: world-space position/normal plus the incoming
    // ray direction (Position -> Camera). Uses PSMain In + SceneCamera.
    class GeometryNode : public ShaderNodeBase {
    public:
        GeometryNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Fresnel (dielectric Schlick with IOR-derived F0).
    class FresnelNode : public ShaderNodeBase {
    public:
        FresnelNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Layer Weight. Fresnel is Schlick with fixed IOR 1.45.
    // Facing is a phase-1 interpretation (pow curve around the identity
    // at Blend 0.5); the exact Blender curve is unverified.
    class LayerWeightNode : public ShaderNodeBase {
    public:
        LayerWeightNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Blackbody (Tanner Helland approximation, clamped range).
    class BlackbodyNode : public ShaderNodeBase {
    public:
        BlackbodyNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Samples an already-bound material texture slot (no new SRB work).
    // Custom file-backed slots are out of scope for phase 1.
    class ImageTextureNode : public ShaderNodeBase {
    public:
        enum class Slot {
            BaseColor,
            Opacity,
            Emission,
            MetallicRoughness,
            Normal,
            Occlusion
        };
        Slot slot = Slot::BaseColor;
        ImageTextureNode(const std::string& nodeId,
                         Slot s = Slot::BaseColor);
        std::string generateHLSL() const override;
    };

    // Procedural textures (HLSL-only, no bindings). Scale follows the
    // Blender convention (UV * Scale). Octaves are fixed (fbm x4).
    class NoiseTextureNode : public ShaderNodeBase {
    public:
        NoiseTextureNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    class VoronoiTextureNode : public ShaderNodeBase {
    public:
        VoronoiTextureNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Mix (legacy MixRGB): result = lerp(C1, blend(C1,C2), Fac).
    class MixRGBNode : public ShaderNodeBase {
    public:
        enum class Blend {
            Mix,
            Add,
            Multiply,
            Screen,
            Overlay,
            Darken,
            Lighten,
            Difference
        };
        Blend blend = Blend::Mix;
        bool clampFactor = true;
        bool clampResult = false;
        MixRGBNode(const std::string& nodeId, Blend b = Blend::Mix);
        std::string generateHLSL() const override;
    };

    // Blender Color Ramp: arbitrary stops, per-ramp interpolation.
    class ColorRampNode : public ShaderNodeBase {
    public:
        enum class Interp { Linear, Constant, Ease };
        struct Stop {
            float pos = 0.0f;
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
        };
        Interp interp = Interp::Linear;
        std::vector<Stop> stops;
        ColorRampNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    class MathNode : public ShaderNodeBase {
    public:
        // Vectorized over float4 (covers Blender Math + Vector Math use).
        // Single-operand ops use A only; Mix additionally uses Fac.
        enum class Op {
            Add, Subtract, Multiply, Divide, Mix,
            Sine, Cosine, Power, Minimum, Maximum,
            LessThan, GreaterThan, Modulo,
            Absolute, Floor, Ceil, Fract
        };
        Op op = Op::Add;
        bool clampResult = false;
        MathNode(const std::string& nodeId, Op operation = Op::Add);
        std::string generateHLSL() const override;
    };

    class PrincipledBSDFNode : public ShaderNodeBase {
    public:
        PrincipledBSDFNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    class MixShaderNode : public ShaderNodeBase {
    public:
        MixShaderNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    class MaterialOutputNode : public ShaderNodeBase {
    public:
        MaterialOutputNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Normal Map: tangent-space sample in, perturbed sample out.
    // Feeds Principled Normal (same encoding as texture normalSample).
    class NormalMapNode : public ShaderNodeBase {
    public:
        NormalMapNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Bump: height derivatives perturb the WORLD normal (Mikkelsen
    // unparametrized-surface form). Output is world space: link it to
    // Material Output Displacement, NOT to Principled Normal.
    class BumpNode : public ShaderNodeBase {
    public:
        BumpNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Invert: lerp(Color, 1 - Color, Fac).
    class InvertNode : public ShaderNodeBase {
    public:
        InvertNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Gamma: per-channel pow(Color, Gamma), alpha passthrough.
    class GammaNode : public ShaderNodeBase {
    public:
        GammaNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Hue/Saturation/Value (standard HSV roundtrip helpers).
    class HueSatValNode : public ShaderNodeBase {
    public:
        HueSatValNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // RGB to BW luminance (Rec.709 weights).
    class LuminanceNode : public ShaderNodeBase {
    public:
        LuminanceNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Map Range (float mode; vector/color modes deferred).
    class MapRangeNode : public ShaderNodeBase {
    public:
        enum class Interp { Linear, Stepped, SmoothStep, SmootherStep };
        Interp interp = Interp::Linear;
        bool clampRange = true;
        MapRangeNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    // Blender Clamp (min/max auto-ordered).
    class ClampNode : public ShaderNodeBase {
    public:
        ClampNode(const std::string& nodeId);
        std::string generateHLSL() const override;
    };

    struct MaterialGraphResult {
        bool ok = false;
        std::string error;
        // Global-scope helpers (functions). Splice before PSMain.
        std::string helperHlsl;
        // Body statements. Splice inside PSMain after the texture samples.
        std::string blockHlsl;
        std::string hashHex;
    };

    struct MaterialGraphLayout {
        std::string nodeId;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct MaterialGraphLoadResult {
        bool ok = false;
        std::string error;
        std::vector<MaterialGraphLayout> layout;
    };

    class NodeGraph {
    public:
        std::vector<std::unique_ptr<ShaderNodeBase>> nodes;
        std::vector<std::unique_ptr<Link>> links;
        
        ShaderNodeBase* addNode(std::unique_ptr<ShaderNodeBase> node);
        void link(Pin* from, Pin* to);
        // Removes the exact link. No-op when absent.
        bool unlink(Pin* from, Pin* to);
        // Removes whatever feeds the given input pin. No-op when free.
        void disconnectInput(Pin* toPin);
        // Removes the node and all links touching its pins.
        bool removeNode(ShaderNodeBase* node);
        
        // Generates the final concatenated HLSL text
        std::string compileHLSL() const;
        // Blender-style material compile. Never throws; on structural error
        // returns ok=false with a magenta-fallback block (still compilable).
        MaterialGraphResult compileMaterialGraph() const;
        // Versioned JSON round-trip (nodes + links + per-type params).
        // Layout (canvas positions) travels alongside, keyed by node id.
        std::string toJson(const std::vector<MaterialGraphLayout>& layout) const;
        // Replaces content on success; leaves the graph untouched on failure.
        MaterialGraphLoadResult fromJson(const std::string& json);
    private:
        std::vector<ShaderNodeBase*> getTopologicalOrder() const;
        Pin* getConnectedInput(Pin* toPin) const;
    };

} // namespace ShaderNode
} // namespace Artifact
