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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
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
        Texture2D
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

    class ShaderNodeBase {
    public:
        std::string id;
        std::string name;
        std::vector<std::unique_ptr<Pin>> inputs;
        std::vector<std::unique_ptr<Pin>> outputs;
        
        virtual ~ShaderNodeBase() = default;
        virtual std::string generateHLSL() const = 0;
        
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

    class NodeGraph {
    public:
        std::vector<std::unique_ptr<ShaderNodeBase>> nodes;
        std::vector<std::unique_ptr<Link>> links;
        
        ShaderNodeBase* addNode(std::unique_ptr<ShaderNodeBase> node);
        void link(Pin* from, Pin* to);
        
        // Generates the final concatenated HLSL text
        std::string compileHLSL() const;
    private:
        std::vector<ShaderNodeBase*> getTopologicalOrder() const;
        Pin* getConnectedInput(Pin* toPin) const;
    };

} // namespace ShaderNode
} // namespace Artifact
