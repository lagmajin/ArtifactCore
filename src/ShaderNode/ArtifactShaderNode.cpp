module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <format>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtGlobal>

module Artifact.ShaderNode.Core;

import Core.ArtifactString;
import Container.NamedVector;

namespace Artifact {
namespace ShaderNode {

    std::string Pin::getDefaultValueString() const {
        switch(type) {
            case PinType::Float: return "0.0";
            case PinType::Vector2: return "float2(0.0, 0.0)";
            case PinType::Vector3: return "float3(0.0, 0.0, 0.0)";
            case PinType::Vector4: return "float4(0.0, 0.0, 0.0, 1.0)";
            case PinType::Shader: return "0";
            default: return "0";
        }
    }

    std::string sanitizeShaderId(const std::string& id) {
        std::string out;
        out.reserve(id.size() + 2);
        for (char c : id) {
            const bool ok = (c >= 'A' && c <= 'Z') ||
                            (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '_';
            out.push_back(ok ? c : '_');
        }
        if (out.empty() || (out[0] >= '0' && out[0] <= '9')) {
            out = "n_" + out;
        }
        return out;
    }

    void ShaderNodeBase::addInput(const std::string& name, PinType type) {
        auto pin = std::make_unique<Pin>();
        pin->name = name;
        pin->type = type;
        pin->owner = this;
        pin->isInput = true;
        inputs.push_back(std::move(pin));
    }

    void ShaderNodeBase::addOutput(const std::string& name, PinType type) {
        auto pin = std::make_unique<Pin>();
        pin->name = name;
        pin->type = type;
        pin->owner = this;
        pin->isInput = false;
        outputs.push_back(std::move(pin));
    }

    ColorNode::ColorNode(const std::string& nodeId) {
        id = nodeId;
        name = "Color";
        addOutput("Color", PinType::Vector4);
    }

    std::string ColorNode::generateHLSL() const {
        return std::format("float4 {}_out = float4({}f, {}f, {}f, {}f);\n", 
                           id, r, g, b, a);
    }

    OutputNode::OutputNode(const std::string& nodeId) {
        id = nodeId;
        name = "Output";
        addInput("Surface", PinType::Vector4);
    }

    std::string OutputNode::generateHLSL() const {
        return std::format("return {};\n", inputs[0]->name); // Simplified for prototype
    }

    AddNode::AddNode(const std::string& nodeId) {
        id = nodeId;
        name = "Add";
        addInput("A", PinType::Vector4);
        addInput("B", PinType::Vector4);
        addOutput("Result", PinType::Vector4);
    }

    ValueNode::ValueNode(const std::string& nodeId, float v) {
        id = nodeId;
        name = "Value";
        value = v;
        addOutput("Value", PinType::Float);
    }

    RGBNode::RGBNode(const std::string& nodeId) {
        id = nodeId;
        name = "RGB";
        addOutput("Color", PinType::Vector4);
    }

    TexCoordNode::TexCoordNode(const std::string& nodeId) {
        id = nodeId;
        name = "Texture Coordinate";
        addOutput("UV", PinType::Vector2);
    }

    GeometryNode::GeometryNode(const std::string& nodeId) {
        id = nodeId;
        name = "Geometry";
        addOutput("Position", PinType::Vector3);
        addOutput("Normal", PinType::Vector3);
        addOutput("Incoming", PinType::Vector3);
    }

    FresnelNode::FresnelNode(const std::string& nodeId) {
        id = nodeId;
        name = "Fresnel";
        addInput("IOR", PinType::Float);
        addInput("Normal", PinType::Vector3);
        addOutput("Fac", PinType::Float);
    }

    LayerWeightNode::LayerWeightNode(const std::string& nodeId) {
        id = nodeId;
        name = "Layer Weight";
        addInput("Blend", PinType::Float);
        addInput("Normal", PinType::Vector3);
        addOutput("Fresnel", PinType::Float);
        addOutput("Facing", PinType::Float);
    }

    BlackbodyNode::BlackbodyNode(const std::string& nodeId) {
        id = nodeId;
        name = "Blackbody";
        addInput("Temperature", PinType::Float);
        addOutput("Color", PinType::Vector4);
    }

    ImageTextureNode::ImageTextureNode(const std::string& nodeId, Slot s) {
        id = nodeId;
        name = "Image Texture";
        slot = s;
        addInput("UV", PinType::Vector2);
        addOutput("Color", PinType::Vector4);
        addOutput("Alpha", PinType::Float);
    }

    NoiseTextureNode::NoiseTextureNode(const std::string& nodeId) {
        id = nodeId;
        name = "Noise Texture";
        addInput("Vector", PinType::Vector2);
        addInput("Scale", PinType::Float);
        addOutput("Fac", PinType::Float);
        addOutput("Color", PinType::Vector4);
    }

    VoronoiTextureNode::VoronoiTextureNode(const std::string& nodeId) {
        id = nodeId;
        name = "Voronoi Texture";
        addInput("Vector", PinType::Vector2);
        addInput("Scale", PinType::Float);
        addOutput("Distance", PinType::Float);
        addOutput("Color", PinType::Vector4);
    }

    MixRGBNode::MixRGBNode(const std::string& nodeId, Blend b) {
        id = nodeId;
        name = "Mix";
        blend = b;
        addInput("Fac", PinType::Float);
        addInput("Color1", PinType::Vector4);
        addInput("Color2", PinType::Vector4);
        addOutput("Color", PinType::Vector4);
    }

    ColorRampNode::ColorRampNode(const std::string& nodeId) {
        id = nodeId;
        name = "Color Ramp";
        stops.push_back(Stop{0.0f, 0.0f, 0.0f, 0.0f, 1.0f});
        stops.push_back(Stop{1.0f, 1.0f, 1.0f, 1.0f, 1.0f});
        addInput("Fac", PinType::Float);
        addOutput("Color", PinType::Vector4);
        addOutput("Alpha", PinType::Float);
    }

    MathNode::MathNode(const std::string& nodeId, Op operation) {
        id = nodeId;
        name = "Math";
        op = operation;
        addInput("A", PinType::Vector4);
        addInput("B", PinType::Vector4);
        addInput("Fac", PinType::Float);
        addOutput("Value", PinType::Vector4);
    }

    PrincipledBSDFNode::PrincipledBSDFNode(const std::string& nodeId) {
        id = nodeId;
        name = "Principled BSDF";
        addInput("BaseColor", PinType::Vector4);
        addInput("Metallic", PinType::Float);
        addInput("Roughness", PinType::Float);
        addInput("EmissionColor", PinType::Vector4);
        addInput("EmissionStrength", PinType::Float);
        addInput("Alpha", PinType::Float);
        addInput("Normal", PinType::Vector3);
        // Occlusion has no Blender Principled counterpart; phase-1 extension
        // that feeds the renderer's AO term.
        addInput("Occlusion", PinType::Float);
        addOutput("BSDF", PinType::Shader);
    }

    MixShaderNode::MixShaderNode(const std::string& nodeId) {
        id = nodeId;
        name = "Mix Shader";
        addInput("Fac", PinType::Float);
        addInput("A", PinType::Shader);
        addInput("B", PinType::Shader);
        addOutput("Shader", PinType::Shader);
    }

    MaterialOutputNode::MaterialOutputNode(const std::string& nodeId) {
        id = nodeId;
        name = "Material Output";
        addInput("Surface", PinType::Shader);
        addInput("Displacement", PinType::Vector3);
    }

    NormalMapNode::NormalMapNode(const std::string& nodeId) {
        id = nodeId;
        name = "Normal Map";
        addInput("Strength", PinType::Float);
        addInput("Color", PinType::Vector4);
        addOutput("Normal", PinType::Vector3);
    }

    BumpNode::BumpNode(const std::string& nodeId) {
        id = nodeId;
        name = "Bump";
        addInput("Strength", PinType::Float);
        addInput("Distance", PinType::Float);
        addInput("Height", PinType::Float);
        addInput("Normal", PinType::Vector3);
        addOutput("Normal", PinType::Vector3);
    }

    InvertNode::InvertNode(const std::string& nodeId) {
        id = nodeId;
        name = "Invert";
        addInput("Fac", PinType::Float);
        addInput("Color", PinType::Vector4);
        addOutput("Color", PinType::Vector4);
    }

    GammaNode::GammaNode(const std::string& nodeId) {
        id = nodeId;
        name = "Gamma";
        addInput("Gamma", PinType::Float);
        addInput("Color", PinType::Vector4);
        addOutput("Color", PinType::Vector4);
    }

    HueSatValNode::HueSatValNode(const std::string& nodeId) {
        id = nodeId;
        name = "Hue Saturation Value";
        addInput("Hue", PinType::Float);
        addInput("Saturation", PinType::Float);
        addInput("Value", PinType::Float);
        addInput("Fac", PinType::Float);
        addInput("Color", PinType::Vector4);
        addOutput("Color", PinType::Vector4);
    }

    LuminanceNode::LuminanceNode(const std::string& nodeId) {
        id = nodeId;
        name = "Luminance";
        addInput("Color", PinType::Vector4);
        addOutput("Val", PinType::Float);
    }

    MapRangeNode::MapRangeNode(const std::string& nodeId) {
        id = nodeId;
        name = "Map Range";
        addInput("Value", PinType::Float);
        addInput("FromMin", PinType::Float);
        addInput("FromMax", PinType::Float);
        addInput("ToMin", PinType::Float);
        addInput("ToMax", PinType::Float);
        addInput("Steps", PinType::Float);
        addOutput("Result", PinType::Float);
    }

    ClampNode::ClampNode(const std::string& nodeId) {
        id = nodeId;
        name = "Clamp";
        addInput("Value", PinType::Float);
        addInput("Min", PinType::Float);
        addInput("Max", PinType::Float);
        addOutput("Result", PinType::Float);
    }
    
    std::string AddNode::generateHLSL() const {
        return std::format("float4 {}_out = {}A + {}B;\n", id, id, id);
    }

    // Legacy single-function path does not understand BSDF bundles; these
    // nodes only participate in compileMaterialGraph().
    std::string ValueNode::generateHLSL() const {
        return std::format("// Value node {} (material-graph only)\n", id);
    }

    std::string RGBNode::generateHLSL() const {
        return std::format("// RGB node {} (material-graph only)\n", id);
    }

    std::string TexCoordNode::generateHLSL() const {
        return std::format("// TexCoord node {} (material-graph only)\n", id);
    }

    std::string GeometryNode::generateHLSL() const {
        return std::format("// Geometry node {} (material-graph only)\n", id);
    }

    std::string FresnelNode::generateHLSL() const {
        return std::format("// Fresnel node {} (material-graph only)\n", id);
    }

    std::string LayerWeightNode::generateHLSL() const {
        return std::format("// Layer Weight node {} (material-graph only)\n", id);
    }

    std::string BlackbodyNode::generateHLSL() const {
        return std::format("// Blackbody node {} (material-graph only)\n", id);
    }

    std::string ImageTextureNode::generateHLSL() const {
        return std::format("// Image Texture node {} (material-graph only)\n", id);
    }

    std::string NoiseTextureNode::generateHLSL() const {
        return std::format("// Noise Texture node {} (material-graph only)\n", id);
    }

    std::string VoronoiTextureNode::generateHLSL() const {
        return std::format("// Voronoi Texture node {} (material-graph only)\n", id);
    }

    std::string MixRGBNode::generateHLSL() const {
        return std::format("// Mix node {} (material-graph only)\n", id);
    }

    std::string ColorRampNode::generateHLSL() const {
        return std::format("// Color Ramp node {} (material-graph only)\n", id);
    }

    std::string MathNode::generateHLSL() const {
        return std::format("// Math node {} (material-graph only)\n", id);
    }

    std::string PrincipledBSDFNode::generateHLSL() const {
        return std::format("// Principled BSDF node {} (material-graph only)\n", id);
    }

    std::string MixShaderNode::generateHLSL() const {
        return std::format("// Mix Shader node {} (material-graph only)\n", id);
    }

    std::string MaterialOutputNode::generateHLSL() const {
        return std::format("// Material Output node {} (material-graph only)\n", id);
    }

    std::string NormalMapNode::generateHLSL() const {
        return std::format("// Normal Map node {} (material-graph only)\n", id);
    }

    std::string BumpNode::generateHLSL() const {
        return std::format("// Bump node {} (material-graph only)\n", id);
    }

    std::string InvertNode::generateHLSL() const {
        return std::format("// Invert node {} (material-graph only)\n", id);
    }

    std::string GammaNode::generateHLSL() const {
        return std::format("// Gamma node {} (material-graph only)\n", id);
    }

    std::string HueSatValNode::generateHLSL() const {
        return std::format("// Hue Saturation Value node {} (material-graph only)\n", id);
    }

    std::string LuminanceNode::generateHLSL() const {
        return std::format("// Luminance node {} (material-graph only)\n", id);
    }

    std::string MapRangeNode::generateHLSL() const {
        return std::format("// Map Range node {} (material-graph only)\n", id);
    }

    std::string ClampNode::generateHLSL() const {
        return std::format("// Clamp node {} (material-graph only)\n", id);
    }

    ShaderNodeBase* NodeGraph::addNode(std::unique_ptr<ShaderNodeBase> node) {
        nodes.push_back(std::move(node));
        return nodes.back().get();
    }

    void NodeGraph::link(Pin* from, Pin* to) {
        if (!from || !to || from->isInput || !to->isInput) return;
        auto nwLink = std::make_unique<Link>();
        nwLink->fromPin = from;
        nwLink->toPin = to;
        links.push_back(std::move(nwLink));
    }

    Pin* NodeGraph::getConnectedInput(Pin* toPin) const {
        for (const auto& l : links) {
            if (l->toPin == toPin) return l->fromPin;
        }
        return nullptr;
    }

    bool NodeGraph::unlink(Pin* from, Pin* to) {
        if (!from || !to) {
            return false;
        }
        for (auto it = links.begin(); it != links.end(); ++it) {
            if ((*it)->fromPin == from && (*it)->toPin == to) {
                links.erase(it);
                return true;
            }
        }
        return false;
    }

    void NodeGraph::disconnectInput(Pin* toPin) {
        if (!toPin) {
            return;
        }
        for (auto it = links.begin(); it != links.end();) {
            if ((*it)->toPin == toPin) {
                it = links.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool NodeGraph::removeNode(ShaderNodeBase* node) {
        if (!node) {
            return false;
        }
        for (auto it = links.begin(); it != links.end();) {
            const Pin* from = (*it)->fromPin;
            const Pin* to = (*it)->toPin;
            const bool touches =
                (from && from->owner == node) || (to && to->owner == node);
            if (touches) {
                it = links.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            if (it->get() == node) {
                nodes.erase(it);
                return true;
            }
        }
        return false;
    }

    std::vector<ShaderNodeBase*> NodeGraph::getTopologicalOrder() const {
        // Simplified for prototype: just return nodes in added order (assuming user adds them correctly)
        // In real impl, we walk from OutputNode up the tree
        std::vector<ShaderNodeBase*> sorted;
        for (const auto& n : nodes) sorted.push_back(n.get());
        return sorted;
    }

    std::string NodeGraph::compileHLSL() const {
        ArtifactCore::ZeroString hlslCode = "// ShaderNode Auto-Generated HLSL\n\n";
        hlslCode += "float4 main_shader() {\n";
        
        auto sortedNodes = getTopologicalOrder();
        for (auto* node : sortedNodes) {
            // For inputs, resolve their variables before generating the node code
            for (auto& inPin : node->inputs) {
                Pin* source = getConnectedInput(inPin.get());
                ArtifactCore::ZeroString varName = std::format("{}{}", node->id, inPin->name);
                ArtifactCore::ZeroString valCode = source ? std::format("{}_out", source->owner->id) : inPin->getDefaultValueString();
                hlslCode += std::format("    float4 {} = {};\n", std::string_view(varName), std::string_view(valCode));
            }
            // Generate node's own code
            ArtifactCore::ZeroString nodeCode = node->generateHLSL();
            // Indent lines without materializing a separate stream buffer.
            std::string_view nodeView = nodeCode;
            size_t start = 0;
            while (start < nodeView.size()) {
                const size_t end = nodeView.find('\n', start);
                const size_t lineEnd = (end == std::string_view::npos) ? nodeView.size() : end;
                hlslCode += "    ";
                hlslCode.append(nodeView.data() + start, lineEnd - start);
                hlslCode += "\n";
                if (end == std::string_view::npos) {
                    break;
                }
                start = end + 1;
            }
        }
        hlslCode += "}\n";
        return std::string(hlslCode.data(), hlslCode.length());
    }

    namespace {

    std::string matFloatLit(float v) {
        if (!std::isfinite(v)) {
            v = 0.0f;
        }
        return std::format("{}f", v);
    }

    std::string matFloat4Lit(float r, float g, float b, float a) {
        return std::format("float4({},{},{},{})",
                           matFloatLit(r), matFloatLit(g),
                           matFloatLit(b), matFloatLit(a));
    }

    // Blender-style implicit conversion between value sockets.
    std::string matConvert(const std::string& v, PinType from, PinType to,
                           const std::string& fallback) {
        if (from == to) {
            return v;
        }
        if (to == PinType::Float) {
            if (from == PinType::Vector2 || from == PinType::Vector3 ||
                from == PinType::Vector4) {
                return v + ".x";
            }
            return fallback;
        }
        if (from == PinType::Float) {
            if (to == PinType::Vector2) {
                return std::format("float2({0},{0})", v);
            }
            if (to == PinType::Vector3) {
                return std::format("float3({0},{0},{0})", v);
            }
            if (to == PinType::Vector4) {
                return std::format("float4({0},{0},{0},1.0f)", v);
            }
            return fallback;
        }
        if (from == PinType::Vector2 && to == PinType::Vector3) {
            return std::format("float3({0}.xy,0.0f)", v);
        }
        if (from == PinType::Vector2 && to == PinType::Vector4) {
            return std::format("float4({0}.xy,0.0f,1.0f)", v);
        }
        if (from == PinType::Vector3 && to == PinType::Vector2) {
            return v + ".xy";
        }
        if (from == PinType::Vector3 && to == PinType::Vector4) {
            return std::format("float4({0}.xyz,1.0f)", v);
        }
        if (from == PinType::Vector4 && to == PinType::Vector2) {
            return v + ".xy";
        }
        if (from == PinType::Vector4 && to == PinType::Vector3) {
            return v + ".xyz";
        }
        return fallback;
    }

    struct MatResolver {
        const NodeGraph* graph = nullptr;
        // (source node, source output pin) for a value input, or null.
        std::pair<const ShaderNodeBase*, const Pin*> sourceFor(
            const ShaderNodeBase* node, const std::string& inputName,
            PinType* outFromType = nullptr) const {
            for (const auto& inPin : node->inputs) {
                if (inPin->name != inputName) {
                    continue;
                }
                // links is a public member, so the resolver can scan it
                // directly without friendship.
                for (const auto& l : graph->links) {
                    if (l->toPin == inPin.get()) {
                        if (outFromType) {
                            *outFromType = l->fromPin->type;
                        }
                        return {l->fromPin->owner, l->fromPin};
                    }
                }
                return {nullptr, nullptr};
            }
            return {nullptr, nullptr};
        }
        std::string valueExpr(const ShaderNodeBase* node,
                              const std::string& pin, PinType toType,
                              const std::string& fallback) const {
            PinType fromType = toType;
            const auto src = sourceFor(node, pin, &fromType);
            if (!src.first || !src.second) {
                return fallback;
            }
            const std::string v = sanitizeShaderId(src.first->id) + "_" +
                                  src.second->name;
            return matConvert(v, fromType, toType, fallback);
        }
        // Bundle prefix for a Shader-typed input, or "" when unconnected
        // or linked from a non-Shader socket (Blender rejects those links;
        // here they fall back to defaults so output stays compilable).
        std::string bundlePrefix(const ShaderNodeBase* node,
                                 const std::string& pin) const {
            const auto src = sourceFor(node, pin);
            if (!src.first || !src.second) {
                return {};
            }
            if (src.second->type != PinType::Shader) {
                return {};
            }
            return sanitizeShaderId(src.first->id);
        }
    };

    void matEmitProceduralHelpers(std::string& out) {
        // Emitted once per block; node call sites reference these.
        // artMatGraph_ prefix avoids collisions with MeshRenderer symbols.
        out += "    float artMatGraph_hash21(float2 p)\n";
        out += "    {\n";
        out += "        float3 p3 = frac(float3(p.xyx) * 0.1031f);\n";
        out += "        p3 += dot(p3, p3.yzx + 33.33f);\n";
        out += "        return frac((p3.x + p3.y) * p3.z);\n";
        out += "    }\n";
        out += "    float artMatGraph_vnoise(float2 p)\n";
        out += "    {\n";
        out += "        float2 i = floor(p);\n";
        out += "        float2 f = frac(p);\n";
        out += "        float2 u = f * f * (3.0f - 2.0f * f);\n";
        out += "        float a = artMatGraph_hash21(i);\n";
        out += "        float b = artMatGraph_hash21(i + float2(1.0f, 0.0f));\n";
        out += "        float c = artMatGraph_hash21(i + float2(0.0f, 1.0f));\n";
        out += "        float d = artMatGraph_hash21(i + float2(1.0f, 1.0f));\n";
        out += "        return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);\n";
        out += "    }\n";
        out += "    float artMatGraph_fbm4(float2 p)\n";
        out += "    {\n";
        out += "        float v = 0.0f;\n";
        out += "        float amp = 0.5f;\n";
        out += "        for (int k = 0; k < 4; ++k)\n";
        out += "        {\n";
        out += "            v += amp * artMatGraph_vnoise(p);\n";
        out += "            p = p * 2.03f + float2(17.3f, 9.1f);\n";
        out += "            amp *= 0.5f;\n";
        out += "        }\n";
        out += "        return v;\n";
        out += "    }\n";
        out += "    void artMatGraph_voronoi(float2 p, out float dist, out float3 cell)\n";
        out += "    {\n";
        out += "        float2 ip = floor(p);\n";
        out += "        float2 fp = frac(p);\n";
        out += "        dist = 8.0f;\n";
        out += "        float2 best = float2(0.0f, 0.0f);\n";
        out += "        for (int j = -1; j <= 1; ++j)\n";
        out += "        {\n";
        out += "            for (int i = -1; i <= 1; ++i)\n";
        out += "            {\n";
        out += "                float2 g = float2(float(i), float(j));\n";
        out += "                float2 o = float2(artMatGraph_hash21(ip + g), artMatGraph_hash21(ip + g + 19.19f));\n";
        out += "                float2 r = g + o - fp;\n";
        out += "                float d = dot(r, r);\n";
        out += "                if (d < dist) { dist = d; best = ip + g; }\n";
        out += "            }\n";
        out += "        }\n";
        out += "        dist = sqrt(dist);\n";
        out += "        cell = float3(artMatGraph_hash21(best), artMatGraph_hash21(best + 7.7f), artMatGraph_hash21(best + 3.1f));\n";
        out += "    }\n";
        out += "    float3 artMatGraph_rgb2hsv(float3 c)\n";
        out += "    {\n";
        out += "        float4 K = float4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);\n";
        out += "        float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));\n";
        out += "        float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));\n";
        out += "        float d = q.x - min(q.w, q.y);\n";
        out += "        float e = 1e-10f;\n";
        out += "        return float3(abs(q.z + (q.w - q.y) / (6.0f * d + e)), d / (q.x + e), q.x);\n";
        out += "    }\n";
        out += "    float3 artMatGraph_hsv2rgb(float3 c)\n";
        out += "    {\n";
        out += "        float4 K = float4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);\n";
        out += "        float3 p = abs(frac(c.xxx + K.xyz) * 6.0f - K.www);\n";
        out += "        return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);\n";
        out += "    }\n";
    }

    } // namespace

    namespace {

    std::string matNodeTypeName(const ShaderNodeBase* n) {
        if (dynamic_cast<const ValueNode*>(n)) return "Value";
        if (dynamic_cast<const RGBNode*>(n)) return "RGB";
        if (dynamic_cast<const TexCoordNode*>(n)) return "TexCoord";
        if (dynamic_cast<const ImageTextureNode*>(n)) return "ImageTexture";
        if (dynamic_cast<const NoiseTextureNode*>(n)) return "Noise";
        if (dynamic_cast<const VoronoiTextureNode*>(n)) return "Voronoi";
        if (dynamic_cast<const MathNode*>(n)) return "Math";
        if (dynamic_cast<const MixRGBNode*>(n)) return "MixRGB";
        if (dynamic_cast<const ColorRampNode*>(n)) return "ColorRamp";
        if (dynamic_cast<const PrincipledBSDFNode*>(n)) return "Principled";
        if (dynamic_cast<const MixShaderNode*>(n)) return "MixShader";
        if (dynamic_cast<const GeometryNode*>(n)) return "Geometry";
        if (dynamic_cast<const FresnelNode*>(n)) return "Fresnel";
        if (dynamic_cast<const LayerWeightNode*>(n)) return "LayerWeight";
        if (dynamic_cast<const BlackbodyNode*>(n)) return "Blackbody";
        if (dynamic_cast<const NormalMapNode*>(n)) return "NormalMap";
        if (dynamic_cast<const BumpNode*>(n)) return "Bump";
        if (dynamic_cast<const InvertNode*>(n)) return "Invert";
        if (dynamic_cast<const GammaNode*>(n)) return "Gamma";
        if (dynamic_cast<const HueSatValNode*>(n)) return "HueSatVal";
        if (dynamic_cast<const LuminanceNode*>(n)) return "Luminance";
        if (dynamic_cast<const MapRangeNode*>(n)) return "MapRange";
        if (dynamic_cast<const ClampNode*>(n)) return "Clamp";
        if (dynamic_cast<const MaterialOutputNode*>(n)) return "MaterialOutput";
        if (dynamic_cast<const ColorNode*>(n)) return "LegacyColor";
        if (dynamic_cast<const AddNode*>(n)) return "LegacyAdd";
        if (dynamic_cast<const OutputNode*>(n)) return "LegacyOutput";
        return {};
    }

    std::unique_ptr<ShaderNodeBase> matCreateNode(const std::string& type,
                                                  const std::string& id) {
        if (type == "Value") return std::make_unique<ValueNode>(id);
        if (type == "RGB") return std::make_unique<RGBNode>(id);
        if (type == "TexCoord") return std::make_unique<TexCoordNode>(id);
        if (type == "ImageTexture") return std::make_unique<ImageTextureNode>(id);
        if (type == "Noise") return std::make_unique<NoiseTextureNode>(id);
        if (type == "Voronoi") return std::make_unique<VoronoiTextureNode>(id);
        if (type == "Math") return std::make_unique<MathNode>(id);
        if (type == "MixRGB") return std::make_unique<MixRGBNode>(id);
        if (type == "ColorRamp") return std::make_unique<ColorRampNode>(id);
        if (type == "Principled") return std::make_unique<PrincipledBSDFNode>(id);
        if (type == "MixShader") return std::make_unique<MixShaderNode>(id);
        if (type == "Geometry") return std::make_unique<GeometryNode>(id);
        if (type == "Fresnel") return std::make_unique<FresnelNode>(id);
        if (type == "LayerWeight") return std::make_unique<LayerWeightNode>(id);
        if (type == "Blackbody") return std::make_unique<BlackbodyNode>(id);
        if (type == "NormalMap") return std::make_unique<NormalMapNode>(id);
        if (type == "Bump") return std::make_unique<BumpNode>(id);
        if (type == "Invert") return std::make_unique<InvertNode>(id);
        if (type == "Gamma") return std::make_unique<GammaNode>(id);
        if (type == "HueSatVal") return std::make_unique<HueSatValNode>(id);
        if (type == "Luminance") return std::make_unique<LuminanceNode>(id);
        if (type == "MapRange") return std::make_unique<MapRangeNode>(id);
        if (type == "Clamp") return std::make_unique<ClampNode>(id);
        if (type == "MaterialOutput") return std::make_unique<MaterialOutputNode>(id);
        if (type == "LegacyColor") return std::make_unique<ColorNode>(id);
        if (type == "LegacyAdd") return std::make_unique<AddNode>(id);
        if (type == "LegacyOutput") return std::make_unique<OutputNode>(id);
        return nullptr;
    }

    double matFiniteDouble(const QJsonObject& o, const char* key, double fallback) {
        const double v = o.value(QLatin1String(key)).toDouble(fallback);
        return std::isfinite(v) ? v : fallback;
    }

    void matWriteParams(const ShaderNodeBase* n, QJsonObject& p) {
        if (const auto* v = dynamic_cast<const ValueNode*>(n)) {
            p[QStringLiteral("value")] = static_cast<double>(v->value);
        } else if (const auto* v = dynamic_cast<const RGBNode*>(n)) {
            p[QStringLiteral("r")] = static_cast<double>(v->r);
            p[QStringLiteral("g")] = static_cast<double>(v->g);
            p[QStringLiteral("b")] = static_cast<double>(v->b);
            p[QStringLiteral("a")] = static_cast<double>(v->a);
        } else if (const auto* v = dynamic_cast<const MathNode*>(n)) {
            p[QStringLiteral("op")] = static_cast<int>(v->op);
            p[QStringLiteral("clamp")] = v->clampResult;
        } else if (const auto* v = dynamic_cast<const MixRGBNode*>(n)) {
            p[QStringLiteral("blend")] = static_cast<int>(v->blend);
            p[QStringLiteral("clampFactor")] = v->clampFactor;
            p[QStringLiteral("clampResult")] = v->clampResult;
        } else if (const auto* v = dynamic_cast<const ColorRampNode*>(n)) {
            p[QStringLiteral("interp")] = static_cast<int>(v->interp);
            QJsonArray stops;
            for (const auto& s : v->stops) {
                QJsonObject stop;
                stop[QStringLiteral("pos")] = static_cast<double>(s.pos);
                stop[QStringLiteral("r")] = static_cast<double>(s.r);
                stop[QStringLiteral("g")] = static_cast<double>(s.g);
                stop[QStringLiteral("b")] = static_cast<double>(s.b);
                stop[QStringLiteral("a")] = static_cast<double>(s.a);
                stops.push_back(stop);
            }
            p[QStringLiteral("stops")] = stops;
        } else if (const auto* v = dynamic_cast<const ImageTextureNode*>(n)) {
            p[QStringLiteral("slot")] = static_cast<int>(v->slot);
        } else if (const auto* v = dynamic_cast<const MapRangeNode*>(n)) {
            p[QStringLiteral("interp")] = static_cast<int>(v->interp);
            p[QStringLiteral("clamp")] = v->clampRange;
        } else if (const auto* v = dynamic_cast<const ColorNode*>(n)) {
            p[QStringLiteral("r")] = static_cast<double>(v->r);
            p[QStringLiteral("g")] = static_cast<double>(v->g);
            p[QStringLiteral("b")] = static_cast<double>(v->b);
            p[QStringLiteral("a")] = static_cast<double>(v->a);
        }
    }

    void matReadParams(ShaderNodeBase* n, const QJsonObject& p) {
        if (auto* v = dynamic_cast<ValueNode*>(n)) {
            v->value = static_cast<float>(matFiniteDouble(p, "value", v->value));
        } else if (auto* v = dynamic_cast<RGBNode*>(n)) {
            v->r = static_cast<float>(matFiniteDouble(p, "r", v->r));
            v->g = static_cast<float>(matFiniteDouble(p, "g", v->g));
            v->b = static_cast<float>(matFiniteDouble(p, "b", v->b));
            v->a = static_cast<float>(matFiniteDouble(p, "a", v->a));
        } else if (auto* v = dynamic_cast<MathNode*>(n)) {
            const int op = std::clamp(p.value(QStringLiteral("op")).toInt(0), 0, 16);
            v->op = static_cast<MathNode::Op>(op);
            v->clampResult = p.value(QStringLiteral("clamp")).toBool(v->clampResult);
        } else if (auto* v = dynamic_cast<MixRGBNode*>(n)) {
            const int blend = std::clamp(p.value(QStringLiteral("blend")).toInt(0), 0, 7);
            v->blend = static_cast<MixRGBNode::Blend>(blend);
            v->clampFactor = p.value(QStringLiteral("clampFactor")).toBool(v->clampFactor);
            v->clampResult = p.value(QStringLiteral("clampResult")).toBool(v->clampResult);
        } else if (auto* v = dynamic_cast<ColorRampNode*>(n)) {
            const int interp = std::clamp(p.value(QStringLiteral("interp")).toInt(0), 0, 2);
            v->interp = static_cast<ColorRampNode::Interp>(interp);
            const QJsonArray stops = p.value(QStringLiteral("stops")).toArray();
            if (!stops.isEmpty()) {
                v->stops.clear();
                for (const QJsonValue& entry : stops) {
                    const QJsonObject stop = entry.toObject();
                    ColorRampNode::Stop s;
                    s.pos = static_cast<float>(matFiniteDouble(stop, "pos", 0.0));
                    s.r = static_cast<float>(matFiniteDouble(stop, "r", 0.0));
                    s.g = static_cast<float>(matFiniteDouble(stop, "g", 0.0));
                    s.b = static_cast<float>(matFiniteDouble(stop, "b", 0.0));
                    s.a = static_cast<float>(matFiniteDouble(stop, "a", 1.0));
                    v->stops.push_back(s);
                    if (v->stops.size() >= 64) {
                        break;
                    }
                }
                if (v->stops.empty()) {
                    v->stops.push_back(ColorRampNode::Stop{0.0f, 0.0f, 0.0f, 0.0f, 1.0f});
                    v->stops.push_back(ColorRampNode::Stop{1.0f, 1.0f, 1.0f, 1.0f, 1.0f});
                }
            }
        } else if (auto* v = dynamic_cast<ImageTextureNode*>(n)) {
            const int slot = std::clamp(p.value(QStringLiteral("slot")).toInt(0), 0, 5);
            v->slot = static_cast<ImageTextureNode::Slot>(slot);
        } else if (auto* v = dynamic_cast<MapRangeNode*>(n)) {
            const int interp = std::clamp(p.value(QStringLiteral("interp")).toInt(0), 0, 3);
            v->interp = static_cast<MapRangeNode::Interp>(interp);
            v->clampRange = p.value(QStringLiteral("clamp")).toBool(v->clampRange);
        } else if (auto* v = dynamic_cast<ColorNode*>(n)) {
            v->r = static_cast<float>(matFiniteDouble(p, "r", v->r));
            v->g = static_cast<float>(matFiniteDouble(p, "g", v->g));
            v->b = static_cast<float>(matFiniteDouble(p, "b", v->b));
            v->a = static_cast<float>(matFiniteDouble(p, "a", v->a));
        }
    }

    Pin* matFindPin(const std::vector<std::unique_ptr<Pin>>& pins, const std::string& name) {
        for (const auto& pin : pins) {
            if (pin && pin->name == name) {
                return pin.get();
            }
        }
        return nullptr;
    }

    } // namespace

    std::string NodeGraph::toJson(const std::vector<MaterialGraphLayout>& layout) const {
        QJsonObject root;
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("kind")] = QStringLiteral("artifact-material-graph");
        QJsonArray nodeArray;
        for (const auto& node : nodes) {
            if (!node) {
                continue;
            }
            const std::string type = matNodeTypeName(node.get());
            if (type.empty()) {
                continue;
            }
            QJsonObject entry;
            entry[QStringLiteral("id")] = QString::fromStdString(node->id);
            entry[QStringLiteral("type")] = QString::fromStdString(type);
            for (const auto& placed : layout) {
                if (placed.nodeId == node->id) {
                    entry[QStringLiteral("x")] = static_cast<double>(placed.x);
                    entry[QStringLiteral("y")] = static_cast<double>(placed.y);
                    break;
                }
            }
            QJsonObject params;
            matWriteParams(node.get(), params);
            entry[QStringLiteral("params")] = params;
            nodeArray.push_back(entry);
        }
        root[QStringLiteral("nodes")] = nodeArray;
        QJsonArray linkArray;
        for (const auto& link : links) {
            if (!link || !link->fromPin || !link->toPin || !link->fromPin->owner ||
                !link->toPin->owner) {
                continue;
            }
            QJsonObject entry;
            entry[QStringLiteral("from")] = QString::fromStdString(link->fromPin->owner->id);
            entry[QStringLiteral("out")] = QString::fromStdString(link->fromPin->name);
            entry[QStringLiteral("to")] = QString::fromStdString(link->toPin->owner->id);
            entry[QStringLiteral("in")] = QString::fromStdString(link->toPin->name);
            linkArray.push_back(entry);
        }
        root[QStringLiteral("links")] = linkArray;
        const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
        return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
    }

    MaterialGraphLoadResult NodeGraph::fromJson(const std::string& json) {
        MaterialGraphLoadResult result;
        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(
            QByteArray(json.data(), static_cast<qsizetype>(json.size())), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            result.error = std::string("JSON parse error: ") + parseError.errorString().toStdString();
            return result;
        }
        const QJsonObject root = document.object();
        if (root.value(QStringLiteral("version")).toInt(-1) != 1) {
            result.error = "unsupported material graph version";
            return result;
        }
        NodeGraph staged;
        std::vector<std::pair<std::string, ShaderNodeBase*>> index;
        const QJsonArray nodeArray = root.value(QStringLiteral("nodes")).toArray();
        for (const QJsonValue& entryValue : nodeArray) {
            const QJsonObject entry = entryValue.toObject();
            const std::string id = entry.value(QStringLiteral("id")).toString().toStdString();
            const std::string type = entry.value(QStringLiteral("type")).toString().toStdString();
            if (id.empty()) {
                result.error = "node entry without id";
                return result;
            }
            bool duplicate = false;
            for (const auto& known : index) {
                if (known.first == id) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                result.error = "duplicate node id: " + id;
                return result;
            }
            auto created = matCreateNode(type, id);
            if (!created) {
                result.error = "unknown node type: " + type;
                return result;
            }
            matReadParams(created.get(), entry.value(QStringLiteral("params")).toObject());
            double x = 0.0;
            double y = 0.0;
            bool hasPos = false;
            if (entry.contains(QStringLiteral("x")) && entry.contains(QStringLiteral("y"))) {
                x = entry.value(QStringLiteral("x")).toDouble(0.0);
                y = entry.value(QStringLiteral("y")).toDouble(0.0);
                hasPos = std::isfinite(x) && std::isfinite(y);
            }
            ShaderNodeBase* raw = staged.addNode(std::move(created));
            index.emplace_back(id, raw);
            if (hasPos) {
                result.layout.push_back(MaterialGraphLayout{
                    id, static_cast<float>(x), static_cast<float>(y)});
            }
        }
        int skippedLinks = 0;
        const QJsonArray linkArray = root.value(QStringLiteral("links")).toArray();
        for (const QJsonValue& entryValue : linkArray) {
            const QJsonObject entry = entryValue.toObject();
            const std::string fromId = entry.value(QStringLiteral("from")).toString().toStdString();
            const std::string outName = entry.value(QStringLiteral("out")).toString().toStdString();
            const std::string toId = entry.value(QStringLiteral("to")).toString().toStdString();
            const std::string inName = entry.value(QStringLiteral("in")).toString().toStdString();
            ShaderNodeBase* fromNode = nullptr;
            ShaderNodeBase* toNode = nullptr;
            for (const auto& known : index) {
                if (known.first == fromId) {
                    fromNode = known.second;
                }
                if (known.first == toId) {
                    toNode = known.second;
                }
            }
            Pin* fromPin = fromNode ? matFindPin(fromNode->outputs, outName) : nullptr;
            Pin* toPin = toNode ? matFindPin(toNode->inputs, inName) : nullptr;
            if (!fromPin || !toPin || fromPin->type != toPin->type) {
                ++skippedLinks;
                continue;
            }
            staged.link(fromPin, toPin);
        }
        nodes = std::move(staged.nodes);
        links = std::move(staged.links);
        result.ok = true;
        if (skippedLinks > 0) {
            result.error = "warning: skipped " + std::to_string(skippedLinks) + " link(s)";
        }
        return result;
    }

    MaterialGraphResult NodeGraph::compileMaterialGraph() const {
        MaterialGraphResult result;
        const auto fail = [&result](const std::string& msg) {
            result.ok = false;
            result.error = msg;
            result.helperHlsl.clear();
            result.blockHlsl =
                "    // Material graph invalid: " + msg + "\n"
                "    float4 graphBaseColor = float4(1.0f,0.0f,1.0f,1.0f);\n"
                "    float graphMetallic = 0.0f;\n"
                "    float graphRoughness = 0.5f;\n"
                "    float3 graphEmission = float3(0.0f,0.0f,0.0f);\n"
                "    float graphAlpha = 1.0f;\n"
                "    float3 graphNormalSample = normalSample;\n"
                "    float graphOcclusion = occlusionSample;\n"
                "    float3 graphWorldNormal = float3(0.0f,0.0f,1.0f);\n"
                "    float graphUseWorldNormal = 0.0f;\n";
            uint64_t h = 14695981039346656037ULL;
            for (unsigned char c : result.blockHlsl) {
                h ^= c;
                h *= 1099511628211ULL;
            }
            result.hashHex = std::format("{:016x}", h);
            return result;
        };

        std::vector<MaterialOutputNode*> outputs;
        for (auto& n : nodes) {
            if (auto* out = dynamic_cast<MaterialOutputNode*>(n.get())) {
                outputs.push_back(out);
            }
        }
        if (outputs.empty()) {
            return fail("no Material Output node");
        }
        MaterialOutputNode* root = outputs.front();

        // Depth-first topological order from the output over all links.
        std::vector<ShaderNodeBase*> order;
        std::vector<ShaderNodeBase*> tempStack;
        std::vector<ShaderNodeBase*> done;
        bool hasCycle = false;
        const auto contains = [](const std::vector<ShaderNodeBase*>& v,
                                 const ShaderNodeBase* n) {
            for (const auto* e : v) {
                if (e == n) {
                    return true;
                }
            }
            return false;
        };
        std::function<void(ShaderNodeBase*)> visit =
            [&](ShaderNodeBase* n) {
                if (!n || contains(done, n) || hasCycle) {
                    return;
                }
                if (contains(tempStack, n)) {
                    hasCycle = true;
                    return;
                }
                tempStack.push_back(n);
                for (const auto& inPin : n->inputs) {
                    const Pin* src = getConnectedInput(inPin.get());
                    if (src && src->owner) {
                        visit(src->owner);
                    }
                }
                tempStack.pop_back();
                done.push_back(n);
                order.push_back(n);
            };
        visit(root);
        if (hasCycle) {
            return fail("cycle detected");
        }

        MatResolver resolver{this};
        std::string block = "    // Material graph auto-generated block\n";
        // World-normal override channel (Bump via Displacement). Declared up
        // front so any node can set it; the renderer epilogue applies it.
        block += "    float3 graphWorldNormal = float3(0.0f,0.0f,1.0f);\n";
        block += "    float graphUseWorldNormal = 0.0f;\n";
        for (const ShaderNodeBase* node : order) {
            const std::string sid = sanitizeShaderId(node->id);
            if (const auto* n = dynamic_cast<const ValueNode*>(node)) {
                block += std::format("    float {0}_Value = {1};\n", sid,
                                     matFloatLit(n->value));
            } else if (const auto* n = dynamic_cast<const RGBNode*>(node)) {
                block += std::format("    float4 {0}_Color = {1};\n", sid,
                                     matFloat4Lit(n->r, n->g, n->b, n->a));
            } else if (dynamic_cast<const TexCoordNode*>(node)) {
                block += std::format("    float2 {0}_UV = In.UV;\n", sid);
            } else if (dynamic_cast<const GeometryNode*>(node)) {
                block += std::format("    float3 {0}_Position = In.WorldPosition;\n", sid);
                block += std::format("    float3 {0}_Normal = normalize(In.WorldNormal);\n", sid);
                block += std::format("    float3 {0}_Incoming = normalize(In.WorldPosition - SceneCameraPosition.xyz);\n", sid);
            } else if (dynamic_cast<const FresnelNode*>(node)) {
                const std::string ior = resolver.valueExpr(
                    node, "IOR", PinType::Float, "1.45f");
                const std::string nrm = resolver.valueExpr(
                    node, "Normal", PinType::Vector3,
                    "normalize(In.WorldNormal)");
                block += std::format("    float3 {0}_N = normalize({1});\n", sid, nrm);
                block += std::format("    float3 {0}_V = normalize(SceneCameraPosition.xyz - In.WorldPosition);\n", sid);
                block += std::format("    float {0}_ior = max({1}, 1.0f);\n", sid, ior);
                block += std::format("    float {0}_f0 = ({0}_ior - 1.0f) * ({0}_ior - 1.0f) / (({0}_ior + 1.0f) * ({0}_ior + 1.0f));\n", sid);
                block += std::format("    float {0}_Fac = saturate({0}_f0 + (1.0f - {0}_f0) * pow(1.0f - saturate(dot({0}_N, {0}_V)), 5.0f));\n", sid);
            } else if (dynamic_cast<const LayerWeightNode*>(node)) {
                const std::string blend = resolver.valueExpr(
                    node, "Blend", PinType::Float, "0.5f");
                const std::string nrm = resolver.valueExpr(
                    node, "Normal", PinType::Vector3,
                    "normalize(In.WorldNormal)");
                block += std::format("    float3 {0}_N = normalize({1});\n", sid, nrm);
                block += std::format("    float3 {0}_V = normalize(SceneCameraPosition.xyz - In.WorldPosition);\n", sid);
                block += std::format("    float {0}_ndv = saturate(dot({0}_N, {0}_V));\n", sid);
                block += std::format("    float {0}_Fresnel = saturate(0.033735f + (1.0f - 0.033735f) * pow(1.0f - {0}_ndv, 5.0f));\n", sid);
                block += std::format("    float {0}_exp = pow(2.0f, (0.5f - saturate({1})) * 4.0f);\n", sid, blend);
                block += std::format("    float {0}_Facing = saturate(pow({0}_ndv, {0}_exp));\n", sid);
            } else if (dynamic_cast<const BlackbodyNode*>(node)) {
                const std::string temp = resolver.valueExpr(
                    node, "Temperature", PinType::Float, "1500.0f");
                block += std::format("    float {0}_t = clamp(({1}) / 100.0f, 10.0f, 120.0f);\n", sid, temp);
                block += std::format("    float {0}_r = ({0}_t <= 66.0f) ? 1.0f : saturate(1.29294f * pow(({0}_t - 60.0f), -0.1332048f));\n", sid);
                block += std::format("    float {0}_g = ({0}_t <= 66.0f) ? saturate((0.3900816f * log({0}_t) - 0.6318414f)) : saturate(1.1298909f * pow(({0}_t - 60.0f), -0.07551485f));\n", sid);
                block += std::format("    float {0}_b = ({0}_t >= 66.0f) ? 1.0f : ((({0}_t <= 19.0f) ? 0.0f : saturate(0.5432068f * log({0}_t - 10.0f) - 1.1962541f)));\n", sid);
                block += std::format("    float4 {0}_Color = float4({0}_r, {0}_g, {0}_b, 1.0f);\n", sid);
            } else if (dynamic_cast<const NormalMapNode*>(node)) {
                const std::string strength = resolver.valueExpr(
                    node, "Strength", PinType::Float, "1.0f");
                const std::string color = resolver.valueExpr(
                    node, "Color", PinType::Vector4,
                    "float4(0.5f,0.5f,1.0f,1.0f)");
                block += std::format("    float3 {0}_nm = ({1}).rgb * 2.0f - 1.0f;\n", sid, color);
                block += std::format("    {0}_nm.xy *= {1};\n", sid, strength);
                block += std::format("    float3 {0}_Normal = {0}_nm * 0.5f + 0.5f;\n", sid);
            } else if (dynamic_cast<const BumpNode*>(node)) {
                const std::string strength = resolver.valueExpr(
                    node, "Strength", PinType::Float, "1.0f");
                const std::string dist = resolver.valueExpr(
                    node, "Distance", PinType::Float, "1.0f");
                const std::string height = resolver.valueExpr(
                    node, "Height", PinType::Float, "0.5f");
                const std::string nrm = resolver.valueExpr(
                    node, "Normal", PinType::Vector3,
                    "normalize(In.WorldNormal)");
                block += std::format("    float3 {0}_N = normalize({1});\n", sid, nrm);
                block += std::format("    float {0}_sx = ddx({1}) * ({2});\n", sid, height, strength);
                block += std::format("    float {0}_sy = ddy({1}) * ({2});\n", sid, height, strength);
                block += std::format("    float3 {0}_dpx = ddx(In.WorldPosition);\n", sid);
                block += std::format("    float3 {0}_dpy = ddy(In.WorldPosition);\n", sid);
                block += std::format("    float3 {0}_r1 = cross({0}_dpy, {0}_N);\n", sid);
                block += std::format("    float3 {0}_r2 = cross({0}_N, {0}_dpx);\n", sid);
                block += std::format("    float {0}_det = dot({0}_dpx, {0}_r1);\n", sid);
                block += std::format("    float3 {0}_grad = sign({0}_det) * ({0}_sx * {0}_r1 + {0}_sy * {0}_r2);\n", sid);
                block += std::format("    float3 {0}_Normal = normalize(abs({0}_det) * {0}_N - {0}_grad * ({1}));\n",
                                     sid, dist);
            } else if (dynamic_cast<const InvertNode*>(node)) {
                const std::string f = resolver.valueExpr(
                    node, "Fac", PinType::Float, "1.0f");
                const std::string c = resolver.valueExpr(
                    node, "Color", PinType::Vector4,
                    "float4(0.0f,0.0f,0.0f,1.0f)");
                block += std::format("    float {0}_f = saturate({1});\n", sid, f);
                block += std::format("    float4 {0}_Color = lerp(({1}), float4(1.0f,1.0f,1.0f,1.0f) - ({1}), {0}_f);\n",
                                     sid, c);
            } else if (dynamic_cast<const GammaNode*>(node)) {
                const std::string g = resolver.valueExpr(
                    node, "Gamma", PinType::Float, "1.0f");
                const std::string c = resolver.valueExpr(
                    node, "Color", PinType::Vector4,
                    "float4(0.5f,0.5f,0.5f,1.0f)");
                block += std::format("    float {0}_g = max(({1}), 1e-5f);\n", sid, g);
                block += std::format("    float3 {0}_gc = pow(max(({1}).rgb, float3(0.0f,0.0f,0.0f)), float3({0}_g, {0}_g, {0}_g));\n",
                                     sid, c);
                block += std::format("    float4 {0}_Color = float4({0}_gc, ({1}).a);\n", sid, c);
            } else if (dynamic_cast<const HueSatValNode*>(node)) {
                const std::string h = resolver.valueExpr(
                    node, "Hue", PinType::Float, "0.5f");
                const std::string s = resolver.valueExpr(
                    node, "Saturation", PinType::Float, "1.0f");
                const std::string v = resolver.valueExpr(
                    node, "Value", PinType::Float, "1.0f");
                const std::string f = resolver.valueExpr(
                    node, "Fac", PinType::Float, "1.0f");
                const std::string c = resolver.valueExpr(
                    node, "Color", PinType::Vector4,
                    "float4(0.8f,0.8f,0.8f,1.0f)");
                block += std::format("    float3 {0}_hsv = artMatGraph_rgb2hsv(saturate(({1}).rgb));\n", sid, c);
                block += std::format("    {0}_hsv.x = frac({0}_hsv.x + (({1}) - 0.5f));\n", sid, h);
                block += std::format("    {0}_hsv.y = saturate({0}_hsv.y * ({1}));\n", sid, s);
                block += std::format("    {0}_hsv.z = saturate({0}_hsv.z * ({1}));\n", sid, v);
                block += std::format("    float3 {0}_adj = artMatGraph_hsv2rgb(saturate({0}_hsv));\n", sid);
                block += std::format("    float4 {0}_Color = float4(lerp(({1}).rgb, {0}_adj, saturate({2})), ({1}).a);\n",
                                     sid, c, f);
            } else if (dynamic_cast<const LuminanceNode*>(node)) {
                const std::string c = resolver.valueExpr(
                    node, "Color", PinType::Vector4,
                    "float4(0.0f,0.0f,0.0f,1.0f)");
                block += std::format("    float {0}_Val = dot(({1}).rgb, float3(0.2126f, 0.7152f, 0.0722f));\n",
                                     sid, c);
            } else if (const auto* n = dynamic_cast<const MapRangeNode*>(node)) {
                const std::string v = resolver.valueExpr(
                    node, "Value", PinType::Float, "1.0f");
                const std::string fmin = resolver.valueExpr(
                    node, "FromMin", PinType::Float, "0.0f");
                const std::string fmax = resolver.valueExpr(
                    node, "FromMax", PinType::Float, "1.0f");
                const std::string tmin = resolver.valueExpr(
                    node, "ToMin", PinType::Float, "0.0f");
                const std::string tmax = resolver.valueExpr(
                    node, "ToMax", PinType::Float, "1.0f");
                const std::string steps = resolver.valueExpr(
                    node, "Steps", PinType::Float, "4.0f");
                block += std::format("    float {0}_span = ({1}) - ({2});\n", sid, fmax, fmin);
                block += std::format("    {0}_span = (abs({0}_span) < 1e-5f) ? 1.0f : {0}_span;\n", sid);
                block += std::format("    float {0}_t = (({1}) - ({2})) / {0}_span;\n", sid, v, fmin);
                std::string shaped;
                switch (n->interp) {
                    case MapRangeNode::Interp::Stepped:
                        shaped = std::format("(floor(saturate({0}_t) * max(floor({1}), 1.0f)) / max(floor({1}), 1.0f))", sid, steps);
                        break;
                    case MapRangeNode::Interp::SmoothStep:
                        shaped = std::format("(({0}_t) * ({0}_t) * (3.0f - 2.0f * ({0}_t)))", sid);
                        break;
                    case MapRangeNode::Interp::SmootherStep:
                        shaped = std::format("(({0}_t) * ({0}_t) * ({0}_t) * (({0}_t) * (({0}_t) * 6.0f - 15.0f) + 10.0f))", sid);
                        break;
                    case MapRangeNode::Interp::Linear:
                    default:
                        shaped = std::format("({0}_t)", sid);
                        break;
                }
                std::string mapped = std::format("(({0}) + ({1}) * (({2}) - ({0})))", tmin, shaped, tmax);
                if (n->clampRange) {
                    mapped = std::format("(clamp(({0}), min(({1}), ({2})), max(({1}), ({2}))))",
                                         mapped, tmin, tmax);
                }
                block += std::format("    float {0}_Result = {1};\n", sid, mapped);
            } else if (dynamic_cast<const ClampNode*>(node)) {
                const std::string v = resolver.valueExpr(
                    node, "Value", PinType::Float, "0.0f");
                const std::string mn = resolver.valueExpr(
                    node, "Min", PinType::Float, "0.0f");
                const std::string mx = resolver.valueExpr(
                    node, "Max", PinType::Float, "1.0f");
                block += std::format("    float {0}_Result = clamp(({1}), min(({2}), ({3})), max(({2}), ({3})));\n",
                                     sid, v, mn, mx);
            } else if (const auto* n = dynamic_cast<const ImageTextureNode*>(node)) {
                const std::string uv = resolver.valueExpr(
                    node, "UV", PinType::Vector2, "In.UV");
                std::string tex;
                switch (n->slot) {
                    case ImageTextureNode::Slot::Opacity:
                        tex = "g_OpacityTexture";
                        break;
                    case ImageTextureNode::Slot::Emission:
                        tex = "g_EmissionTexture";
                        break;
                    case ImageTextureNode::Slot::MetallicRoughness:
                        tex = "g_MetallicRoughnessTexture";
                        break;
                    case ImageTextureNode::Slot::Normal:
                        tex = "g_NormalTexture";
                        break;
                    case ImageTextureNode::Slot::Occlusion:
                        tex = "g_OcclusionTexture";
                        break;
                    case ImageTextureNode::Slot::BaseColor:
                    default:
                        tex = "g_BaseColorTexture";
                        break;
                }
                block += std::format("    float4 {0}_Color = {1}.Sample(g_BaseColorSampler, {2});\n",
                                     sid, tex, uv);
                block += std::format("    float {0}_Alpha = {0}_Color.a;\n", sid);
            } else if (dynamic_cast<const NoiseTextureNode*>(node)) {
                const std::string vec = resolver.valueExpr(
                    node, "Vector", PinType::Vector2, "In.UV");
                const std::string scale = resolver.valueExpr(
                    node, "Scale", PinType::Float, "5.0f");
                block += std::format("    float2 {0}_p = ({1}) * max({2}, 0.001f);\n",
                                     sid, vec, scale);
                block += std::format("    float {0}_Fac = artMatGraph_fbm4({0}_p);\n", sid);
                block += std::format("    float4 {0}_Color = float4({0}_Fac, {0}_Fac, {0}_Fac, 1.0f);\n", sid);
            } else if (dynamic_cast<const VoronoiTextureNode*>(node)) {
                const std::string vec = resolver.valueExpr(
                    node, "Vector", PinType::Vector2, "In.UV");
                const std::string scale = resolver.valueExpr(
                    node, "Scale", PinType::Float, "5.0f");
                block += std::format("    float2 {0}_p = ({1}) * max({2}, 0.001f);\n",
                                     sid, vec, scale);
                block += std::format("    float {0}_Distance = 0.0f;\n", sid);
                block += std::format("    float3 {0}_cell = float3(0.0f, 0.0f, 0.0f);\n", sid);
                block += std::format("    artMatGraph_voronoi({0}_p, {0}_Distance, {0}_cell);\n", sid);
                block += std::format("    float4 {0}_Color = float4({0}_cell, 1.0f);\n", sid);
            } else if (const auto* n = dynamic_cast<const MixRGBNode*>(node)) {
                const std::string f = resolver.valueExpr(
                    node, "Fac", PinType::Float, "0.5f");
                const std::string c1 = resolver.valueExpr(
                    node, "Color1", PinType::Vector4,
                    "float4(0.5f,0.5f,0.5f,1.0f)");
                const std::string c2 = resolver.valueExpr(
                    node, "Color2", PinType::Vector4,
                    "float4(1.0f,1.0f,1.0f,1.0f)");
                block += std::format("    float {0}_f = {1};\n", sid,
                                     n->clampFactor ? std::format("saturate({0})", f) : f);
                block += std::format("    float3 {0}_a = ({1}).rgb;\n", sid, c1);
                block += std::format("    float3 {0}_b = ({1}).rgb;\n", sid, c2);
                std::string blendRGB;
                switch (n->blend) {
                    case MixRGBNode::Blend::Add:
                        blendRGB = std::format("({0}_a + {0}_b)", sid);
                        break;
                    case MixRGBNode::Blend::Multiply:
                        blendRGB = std::format("({0}_a * {0}_b)", sid);
                        break;
                    case MixRGBNode::Blend::Screen:
                        blendRGB = std::format("(1.0f - (1.0f - {0}_a) * (1.0f - {0}_b))", sid);
                        break;
                    case MixRGBNode::Blend::Overlay:
                        blendRGB = std::format("(lerp(2.0f * {0}_a * {0}_b, 1.0f - 2.0f * (1.0f - {0}_a) * (1.0f - {0}_b), step(0.5f, {0}_a)))", sid);
                        break;
                    case MixRGBNode::Blend::Darken:
                        blendRGB = std::format("(min({0}_a, {0}_b))", sid);
                        break;
                    case MixRGBNode::Blend::Lighten:
                        blendRGB = std::format("(max({0}_a, {0}_b))", sid);
                        break;
                    case MixRGBNode::Blend::Difference:
                        blendRGB = std::format("(abs({0}_a - {0}_b))", sid);
                        break;
                    case MixRGBNode::Blend::Mix:
                    default:
                        blendRGB = std::format("{0}_b", sid);
                        break;
                }
                std::string mixed = std::format("lerp({0}_a, {1}, {0}_f)", sid, blendRGB);
                if (n->clampResult) {
                    mixed = std::format("saturate({0})", mixed);
                }
                block += std::format("    float3 {0}_rgb = {1};\n", sid, mixed);
                block += std::format("    float4 {0}_Color = float4({0}_rgb, lerp(({1}).a, ({2}).a, {0}_f));\n",
                                     sid, c1, c2);
            } else if (const auto* n = dynamic_cast<const ColorRampNode*>(node)) {
                const std::string f = resolver.valueExpr(
                    node, "Fac", PinType::Float, "0.5f");
                // Copy + sort + sanitize stops in C++ so HLSL stays branch-light.
                struct RampStop { float p, r, g, b, a; };
                std::vector<RampStop> ramp;
                ramp.reserve(n->stops.size());
                for (const auto& s : n->stops) {
                    float p = std::isfinite(s.pos) ? std::clamp(s.pos, 0.0f, 1.0f) : 0.0f;
                    ramp.push_back(RampStop{
                        p,
                        std::isfinite(s.r) ? s.r : 0.0f,
                        std::isfinite(s.g) ? s.g : 0.0f,
                        std::isfinite(s.b) ? s.b : 0.0f,
                        std::isfinite(s.a) ? s.a : 1.0f});
                }
                std::sort(ramp.begin(), ramp.end(),
                          [](const RampStop& l, const RampStop& r) { return l.p < r.p; });
                if (ramp.empty()) {
                    block += std::format("    float4 {0}_Color = float4(0.0f,0.0f,0.0f,1.0f);\n", sid);
                    block += std::format("    float {0}_Alpha = 1.0f;\n", sid);
                } else if (ramp.size() == 1) {
                    block += std::format("    float4 {0}_Color = {1};\n", sid,
                                         matFloat4Lit(ramp[0].r, ramp[0].g, ramp[0].b, ramp[0].a));
                    block += std::format("    float {0}_Alpha = {1};\n", sid,
                                         matFloatLit(ramp[0].a));
                } else {
                    block += std::format("    float {0}_t = saturate({1});\n", sid, f);
                    const auto& first = ramp.front();
                    block += std::format("    float4 {0}_ramp = {1};\n", sid,
                                         matFloat4Lit(first.r, first.g, first.b, first.a));
                    for (size_t si = 1; si < ramp.size(); ++si) {
                        const auto& p0 = ramp[si - 1];
                        const auto& p1 = ramp[si];
                        const float span = std::max(p1.p - p0.p, 1e-5f);
                        std::string t;
                        if (n->interp == ColorRampNode::Interp::Constant) {
                            t = std::format("step({0}, {1}_t)", matFloatLit(p1.p), sid);
                        } else {
                            t = std::format("saturate(({0}_t - {1}) / {2})",
                                            sid, matFloatLit(p0.p), matFloatLit(span));
                            if (n->interp == ColorRampNode::Interp::Ease) {
                                t = std::format("({0} * {0} * (3.0f - 2.0f * {0}))", t);
                            }
                        }
                        block += std::format("    {0}_ramp = lerp({0}_ramp, {1}, {2});\n",
                                             sid, matFloat4Lit(p1.r, p1.g, p1.b, p1.a), t);
                    }
                    block += std::format("    float4 {0}_Color = {0}_ramp;\n", sid);
                    block += std::format("    float {0}_Alpha = {0}_ramp.a;\n", sid);
                }
            } else if (const auto* n = dynamic_cast<const MathNode*>(node)) {
                const std::string a = resolver.valueExpr(
                    node, "A", PinType::Vector4,
                    "float4(0.5f,0.5f,0.5f,0.5f)");
                std::string expr;
                const auto twoOp = [&](const std::string& dfltB) {
                    return resolver.valueExpr(node, "B", PinType::Vector4, dfltB);
                };
                switch (n->op) {
                    case MathNode::Op::Add:
                        expr = std::format("({0} + {1})", a, twoOp("float4(0.0f,0.0f,0.0f,0.0f)"));
                        break;
                    case MathNode::Op::Subtract:
                        expr = std::format("({0} - {1})", a, twoOp("float4(0.0f,0.0f,0.0f,0.0f)"));
                        break;
                    case MathNode::Op::Multiply:
                        expr = std::format("({0} * {1})", a, twoOp("float4(1.0f,1.0f,1.0f,1.0f)"));
                        break;
                    case MathNode::Op::Divide:
                        expr = std::format("({0} / max({1}, float4(1e-5f,1e-5f,1e-5f,1e-5f)))", a,
                                           twoOp("float4(1.0f,1.0f,1.0f,1.0f)"));
                        break;
                    case MathNode::Op::Mix: {
                        const std::string f = resolver.valueExpr(
                            node, "Fac", PinType::Float, "0.5f");
                        expr = std::format("(lerp({0}, {1}, saturate({2})))", a,
                                           twoOp("float4(0.0f,0.0f,0.0f,0.0f)"), f);
                        break;
                    }
                    case MathNode::Op::Sine:
                        expr = std::format("(sin({0}))", a);
                        break;
                    case MathNode::Op::Cosine:
                        expr = std::format("(cos({0}))", a);
                        break;
                    case MathNode::Op::Power:
                        // pow() is undefined for negative bases; clamp like
                        // the fixed path does for roughness-style inputs.
                        expr = std::format("(pow(max(({0}), float4(0.0f,0.0f,0.0f,0.0f)), ({1})))",
                                           a, twoOp("float4(1.0f,1.0f,1.0f,1.0f)"));
                        break;
                    case MathNode::Op::Minimum:
                        expr = std::format("(min({0}, {1}))", a,
                                           twoOp("float4(0.0f,0.0f,0.0f,0.0f)"));
                        break;
                    case MathNode::Op::Maximum:
                        expr = std::format("(max({0}, {1}))", a,
                                           twoOp("float4(0.0f,0.0f,0.0f,0.0f)"));
                        break;
                    case MathNode::Op::LessThan: {
                        // HLSL ternary needs scalar conditions: stage temps.
                        const std::string b = twoOp("float4(0.0f,0.0f,0.0f,0.0f)");
                        block += std::format("    float4 {0}_la = {1};\n", sid, a);
                        block += std::format("    float4 {0}_lb = {1};\n", sid, b);
                        expr = std::format("float4({0}_la.x < {0}_lb.x ? 1.0f : 0.0f, {0}_la.y < {0}_lb.y ? 1.0f : 0.0f, {0}_la.z < {0}_lb.z ? 1.0f : 0.0f, {0}_la.w < {0}_lb.w ? 1.0f : 0.0f)", sid);
                        break;
                    }
                    case MathNode::Op::GreaterThan: {
                        const std::string b = twoOp("float4(0.0f,0.0f,0.0f,0.0f)");
                        block += std::format("    float4 {0}_la = {1};\n", sid, a);
                        block += std::format("    float4 {0}_lb = {1};\n", sid, b);
                        expr = std::format("float4({0}_la.x > {0}_lb.x ? 1.0f : 0.0f, {0}_la.y > {0}_lb.y ? 1.0f : 0.0f, {0}_la.z > {0}_lb.z ? 1.0f : 0.0f, {0}_la.w > {0}_lb.w ? 1.0f : 0.0f)", sid);
                        break;
                    }
                    case MathNode::Op::Modulo: {
                        // Blender floored modulo (not fmod): a - b*floor(a/b).
                        const std::string b = twoOp("float4(1.0f,1.0f,1.0f,1.0f)");
                        block += std::format("    float4 {0}_la = {1};\n", sid, a);
                        block += std::format("    float4 {0}_lb = max(({1}), float4(1e-5f,1e-5f,1e-5f,1e-5f));\n", sid, b);
                        expr = std::format("({0}_la - {0}_lb * floor({0}_la / {0}_lb))", sid);
                        break;
                    }
                    case MathNode::Op::Absolute:
                        expr = std::format("(abs({0}))", a);
                        break;
                    case MathNode::Op::Floor:
                        expr = std::format("(floor({0}))", a);
                        break;
                    case MathNode::Op::Ceil:
                        expr = std::format("(ceil({0}))", a);
                        break;
                    case MathNode::Op::Fract:
                        expr = std::format("(frac({0}))", a);
                        break;
                }
                if (n->clampResult) {
                    expr = std::format("saturate({0})", expr);
                }
                block += std::format("    float4 {0}_Value = {1};\n", sid, expr);
            } else if (const auto* n = dynamic_cast<const PrincipledBSDFNode*>(node)) {
                (void)n;
                const std::string base = resolver.valueExpr(
                    node, "BaseColor", PinType::Vector4,
                    "float4(0.8f,0.8f,0.8f,1.0f)");
                const std::string metal = resolver.valueExpr(
                    node, "Metallic", PinType::Float, "0.0f");
                const std::string rough = resolver.valueExpr(
                    node, "Roughness", PinType::Float, "0.5f");
                const std::string emis = resolver.valueExpr(
                    node, "EmissionColor", PinType::Vector4,
                    "float4(0.0f,0.0f,0.0f,1.0f)");
                const std::string emisStrength = resolver.valueExpr(
                    node, "EmissionStrength", PinType::Float, "1.0f");
                const std::string alpha = resolver.valueExpr(
                    node, "Alpha", PinType::Float, "1.0f");
                // Unconnected Normal/Occlusion fall back to the sampled
                // values so a graph without normal work behaves like the
                // fixed path instead of flattening the normal map.
                const std::string normal = resolver.valueExpr(
                    node, "Normal", PinType::Vector3, "normalSample");
                const std::string occ = resolver.valueExpr(
                    node, "Occlusion", PinType::Float, "occlusionSample");
                block += std::format("    float4 {0}_baseColor = {1};\n", sid, base);
                block += std::format("    float {0}_metallic = saturate({1});\n", sid, metal);
                block += std::format("    float {0}_roughness = saturate({1});\n", sid, rough);
                block += std::format("    float3 {0}_emission = ({1}).rgb * max({2}, 0.0f);\n",
                                     sid, emis, emisStrength);
                block += std::format("    float {0}_alpha = saturate({1});\n", sid, alpha);
                block += std::format("    float3 {0}_normal = {1};\n", sid, normal);
                block += std::format("    float {0}_occlusion = saturate({1});\n", sid, occ);
            } else if (dynamic_cast<const MixShaderNode*>(node)) {
                const std::string f = resolver.valueExpr(
                    node, "Fac", PinType::Float, "0.5f");
                const std::string a = resolver.bundlePrefix(node, "A");
                const std::string b = resolver.bundlePrefix(node, "B");
                const std::string aBase = a.empty() ? "float4(0.8f,0.8f,0.8f,1.0f)" : a + "_baseColor";
                const std::string bBase = b.empty() ? "float4(0.8f,0.8f,0.8f,1.0f)" : b + "_baseColor";
                const std::string aMet = a.empty() ? "0.0f" : a + "_metallic";
                const std::string bMet = b.empty() ? "0.0f" : b + "_metallic";
                const std::string aRgh = a.empty() ? "0.5f" : a + "_roughness";
                const std::string bRgh = b.empty() ? "0.5f" : b + "_roughness";
                const std::string aEmi = a.empty() ? "float3(0.0f,0.0f,0.0f)" : a + "_emission";
                const std::string bEmi = b.empty() ? "float3(0.0f,0.0f,0.0f)" : b + "_emission";
                const std::string aAlp = a.empty() ? "1.0f" : a + "_alpha";
                const std::string bAlp = b.empty() ? "1.0f" : b + "_alpha";
                const std::string aNrm = a.empty() ? "normalSample" : a + "_normal";
                const std::string bNrm = b.empty() ? "normalSample" : b + "_normal";
                const std::string aOcc = a.empty() ? "occlusionSample" : a + "_occlusion";
                const std::string bOcc = b.empty() ? "occlusionSample" : b + "_occlusion";
                block += std::format("    float {0}_fac = saturate({1});\n", sid, f);
                block += std::format("    float4 {0}_baseColor = lerp({1}, {2}, {0}_fac);\n", sid, aBase, bBase);
                block += std::format("    float {0}_metallic = lerp({1}, {2}, {0}_fac);\n", sid, aMet, bMet);
                block += std::format("    float {0}_roughness = lerp({1}, {2}, {0}_fac);\n", sid, aRgh, bRgh);
                block += std::format("    float3 {0}_emission = lerp({1}, {2}, {0}_fac);\n", sid, aEmi, bEmi);
                block += std::format("    float {0}_alpha = lerp({1}, {2}, {0}_fac);\n", sid, aAlp, bAlp);
                block += std::format("    float3 {0}_normal = lerp({1}, {2}, {0}_fac);\n", sid, aNrm, bNrm);
                block += std::format("    float {0}_occlusion = lerp({1}, {2}, {0}_fac);\n", sid, aOcc, bOcc);
            } else if (dynamic_cast<const MaterialOutputNode*>(node)) {
                const std::string surf = resolver.bundlePrefix(node, "Surface");
                if (surf.empty()) {
                    result.error = "warning: Surface unconnected, defaults used";
                    block += "    float4 graphBaseColor = float4(0.8f,0.8f,0.8f,1.0f);\n";
                    block += "    float graphMetallic = 0.0f;\n";
                    block += "    float graphRoughness = 0.5f;\n";
                    block += "    float3 graphEmission = float3(0.0f,0.0f,0.0f);\n";
                    block += "    float graphAlpha = 1.0f;\n";
                    block += "    float3 graphNormalSample = normalSample;\n";
                    block += "    float graphOcclusion = occlusionSample;\n";
                } else {
                    block += std::format("    float4 graphBaseColor = {0}_baseColor;\n", surf);
                    block += std::format("    float graphMetallic = {0}_metallic;\n", surf);
                    block += std::format("    float graphRoughness = {0}_roughness;\n", surf);
                    block += std::format("    float3 graphEmission = {0}_emission;\n", surf);
                    block += std::format("    float graphAlpha = {0}_alpha;\n", surf);
                    block += std::format("    float3 graphNormalSample = {0}_normal;\n", surf);
                    block += std::format("    float graphOcclusion = {0}_occlusion;\n", surf);
                }
                // Blender Displacement: world-space normal from a Bump node.
                // Unconnected (or non-vector) links leave the flag at 0.
                {
                    PinType dispFrom = PinType::Vector3;
                    const auto dispSrc = resolver.sourceFor(node, "Displacement", &dispFrom);
                    if (dispSrc.first && dispSrc.second &&
                        (dispFrom == PinType::Vector2 || dispFrom == PinType::Vector3 ||
                         dispFrom == PinType::Vector4 || dispFrom == PinType::Float)) {
                        const std::string v = sanitizeShaderId(dispSrc.first->id) + "_" +
                                              dispSrc.second->name;
                        const std::string disp = matConvert(v, dispFrom, PinType::Vector3,
                                                            "normalize(In.WorldNormal)");
                        block += std::format("    graphWorldNormal = normalize({0});\n", disp);
                        block += "    graphUseWorldNormal = 1.0f;\n";
                    }
                }
            } else if (dynamic_cast<const ColorNode*>(node) ||
                       dynamic_cast<const AddNode*>(node) ||
                       dynamic_cast<const OutputNode*>(node)) {
                return fail(std::string("unsupported node in material graph: ") + node->name);
            } else {
                return fail(std::string("unknown node in material graph: ") + node->name);
            }
        }

        result.ok = true;
        if (outputs.size() > 1 && result.error.empty()) {
            result.error = "warning: multiple Material Output nodes, using first";
        }
        matEmitProceduralHelpers(result.helperHlsl);
        result.blockHlsl = std::move(block);
        uint64_t h = 14695981039346656037ULL;
        for (unsigned char c : result.helperHlsl) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        for (unsigned char c : result.blockHlsl) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        result.hashHex = std::format("{:016x}", h);
        return result;
    }

} // namespace ShaderNode
} // namespace Artifact
