module;
#include <utility>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <format>

module Artifact.ShaderNode.Core;

import Core.ArtifactString;

namespace Artifact {
namespace ShaderNode {

    std::string Pin::getDefaultValueString() const {
        switch(type) {
            case PinType::Float: return "0.0";
            case PinType::Vector2: return "float2(0.0, 0.0)";
            case PinType::Vector3: return "float3(0.0, 0.0, 0.0)";
            case PinType::Vector4: return "float4(0.0, 0.0, 0.0, 1.0)";
            default: return "0";
        }
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
    
    std::string AddNode::generateHLSL() const {
        return std::format("float4 {}_out = {}A + {}B;\n", id, id, id);
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

    std::vector<ShaderNodeBase*> NodeGraph::getTopologicalOrder() const {
        // Simplified for prototype: just return nodes in added order (assuming user adds them correctly)
        // In real impl, we walk from OutputNode up the tree
        std::vector<ShaderNodeBase*> sorted;
        for (const auto& n : nodes) sorted.push_back(n.get());
        return sorted;
    }

    std::string NodeGraph::compileHLSL() const {
        ZeroString hlslCode = "// ShaderNode Auto-Generated HLSL\n\n";
        hlslCode += "float4 main_shader() {\n";
        
        auto sortedNodes = getTopologicalOrder();
        for (auto* node : sortedNodes) {
            // For inputs, resolve their variables before generating the node code
            for (auto& inPin : node->inputs) {
                Pin* source = getConnectedInput(inPin.get());
                ZeroString varName = std::format("{}{}", node->id, inPin->name);
                ZeroString valCode = source ? std::format("{}_out", source->owner->id) : inPin->getDefaultValueString();
                hlslCode += std::format("    float4 {} = {};\n", std::string_view(varName), std::string_view(valCode));
            }
            // Generate node's own code
            ZeroString nodeCode = node->generateHLSL();
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
        return hlslCode;
    }

} // namespace ShaderNode
} // namespace Artifact
